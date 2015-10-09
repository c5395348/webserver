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
#include"locker.h"
class service
{
	public:
	static const int READ_BUFFER_SIZE=2048;
	static const int WRITE_BUFFER_SIZE=1024;
	public:
	service(){};
	~service(){};

	public:
	void init(int sockfd,const sockaddr_in& addr);
	void close_conn(bool real_close=true);
	void process();
	bool read();
	bool write();

	private:
	void init();

	public:
	static int m_epollfd;
	static int m_user_count;

	private:
	int m_sockfd;
	sockaddr_in m_address;
	char read_buf[READ_BUFFER_SIZE];
	char write_buf[WRITE_BUFFER_SIZE];
};
#endif
