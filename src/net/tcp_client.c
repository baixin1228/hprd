#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

#define N 128
int main(int argc,char *argv[])
{
    if(argc < 3)
    {
        fprintf(stderr,"Usage:%s [ip] [port]\n",argv[0]);
        exit(1);
    }

    //第一步:通过socket函数创建一个TCP套接字
    int sockfd;
    if((sockfd = socket(AF_INET,SOCK_STREAM,0)) == -1)
    {
        perror("fail to socket");
        exit(1);
    }

    printf("sockefd = %d\n",sockfd);

    //第二步:发送客户端连接请求

    struct sockaddr_in serveraddr;
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(argv[1]);
    serveraddr.sin_port = htons(atoi(argv[2]));
    socklen_t addr_len = sizeof(serveraddr);

    if(connect(sockfd,(struct sockaddr*)&serveraddr,addr_len) == -1)
    {
        perror("fail to connect");
        exit(1);
    }

    while(1)
    {
    //第三步:进行通信
    //发送数据

    char buf[N] = "";
    fgets(buf,N,stdin);
    buf[strlen(buf) -1] = '\0';
    if(send(sockfd,buf,N,0) == -1)
    {
        perror("fail to send");
        exit(1);
    }

    //接收数据
    char text[N] = "";
    if(recv(sockfd,text,N,0) == -1)
    {
        perror("fail to recv");
        exit(1);
    }

    printf("from server:%s\n",text);
    }

    //第四步:关闭套接字文件描述符
    close(sockfd);

}
