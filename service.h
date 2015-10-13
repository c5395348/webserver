#ifndef SERVICE_H
#define SERVICE_H

#include<unistd.h>
#include<signal.h>
#include<sys/types.h>
#include<sys/epoll.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<string.h>
#include<sys/stat.h>
#include<pthread.h>
#include<stdio.h>
#include<stdlib.h>
#include<stdarg.h>
#include<errno.h>
#include<time.h>
#include"locker.h"
class service
{
	public:
	static const int READ_BUFFER_SIZE=2048;
	static const int WRITE_BUFFER_SIZE=1024;
	static const int FILE_BUFFER_SIZE=2048;
	enum METHOD {GET=0,POST,HEAD,PUT,DELETE,TRACE,OPTIONS,CONNECT,PATCH};
	enum CHECK_STATE {CHECK_STATE_REQUESTLINE=0,CHECK_STATE_HEADER,CHECK_STATE_CONTENT};
	enum HTTP_CODE {NO_REQUEST,GET_REQUEST,BAD_REQUEST,NO_RESOURCE,FORBIDDEN_REQUEST,FILE_REQUEST,INTERNAL_ERROR,CLOSED_CONNECTION};
	enum LINE_STATUS{LINE_OK=0,LINE_BAD,LINE_OPEN};
	public:
	service(){};
	~service(){};

	public:
	void init(int sockfd,const sockaddr_in& addr);
	void close_conn(bool real_close=true);
	void process();
	bool sread();
	bool swrite();


	private:
	void init();
	bool send_http_head(int conn_socket, int status, char *s_status, char *filetype);
	HTTP_CODE process_read();
	int send_html();
	void add_page_error(int status,char* s_status,char* msg);
	void add_http_head(int status, char *s_status, char *filetype);
	bool process_write(HTTP_CODE ret);
	bool add_response(const char* format,...);
	void do_url();

	bool add_content(const char* content);
	bool add_status_line(int status,const char* title);
	bool add_headers(int content_length);
	bool add_content_length(int content_length);
	bool add_blank_line();
	bool add_date();


	char* get_line(){return read_buf+m_start_line;}
	LINE_STATUS parse_line();

	HTTP_CODE parse_request_line(char* text);

	public:
	static int m_epollfd;
	static int m_user_count;

	private:
	int m_sockfd;
	sockaddr_in m_address;
	int m_read_idx;
	int m_checked_idx;
	int m_start_line;
	int m_write_idx;

	CHECK_STATE m_check_state;
	char* m_method=new char[100];

	char* m_url=new char[100];
	char* m_version=new char[100];
	char* m_host;
	int m_content_length;

	char read_buf[READ_BUFFER_SIZE];
	char write_buf[WRITE_BUFFER_SIZE];
	char file_buf[FILE_BUFFER_SIZE];

	char* m_file_address;
	struct stat m_file_stat;



};
#endif
