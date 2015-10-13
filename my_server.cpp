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
#include<sys/mman.h>
#include<stdarg.h>
#include<errno.h>
#include"service.h"
#include"myserver_threadpool.h"
#include"service.cpp"

#define MAX_FD 1024
extern void addfd(int epollfd,int fd,bool one_shot);
extern void modfd(int epollfd,int fd,int ev);
void addsig(int sig,void(handler)(int),bool restart=true)
{
	struct sigaction sa;
	memset(&sa,'\0',sizeof(sa));
	sa.sa_handler=handler;
	if(restart)
	{
		sa.sa_flags|=SA_RESTART;
	}
	sigfillset(&sa.sa_mask);
	assert(sigaction(sig,&sa,NULL)!=-1);
}

void show_error(int connfd,const char* info)
{
	printf("%s",info);
	send(connfd,info,strlen(info),0);
	close(connfd);
}

void close_conn(int epollfd,int fd)
{
	epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
	close(fd);
}

int main(int argc,char* argv[])
{
	if(argc<=2)
	{
		printf("usage %s ip_address port_number\n",basename(argv[0]));
		return 1;
	}
	const char* ip=argv[1];
	int port=atoi(argv[2]);
	addsig(SIGPIPE,SIG_IGN);

	myserver_threadpool<service>* pool=NULL;
	pool=new myserver_threadpool<service>(3);

	service* users=new service[MAX_FD];
	assert(users);
	int user_count=0;

	int listenfd=socket(PF_INET,SOCK_STREAM,0);
	assert(listenfd>=0);
	struct linger tmp={1,0};
	setsockopt(listenfd,SOL_SOCKET,SO_LINGER,&tmp,sizeof(tmp));

	int ret=0;
	struct sockaddr_in address;
	bzero(&address,sizeof(address));
	address.sin_family=AF_INET;
	inet_pton(AF_INET,ip,&address.sin_addr);
	address.sin_port=htons(port);

	ret=bind(listenfd,(struct sockaddr*)&address,sizeof(address));
	assert(ret>=0);

	ret=listen(listenfd,5);
	assert(ret>=0);

	epoll_event events[5];
	int epollfd=epoll_create(5);
	assert(epollfd!=-1);
	addfd(epollfd,listenfd,false);
	service::m_epollfd=epollfd;
	while(1)
	{
		int number=epoll_wait(epollfd,events,5,-1);

		for(int i=0;i<number;i++)
		{
			int sockfd=events[i].data.fd;
			if(sockfd==listenfd)
			{
				struct sockaddr_in client_address;
				socklen_t client_addrlength=sizeof(client_address);
				int connfd=accept(listenfd,(struct sockaddr*)&client_address,&client_addrlength);
				if(connfd<0)
				{
					printf("errno is: %d\n",errno);
					continue;
				}
				printf("There is a new connection\n");
				users[connfd].init(connfd,client_address);

			}
			else if(events[i].events&(EPOLLRDHUP|EPOLLHUP|EPOLLERR))
			{
				printf("events is EPOLLRDHUP|EPOLLHUP|EPOLLERR\n");
				users[sockfd].close_conn();
			}
			else if(events[i].events&EPOLLIN)
			{
				printf("events is EPOLLIN\n");
				sockfd=events[i].data.fd;
				if(sockfd<0)
					continue;
				if(users[sockfd].sread())
				{
					pool->append(users+sockfd);
				}
				else
				{
					users[sockfd].close_conn();
				}
			}
			else if(events[i].events&EPOLLOUT)
			{
				printf("events is EPOLLOUT\n");
				sockfd=events[i].data.fd;
				if(users[sockfd].swrite())
				{
					//users[sockfd].close_conn();
				}
			}
		}
	}
	close(epollfd);
	close(listenfd);
	delete [] users;
	delete pool;
	return 0;
}
