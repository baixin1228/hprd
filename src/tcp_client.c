#include <sys/socket.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUFLEN 1024
int main()
{
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
  {
    perror("\n");
    return 0;
  }
  // fcntl(fd, F_SETFL, O_NONBLOCK);
  struct sockaddr_in addr = {.sin_family = PF_INET,
                             .sin_addr.s_addr = inet_addr("127.0.0.1"),
                             .sin_port = htons(9999)};
  // inet_pton(AF_INET, servInetAddr, &servaddr.sin_addr);
  int err = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
  if (err < 0)
  {
    perror("\n connect");
    return 0;
  }

  char buf[1024]={0};
  
  printf("等待发送:%s\n", buf);
  time_t start = time(0);
  for (int i = 0; i < 2; ++i)
  {
    memset(buf , 0 , strlen(buf));
    sprintf(buf, "Hello world_%ld", time(0));
    int n = send(fd, buf, strlen(buf), 0);
    if (n == -1)
    {
      perror("\n");
      return 1;
    }
    printf("[发送] len=%d data=%s\n", n, buf);
    memset(buf, 0, BUFLEN);
    recv(fd, buf, BUFLEN, 0);
    printf("[收到] len=%ld data=%s\n", strlen(buf), buf);
  }
  close(fd);
  time_t end = time(0);
  printf("duration:%ld\n",end-start);
}