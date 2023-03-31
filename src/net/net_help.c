#include <unistd.h>
#include <arpa/inet.h>
#include "util.h"
#include "net_help.h"

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

int tcp_recv_pkt(int fd, char *_recv_buf, void (*callback)(int fd, char *buf, size_t len))
{
	int pkt_len;

	if(_tcp_recv_all(fd, _recv_buf, 4) == -1)
		return -1;

	pkt_len = ntohl(*(uint32_t *)_recv_buf);
	if(pkt_len > 0)
	{
		if(_tcp_recv_all(fd, _recv_buf, pkt_len) == -1)
			return -1;

		callback(fd, _recv_buf, pkt_len);
	}

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

int tcp_send_pkt(int fd, char *buf, size_t len)
{
	uint32_t net_len;
	net_len = htonl(len);
	if(_tcp_send_all(fd, (char *)&net_len, 4) != 4)
		return -1;
	if(_tcp_send_all(fd, buf, len) != len)
		return -1;

	return 0;
}