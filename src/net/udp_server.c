#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int udp_server_init(uint32_t port)
{

    while(1)
    {
        int len = sizeof(caddr);
        char buff[128] = {0};
        recvfrom(sockfd,buff,127,0,(struct sockaddr*)&caddr,&len);
        printf("ip:%s,port:%d,buff=%s\n",inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port),buff );

        sendto(sockfd,"ok",2,0,(struct sockaddr*)&caddr,sizeof(caddr));
    }

    close(sockfd);
}