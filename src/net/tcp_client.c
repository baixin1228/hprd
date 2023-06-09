#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>

#include "util.h"
#include "protocol.h"
#include "net/net_util.h"
#include "net/tcp_client.h"
#include "net/net_client.h"

static inline int _tcp_recv_all(int fd, char *_recv_buf, size_t len)
{
	int recv_len;
	int sum_len = 0;

	while(len > 0)
	{
		recv_len = recv(fd, _recv_buf + sum_len, len, 0);
		if(recv_len <= 0)
		{
			log_info("recv: connect closed.");
			return -1;
		}

		sum_len += recv_len;
		len -= recv_len;
	}
	return 0;
}

static int _tcp_recv_pkt(int fd, char *_recv_buf, size_t buf_len,
	void (*callback)(char *buf, size_t len))
{
	uint32_t pkt_len;
	static int count = 0;

	if(_tcp_recv_all(fd, _recv_buf, 4) == -1)
		return -1;

	pkt_len = ntohl(*(uint32_t *)_recv_buf);
	if(pkt_len > buf_len)
	{
		log_error("pkt_len is too big:%d %d", pkt_len, count);
		exit(-1);
	}
	if(pkt_len > 0)
	{
		if(_tcp_recv_all(fd, _recv_buf, pkt_len) == -1)
			return -1;

		callback(_recv_buf, pkt_len);
	}else{
		log_error("recv len is invalid.");
	}
	count++;
	return 0;
}

static inline int _tcp_send_all(int fd, char *buf, size_t len)
{
	int send_count;
	int sum_count = 0;

	while(len > 0)
	{
		send_count = send(fd, buf + sum_count, len, 0);
		if(send_count <= 0)
		{
			log_perr("send error fd:%d buf:%p buf_len:%d", fd, buf, len);
			return -1;
		}

		sum_count += send_count;
		len -= send_count;
	}
	return sum_count;
}

static int _tcp_send_pkt(int fd, char *buf, size_t len)
{
	uint32_t net_len;
	net_len = htonl(len);
	if(_tcp_send_all(fd, (char *)&net_len, 4) != 4)
		return -1;
	if(_tcp_send_all(fd, buf, len) != len)
		return -1;

	return 0;
}

struct tcp_client {
	int fd;
} tcp_client = {0};

static char _recv_buf[BUFLEN];
static void *_recv_thread(void *oq) {
	while(_tcp_recv_pkt(tcp_client.fd, _recv_buf, BUFLEN, client_on_pkg) != -1);
	log_warning("_recv_thread exit.");
	exit(0);
	return NULL;
}

int connect_timeout(int sock_fd,struct sockaddr_in *servaddr, int time_out)
{
    int ret = -1;

    fcntl(sock_fd, F_SETFL, fcntl(sock_fd, F_GETFL, 0) | O_NONBLOCK);
    int connected = connect(sock_fd, (struct sockaddr*)servaddr, sizeof(struct sockaddr_in));
    if(connected != 0)
	{
		if(errno != EINPROGRESS)
			printf("connect error :%s\n",strerror(errno));
		else
		{
			struct timeval tm = {time_out, 0};
			fd_set wset,rset;
			FD_ZERO(&wset);
			FD_ZERO(&rset);
			FD_SET(sock_fd,&wset);
			FD_SET(sock_fd,&rset);
			long t1 = time(NULL);
			int res = select(sock_fd+1, &rset, &wset, NULL, &tm);
			long t2 = time(NULL);
			printf("interval time: %ld\n", t2 - t1);
			if(res < 0)
			{
				printf("network error in connect\n");
			}
			else if(res == 0)
			{
				printf("connect time out\n");
			}
			else if (1 == res)
			{
				if(FD_ISSET(sock_fd,&wset))
				{
					printf("connect succeed.\n");
					fcntl(sock_fd, F_SETFL, fcntl(sock_fd, F_GETFL, 0) & ~O_NONBLOCK);
					ret = 0;
				}
				else
				{
					printf("other error when select:%s\n",strerror(errno));
				}
			}
		}
	}else{
		fcntl(sock_fd, F_SETFL, fcntl(sock_fd, F_GETFL, 0) & ~O_NONBLOCK);
	}
 
    return ret;
}

int tcp_client_init(char *ip, uint16_t port)
{
	pthread_t p1;

	tcp_client.fd = socket(AF_INET, SOCK_STREAM, 0);
	if (tcp_client.fd < 0) {
		perror("\n");
		return -1;
	}

	struct sockaddr_in addr = {
		.sin_family = PF_INET,
		.sin_addr.s_addr = inet_addr(ip),
		.sin_port = htons(port)
	};

	int err = connect_timeout(tcp_client.fd, &addr, 2);
	if (err < 0) {
		return -1;
	}

	pthread_create(&p1, NULL, _recv_thread, NULL);
	pthread_detach(p1);
	return 0;
}

int tcp_client_send_pkt(char *buf, size_t len)
{
	return _tcp_send_pkt(tcp_client.fd, buf, len);
}

void tcp_client_release()
{
	if(tcp_client.fd != -1) {
		close(tcp_client.fd);
	}
	tcp_client.fd = -1;
}
