#include"service.h"

const char* ok_200_title="OK";
const char* error_400_title="Bad Request";
const char* error_400_form="Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title="Forbidden";
const char* error_403_form="You do not have permission to get file from this server.\n";
const char* error_404_title="Not Found";
const char* error_404_form="The requested file was not found on this server.\n";
const char* error_500_title="Internal Error";
const char* error_500_form="There was an unusual problem serving the requested file.\n";
const char* doc_root="/var/www/html";

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

void modfd(int epollfd,int fd,int ev)
{
	epoll_event event;;
	event.data.fd=fd;
	event.events=ev|EPOLLET|EPOLLONESHOT|EPOLLRDHUP;
	epoll_ctl(epollfd,EPOLL_CTL_MOD,fd,&event);
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
	memset(write_buf,'\0',WRITE_BUFFER_SIZE);
	memset(file_buf,'\0',FILE_BUFFER_SIZE);
	m_check_state=CHECK_STATE_REQUESTLINE;

	m_content_length=0;
	m_host=0;
	m_start_line=0;
	m_checked_idx=0;
	m_read_idx=0;
	m_write_idx=0;
}

bool service::sread()
{
	if(m_read_idx>=READ_BUFFER_SIZE)
	{
		return false;
	}
	int bytes_read=0;
	while(true)
	{
		bytes_read=recv(m_sockfd,read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0);
		printf("read_buf is %s\n",read_buf);
		if(bytes_read==-1)
		{
			if(errno==EAGAIN||errno==EWOULDBLOCK)
			{
				break;
			}
			return false;
		}
		else if(bytes_read==0)
		{
			return false;
		}
		m_read_idx+=bytes_read;
	}
	return true;
}

bool service::swrite()
{
	printf("This is write() m_write_idx=%d\n",m_write_idx);
	printf("In swrite() write_buf is %s \n",write_buf);
	int temp=0;
	int bytes_have_send=0;
	int bytes_to_send=m_write_idx;
	if(bytes_to_send==0)
	{
		printf("bytes_to_send = 0\n");
		modfd(m_epollfd,m_sockfd,EPOLLIN);
		init();
		return false;
	}

	while(1)
	{
		temp=write(m_sockfd,write_buf,bytes_to_send);
		if(temp<=-1)
		{
			if(errno==EAGAIN)
			{
				modfd(m_epollfd,m_sockfd,EPOLLOUT);
				return true;
			}
			return false;
		}
		bytes_to_send-=temp;
		bytes_have_send+=temp;
		if(bytes_to_send==0)
		{
			init();
			modfd(m_epollfd,m_sockfd,EPOLLIN);
			return true;
		}
	}

}

void service::do_url()
{
	char *p;
    p=strchr(m_url+1, '?');
    if(p)
	{
		*p = 0;
		p++;
	}
}

void service::add_page_error(int status,char* s_status,char* msg)
{
    add_response("<html><head></head><body><h1> %s </h1><hr>Reage Web Server 0.01</body></head>", msg);
    add_http_head(status, s_status, "text/html");
}

void service::add_http_head(int status, char *s_status, char *filetype)
{
	add_status_line(status, s_status);
	add_date();
	add_content_length(m_file_stat.st_size);
	add_blank_line();

}

bool service::add_status_line(int status,const char* title)
{
	return add_response("%s %d %s\r\n","HTTP/1.1,",status,title);
}

bool service::add_content_length(int content_len)
{
	return add_response("Content-Length: %d \r\n",content_len);
}

bool service::add_blank_line()
{
	return add_response("%s","\r\n");
}

bool service::add_content(const char* content)
{
	return add_response("%s",content);
}

bool service::add_date()
{
	time_t timep;
	time (&timep);
	return add_response("Date: %s",ctime(&timep));
}


bool service::add_response(const char* format,...)
{
	if(m_write_idx>=WRITE_BUFFER_SIZE)
	{
		return false;
	}
	va_list arg_list;
	va_start(arg_list,format);
	int len=vsnprintf(write_buf+m_write_idx,WRITE_BUFFER_SIZE-1-m_write_idx,format,arg_list);
	if(len>=(WRITE_BUFFER_SIZE-1-m_write_idx))
	{
		return false;
	}
	m_write_idx+=len;
	va_end(arg_list);
	return true;
}


int service::send_html()
{

	int f;
	int tmp;
	printf("m_url = %s\n",m_url);
	m_url=strchr(m_url,'/');
	if(strlen(m_url)==1||!strcasecmp(m_url,"/favicon.ico"))
	{
		printf("exec strcpy\n");
		strcpy(m_url,"index.html");
	}
	else
	{
		printf("exec strchr\n");
		m_url=m_url+1;
		printf("m_url is %s\n",m_url);
	}
	if(stat(m_url,&m_file_stat)>=0)
	{
		//add_page_error(404,"Not found", "Not found<br/> Reage does not implement this mothod\n");
		printf("file exist\n");

	}
	else if(!(S_ISREG(m_file_stat.st_mode))||!(S_IRUSR&m_file_stat.st_mode))
	{
		add_page_error(403 , "Forbidden", "Forbidden<br/> Reage couldn't read the file\n");
		return 1;
	}
	add_http_head( 200, "OK", "text/html" );
	f=open(m_url,O_RDONLY);
	if(f<0)
	{
		add_page_error(404, "Not found", "Not found<br/> Reage couldn't read the file\n");
	}

	tmp=read(f,write_buf+m_write_idx,m_file_stat.st_size);
	m_write_idx+=m_file_stat.st_size;
	return 1;
}


service::HTTP_CODE service::process_read()
{
	if(sscanf(read_buf,"%s %s %s",m_method,m_url,m_version))
		{
			printf("the sscanf data is %s %s %s\n",m_method,m_url,m_version);
			printf("request correct\n");
		}
	else
		printf("request error\n");
	if(!strcasecmp(m_method, "get"))
		do_url();
	return FILE_REQUEST;
}

bool service::process_write(HTTP_CODE ret)
{
	if(ret==FILE_REQUEST)
	{
		if(send_html())
		    return true;
		else
			return false;
	}
	return false;
}

void service::process()
{
	HTTP_CODE read_ret=process_read();
	printf("process() read_ret=%d\n",read_ret);
	if(read_ret==NO_REQUEST)
	{
		modfd(m_epollfd,m_sockfd,EPOLLIN);
		return;
	}
	bool write_ret=process_write(read_ret);
	if(!write_ret)
	{
		close_conn();
	}
	modfd(m_epollfd,m_sockfd,EPOLLOUT);
}
