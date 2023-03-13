#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "util.h"
#include "protocol.h"
#include "net_help.h"

int client_connect(uint32_t ip, uint16_t port)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("\n");
		return fd;
	}

	struct sockaddr_in addr = {
		.sin_family = PF_INET,
		.sin_addr.s_addr = ip,
		.sin_port = port
	};

	int err = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
	if (err < 0) {
		perror("\n connect");
		return err;
	}

	return fd;
}

int client_send_pkt(int fd, char *buf, size_t len)
{
	return tcp_send_pkt(fd, buf, len);
}