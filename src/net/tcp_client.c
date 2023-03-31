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

int client_connect(char *ip, uint16_t port)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("\n");
		return -1;
	}

	struct sockaddr_in addr = {
		.sin_family = PF_INET,
		.sin_addr.s_addr = inet_addr(ip),
		.sin_port = htons(port)
	};

	int err = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
	if (err < 0) {
		perror("\n connect");
		return -1;
	}

	return fd;
}