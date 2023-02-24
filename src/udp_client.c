#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
 
#define MAXBUF 100
 
int main(int argc, const char *argv[])
{
    char *server = "127.0.0.1";
    char *serverport = "1234";
    char *echostring = "helloworld";
 
    struct addrinfo client,*servinfo;
    memset(&client,0,sizeof(client));
    client.ai_family = AF_INET;
    client.ai_socktype = SOCK_DGRAM;
    client.ai_protocol= IPPROTO_UDP;
 
    if(getaddrinfo(server,serverport,&client,&servinfo)<0)
    {
        printf("error in getaddrinfo");
        exit(-1);
    }
 
    int sockfd = socket(servinfo->ai_family,servinfo->ai_socktype,servinfo->ai_protocol);
    if(sockfd <0)
    {
        printf("error in socket create");
        exit(-1);
    }

	//create connection
    char bufmsg[2];
	bufmsg[0]=0;
	bufmsg[1]='\0';
    ssize_t numBytes = sendto(sockfd,bufmsg,strlen(bufmsg),0,servinfo->ai_addr,servinfo->ai_addrlen);
    if(numBytes < 0)
    {
        printf("error in send the data");
    }
 
	struct sockaddr_storage fromaddr;
    socklen_t fromaddrlen = sizeof(fromaddr);
    char buf[MAXBUF+1];

    numBytes = recvfrom(sockfd, buf, MAXBUF+1, 0, (struct sockaddr *)&fromaddr, &fromaddrlen);
	if (buf[0] == 1)
	{
		printf("connected to the server\n");
	}
	
    //data transfer
    while (1) {
            char  echomsg[1024];
            echomsg[0]=2;
            char *ptr = echomsg+1;
            strcpy(ptr,echostring);

            numBytes=0;
            numBytes = sendto(sockfd,echomsg,strlen(echomsg),0,servinfo->ai_addr,servinfo->ai_addrlen);
            if(numBytes<0)
            {
                    printf("error in send the data");
            }	

            numBytes=0;
            numBytes = recvfrom(sockfd, buf, MAXBUF+1,0,(struct sockaddr *)&fromaddr,&fromaddrlen);
            if(buf[0]==2)
            {
                    char *str=buf+1;
                    buf[numBytes] = '\0';
                    printf("recv msg from server %s\n",str);
            }

            sleep(3);
    }

    //close connection
	numBytes=0;
	buf[0] = 3;
	numBytes = sendto(sockfd,buf,2,0,servinfo->ai_addr,servinfo->ai_addrlen);
    if(numBytes<0)
    {
        printf("error in send the data");
    }	
 
    freeaddrinfo(servinfo);
 
    printf("Received:%s \n",buf);

    close(sockfd);
    return 0;
}