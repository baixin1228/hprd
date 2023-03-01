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
	int recv_len;
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("\n");
		exit(-1);
	}
	// fcntl(fd, F_SETFL, O_NONBLOCK);
	struct sockaddr_in addr = {
		.sin_family = PF_INET,
		.sin_addr.s_addr = inet_addr("127.0.0.1"),
		.sin_port = htons(9999)
	};
	// inet_pton(AF_INET, servInetAddr, &servaddr.sin_addr);
	int err = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
	if (err < 0) {
		perror("\n connect");
		exit(-1);
	}

	// printf("等待发送:%s\n", buf);
	// time_t start = time(0);
	// for (int i = 0; i < 2; ++i) {
	// 	memset(buf, 0, strlen(buf));
	// 	sprintf(buf, "Hello world_%ld", time(0));
	// 	int n = send(fd, buf, strlen(buf), 0);
	// 	if (n == -1) {
	// 		perror("\n");
	// 		exit(-1);
	// 	}
	// 	printf("[发送] len=%d data=%s\n", n, buf);
	// 	memset(buf, 0, BUFLEN);
	// 	recv(fd, buf, BUFLEN, 0);
	// 	printf("[收到] len=%ld data=%s\n", strlen(buf), buf);
	// }
	while(1)
	{
		recv_len = recv(fd, buf, BUFLEN, 0);
		if(recv_len < 0)
			exit(0);

		if(recv_len > 0)
			on_package(buf, recv_len);
	}
	close(fd);
	// time_t end = time(0);
	// printf("duration:%ld\n", end - start);
	return NULL;
}