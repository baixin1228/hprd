#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "protocol.h"
#include "net_help.h"

void on_package(char *buf, size_t len);

static void _on_client_pkt(char *buf, size_t len)
{
	struct data_pkt *pkt = (struct data_pkt *)buf;
	switch(pkt->cmd)
	{
		case VIDEO_DATA:
		{
			on_package(pkt->data, pkt->data_len);
			break;
		}
	}
}

static int fd;
#define BUFLEN 1024 * 1024 * 10
static char _recv_buf[BUFLEN];
void *tcp_client_thread(void *opaque)
{
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("\n");
		exit(-1);
	}

	struct sockaddr_in addr = {
		.sin_family = PF_INET,
		.sin_addr.s_addr = inet_addr("127.0.0.1"),
		.sin_port = htons(9999)
	};

	int err = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
	if (err < 0) {
		perror("\n connect");
		exit(-1);
	}

	while(1)
	{
		tcp_recv_pkt(fd, _recv_buf, _on_client_pkt);
	}
	close(fd);
	return NULL;
}

int client_send_pkt(char *buf, size_t len)
{
	return tcp_send_pkt(fd, buf, len);
}