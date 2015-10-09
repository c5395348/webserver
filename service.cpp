#include"service.h"

int setnonblocking(int fd)
{
	int old_option=fcntl(fd,F_GETFL);
	int new_option=old_option|O_NONBLOCK;
	fcntl(fd,F_SETFL,new_option);
	return old_option;
}
void addfd(int epollfd,int fd,bool one_shot)
{
	epoll_event event;
	event.data.fd=fd;
	event.events=EPOLLIN|EPOLLET|EPOLLRDHUP|EPOLLOUT|EPOLLERR;
	if(one_shot)
	{
		event.events|=EPOLLONESHOT;
	}
	epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
	setnonblocking(fd);
}
void removefd(int epollfd,int fd)
{
	epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
	close(fd);
}


int service::m_user_count=0;
int service::m_epollfd=-1;
void service::close_conn(bool real_close)
{
	if(real_close&&(m_sockfd!=-1))
	{
		removefd(m_epollfd,m_sockfd);
		m_sockfd=-1;
		m_user_count--;
	}
}

void service::init(int sockfd,const sockaddr_in& addr)
{
	m_sockfd=sockfd;
	m_address=addr;
	addfd(m_epollfd,sockfd,true);
	m_user_count++;
	init();
}

void service::init()
{
	memset(read_buf,'\0',READ_BUFFER_SIZE);
	sprintf(write_buf,"helloworld");
	printf("finish init()\n");
}

bool service::sread()
{
	int bytes_read=read(m_sockfd,read_buf,READ_BUFFER_SIZE);
	if(bytes_read==-1)
	{
		if(errno==EAGAIN||errno==EWOULDBLOCK)
		{
			return false;
		}
		return false;
	}
	else if(bytes_read==0)
	{
		close(m_sockfd);
		return false;
	}
	printf("read_buf is %s\n",read_buf);
	return true;
}

bool service::swrite()
{
	int write_bytes=write(m_sockfd,write_buf,10);
	if(write_bytes)
	{
		return true;
	}
	return false;
}

void service::process()
{
	printf("process running successfully\n");
}
