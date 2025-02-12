#include "http_conn.h"

using namespace std;

string ok_200_title = "OK";
string error_400_title = "Bad Request";
string error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.";
string error_403_title = "Forbidden";
string error_403_form = "You do not have permission to get file form this server.";
string error_404_title = "Not Found";
string error_404_form = "The requested file was not found on this server.";
string error_500_title = "Internal Error";
string error_500_form = "There was an unusual problem serving the request file.";

locker m_mutex;
map<string, string> users;

void http_conn::init_mysql_result(connect_pool *connpool) {
    MYSQL *mysql = NULL;
    connectRAII conn(connpool, &mysql);

    if (mysql_query(mysql, "SELECT username, password FROM user")) {
        LOG_ERROR("sql error" + string(mysql_error(mysql)));
    }

    MYSQL_RES *res = mysql_store_result(mysql);
    int row_num = mysql_num_fields(res);
    MYSQL_FIELD *fields = mysql_fetch_field(res);

    while (MYSQL_ROW row = mysql_fetch_row(res)) {
        users[string(row[0])] = string(row[1]);
    }
}

int setNonBlocking(int fd) {
    int oldOption = fcntl(fd, F_GETFL);
    int newOption = oldOption | O_NONBLOCK;
    fcntl(fd, F_SETFL, newOption);
    return oldOption;
}

