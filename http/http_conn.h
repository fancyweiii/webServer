#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <map>
#include <mysql/mysql.h>
#include <fstream>
#include <vector>
#include <sstream>

#include "../lock/locker.h"
#include "../CGImysql/sql_connect_pool.h"
#include "../timer/lst_timer.h"
#include "../log/log.h"

using namespace std;

class http_conn {
    public:
    static const int FILENAME_LEN = 200;
    static const int READ_BUFFER_SIZE = 2048;
    static const int WRITE_BUFFER_SIZE = 1024;
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

    public:
    http_conn() {};
    ~http_conn() {};

    public:
    void init(int sockfd, const sockaddr_in &addr, string , int, int, string user, string passwd, string name);
    void close_conn(bool real_close = true);
    void process();
    bool read_once();
    bool write();
    sockaddr_in *get_address() {
        return &m_address;
    }
    void init_mysql_result(connect_pool *connpool);
    int timer_flag;
    int improv;

    private:
    void init();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE code);
    HTTP_CODE parse_request_line(string text);
    HTTP_CODE parse_headers(string text);
    HTTP_CODE parse_content(string text);
    HTTP_CODE do_request();
    string get_line() { 
        return m_read_buf + m_start_line; 
    };
    LINE_STATUS parse_line();
    void unmap();
    bool add_response(string response);
    bool add_content(string content);
    bool add_status_line(int status, string title);
    bool add_headers(int content_length);
    bool add_content_type();
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

    public:
    static int m_epollfd;
    static int m_user_cnt;
    MYSQL *mysql;
    int m_state;  //读为0, 写为1


    private:
    int m_sockfd;
    sockaddr_in m_address;
    char m_read_buf[READ_BUFFER_SIZE];
    long m_read_idx;
    long m_checked_idx;
    int m_start_line;
    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;
    CHECK_STATE m_check_state;
    METHOD m_method;
    string m_real_file;
    string m_url;
    string m_version;
    string m_host;
    long m_content_length;
    bool m_linger;
    char *m_file_address;
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;
    int cgi;        //是否启用的POST
    string m_string; //存储请求头数据
    int bytes_to_send;
    int bytes_have_send;
    string doc_root;

    map<string, string> m_users;
    int m_TRIGMode;
    int m_close_log;

    string sql_user;
    string sql_passwd;
    string sql_name;
};

#endif