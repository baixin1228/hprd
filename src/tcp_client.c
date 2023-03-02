#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void on_package(char *buf, size_t len);

#define BUFLEN 1024 * 1024 * 10
char buf[BUFLEN] = {0};

void *tcp_client_thread(void *opaque)
{
	int need_recv_len;
	int let_len;
	int sg_len;
	int sum_len;
	int fd = socket(AF_INET, SOCK_STREAM, 0);
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
		sg_len = recv(fd, buf, 4, 0);
		if(sg_len < 4)
			exit(0);

		need_recv_len = ntohl(*(uint32_t *)buf);
		if(need_recv_len > 0)
		{
			sum_len = 0;
			let_len = need_recv_len;
			while(let_len > 0)
			{
				sg_len = recv(fd, buf + sum_len, let_len, 0);
				if(sg_len < 0)
					exit(0);

				sum_len += sg_len;
				let_len -= sg_len;
			}

			on_package(buf, need_recv_len);
		}

	}
	close(fd);
	return NULL;
}