void addfd(int epollfd, int fd, bool one_shot, int TRIGMode) {
        epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP;
    if (TRIGMode == 1) {
        event.events |= EPOLLET;
    }
    if (one_shot) {
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setNonBlocking(fd);
}

void removefd(int epollfd, int fd) {
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 修改为oneshot
void modfd(int epollfd, int fd, int ev, int trigMode) {
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;

    if (trigMode == 1) {
        event.events |= EPOLLET;
    }

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int http_conn::m_user_cnt = 0;
int http_conn::m_epollfd = -1;

void http_conn::close_conn(bool real_close) {
    if (real_close && m_sockfd != -1) {
        LOG_INFO("close connection " + to_string(m_sockfd));
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_cnt--;
    }
}

void http_conn::init(int sockfd, const sockaddr_in &addr, string root, int trigMode, int close_log, string user, string passwd, string name) {
    m_sockfd = sockfd;
    m_address = addr;
    doc_root = root;
    m_TRIGMode = trigMode;
    m_close_log = close_log;
    sql_user = user;
    sql_passwd = passwd;
    sql_name = name;

    init();
}

// 初始化新接受的连接
void http_conn::init() {
    mysql = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;   // 分析请求行状态
    m_linger = false;
    m_method = GET;
    m_content_length = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    m_state = 0;
    timer_flag = 0;
    improv = 0;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
}

// 读取单行数据并进行操作
http_conn::HTTP_CODE http_conn::process_read() {
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE code = NO_REQUEST;
    string text;

    // 每解析完一部分都会更改chech state 从而解析下一部分
    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || (line_status = parse_line()) == LINE_OK) {
        text = get_line();
        m_start_line = m_checked_idx;
        LOG_INFO(text);

        switch (m_check_state) {
            case CHECK_STATE_REQUESTLINE: {
                code = parse_request_line(text);
                if (code == BAD_REQUEST) {
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER: {
                code = parse_headers(text);
                if (code == BAD_REQUEST) {
                    return code;
                } else if (code == GET_REQUEST) {
                    return do_request();
                    // return code;
                }
                break;
            }
            case CHECK_STATE_CONTENT: {
                code = parse_content(text);
                if (code == GET_REQUEST) {
                    return do_request();
                    // code = do_request();
                }
                line_status = LINE_OPEN;
                break;
            }
            default:
                return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

bool http_conn::process_write(HTTP_CODE code) {
    switch (code) {
        case INTERNAL_ERROR: {
            add_status_line(500, error_500_title);
            add_headers(error_500_form.length());
            if (!add_content(error_500_form)) {
                return false;
            }
            break;
        }
        case BAD_REQUEST: {
            add_status_line(404, error_404_title);
            add_headers(error_404_form.length());
            if (!add_content(error_404_form)) {
                return false;
            }
            break;
        }
        case FORBIDDEN_REQUEST: {
            add_status_line(403, error_403_title);
            add_headers(error_403_form.length());
            if (!add_content(error_403_form)) {
                return false;
            }
            break;
        }
        case FILE_REQUEST: {
            add_status_line(200, ok_200_title);
            if (m_file_stat.st_size != 0) {
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;
                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                bytes_to_send = m_write_idx + m_file_stat.st_size;
                return true;
            } else {
                string ok = "<html><body></body></html>";
                add_headers(ok.length());
                if (!add_content(ok)) {
                    return false;
                }
            }
        }
        default:
            return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

vector<string> split(string& text, char delimiter) {
    vector<string> strs;
    string str;
    istringstream tokenStream(text);
    while (getline(tokenStream, str, delimiter)) {
        strs.push_back(str);
    }
    return strs;
}

// 解析请求行 获取method url http版本号
http_conn::HTTP_CODE http_conn::parse_request_line(string text) {
    vector<string> requestPart = split(text, ' ');
    if (requestPart.size() != 3) {
        return BAD_REQUEST;
    }

    if (requestPart[0] == "GET") {
        m_method = GET;
    } else if (requestPart[0] == "POST") {
        m_method = POST;
        cgi = 1;
    } else {
        return BAD_REQUEST;
    }

    m_url = requestPart[1];
    m_version = requestPart[2];
    if (m_version != "HTTP/1.1") {
        return BAD_REQUEST;
    }

    if (m_url.substr(0, 7) == "http://") {
        m_url = m_url.substr(7);
    } else if (m_url.substr(0, 8) == "https://") {
        m_url = m_url.substr(8);
    }
    if (m_url[0] != '/') {
        if (m_url.find('/') != string::npos) {
            m_url = m_url.substr(m_url.find('/'));
        } else {
            m_url = "/";
        }
    }

    // 少一个judge.html    ???

    // ? 检查完requestline?
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

void skipSpace(string& str) {
    int idx = 0;
    if (str[0] != ' ' && str[0] != '\t') {
        return;
    }
    while (str[idx] == ' ' || str[idx] == '\t') {
        idx++;
    }
    str = str.substr(idx);
}

http_conn::HTTP_CODE http_conn::parse_headers(string text) {
    if (text == "") {
        if (m_content_length != 0) {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    } else if (text.substr(0, 11) == "Connection:") {
        text = text.substr(11);
        skipSpace(text);
        if (text == "keep-alive") {
            m_linger = true;
        }
    } else if (text.substr(0, 15) == "Content-length:") {
        text = text.substr(15);
        skipSpace(text);
        m_content_length = atol(text.c_str());
    } else if (text.substr(0, 5) == "Host:") {
        text = text.substr(5);
        skipSpace(text);
        m_host = text;
    } else {
        LOG_INFO("Unknow header: " + text);
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::parse_content(string text) {
    // 判断报文是否完全读入  （内容长度+报文头长度）
    if (m_read_idx >= m_content_length + m_checked_idx) {
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

int strRchr(string str, char ch) {
    for (int i = str.length() - 1; i > 0; i--) {
        if (str[i] == ch) {
            return i;
        }
    }
    return -1;
}

http_conn::HTTP_CODE http_conn::do_request() {
    m_real_file = doc_root;
    int len = doc_root.length();
    int p = strRchr(doc_root, '/');

    // 
    if (cgi == 1 && (doc_root[p + 1] == '2' || doc_root[p + 1] == '3')) {
        //根据标志判断是登录检测还是注册检测
        char flag = m_url[1];

        string m_url_real = "/" + m_url.substr(2) + m_real_file;

        //将用户名和密码提取出来
        //user=123&passwd=123
        string name, password;
        int i;
        for (i = 5; m_string[i] != '&'; ++i)
            name[i - 5] = m_string[i];

        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];

        if (doc_root[p + 1] == '3') {
            //如果是注册，先检测数据库中是否有重名的
            //没有重名的，进行增加数据
            string sql_insert = "INSERT INTO user(username, passwd) VALUES('" + name + "', '" + password + "')";

            if (users.find(name) == users.end()) {
                m_mutex.lock();
                int res = mysql_query(mysql, sql_insert.c_str());
                users.insert(pair<string, string>(name, password));
                m_mutex.unlock();

                if (!res)
                    m_url = "/log.html";
                else
                    m_url = "/registerError.html";
            } else
                m_url = "/registerError.html";
        } else if (doc_root[p + 1] == '2') {
            //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
            if (users.find(name) != users.end() && users[name] == password)
                m_url = "/welcome.html";
            else
                m_url = "/logError.html";
        }
    }

    if (doc_root[p + 1] == '0') {
        m_real_file += "/register.html";
    } else if (doc_root[p + 1] == '1'){
        m_real_file += "/log.html";
    } else {
        m_real_file += m_url;
    }

    if (stat(m_real_file.c_str(), &m_file_stat) < 0)
        return NO_RESOURCE;

    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;

    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;

    int fd = open(m_real_file.c_str(), O_RDONLY);
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;

}

// 解析出单行数据
http_conn::LINE_STATUS http_conn::parse_line() {
    char tmp;
    for (; m_checked_idx < m_read_idx; m_checked_idx++) {
        tmp = m_read_buf[m_checked_idx];
        if (tmp == '\r') {
            if (m_checked_idx + 1 == m_read_idx) {
                return LINE_OPEN;
            } else if (m_read_buf[m_checked_idx + 1] == '\n') {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if (tmp == '\n') {
            if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r') {
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

void http_conn::unmap() {
    if (m_file_address) {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

bool http_conn::add_response(string response) {
    if (m_write_idx >= WRITE_BUFFER_SIZE) {
        return false;
    }

    // 写入string大于设置最大长度返回长度但不写入
    int len = snprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - m_write_idx - 1, "%s", response.c_str());
    if (len >= WRITE_BUFFER_SIZE - m_write_idx - 1) {
        LOG_ERROR("Buffer Not Enough");
        return false;
    }

    m_write_idx += len;
    LOG_INFO("request:" + string(m_write_buf));
    return true;
}

bool http_conn::add_content(string content) {
    return add_response(content);
}

bool http_conn::add_status_line(int status, string title) {
    string status_line = "HTTP/1.1 " + to_string(status) + " " + title + "\r\n";
    return add_response(status_line);
}

bool http_conn::add_headers(int content_length) {
    return add_content_length(content_length) && add_linger() && add_blank_line();
}

bool http_conn::add_content_type() {
    return add_content("Content-Type:text/html\r\n");
}

bool http_conn::add_content_length(int content_length) {
    string con_len = "Content-Length:" + to_string(content_length) + "\r\n";
    return add_content(con_len);
}

bool http_conn::add_linger() {
    if (m_linger) {
        return add_content("Connection:keep-alive\r\n");
    } else {
        return add_content("Connection:close\r\n");
    }
}

bool http_conn::add_blank_line() {
    return add_content("\r\n");
}

void http_conn::process() {
    HTTP_CODE read_ret = process_read();
    // 不用返回数据
    if (read_ret == NO_REQUEST) {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        return;
    }
    if (!process_write(read_ret)) {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
}

// 一次把数据读完
bool http_conn::read_once() {
    if (m_read_idx >= READ_BUFFER_SIZE) {
        return false;
    }

    int bytes_read = 0;
    // LT模式
    if (m_TRIGMode == 0) {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);

        if (bytes_read <= 0) {
            return false;
        }
        m_read_idx += bytes_read;
    } else {
        // ET模式
        while (true) {
            bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);

            if (bytes_read == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                return false;
            }

            if (bytes_read == 0) {
                return false;
            }

            m_read_idx += bytes_read;
        }
    }
    return true;
}

bool http_conn::write() {
    if (bytes_to_send == 0) {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        init();
        return true;
    }

    int bytes_write;
    
    while (true) {
        bytes_write = writev(m_sockfd, m_iv, m_iv_count);

        if (bytes_write < 0) {
            if (errno == EAGAIN) {
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
                return true;
            }
            unmap();
            return false;
        }

        bytes_to_send -= bytes_write;
        bytes_have_send += bytes_write;

        // ?? 判断iv[0]是否发送完
        if (bytes_have_send >= m_iv[0].iov_len) {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        } else {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len -= bytes_have_send;
        }

        if (bytes_to_send <= 0) {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);

            if (m_linger) {
                init();
                return true;
            }
            return false;
        }
    }
}