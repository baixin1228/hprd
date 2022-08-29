#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <pthread.h>
#include <sys/types.h>
#include <linux/tcp.h>
#include "util.h"
#include "rt_net_socket.h"
#include "rt_net_piple.h"

int tcp_send_pkt(struct rt_net_client *client, const uint8_t *buf, uint16_t len)
{
    int ret;
    if(ALIGN4(len) > MAX_PKT_SIZE - sizeof(client->tcp_send_buf.head))
    {
        func_error("data size is too long.");
        return -1;
    }

    if(len > 0)
        _show_protocol(client, buf, len, true);

    client->tcp_send_buf.head.magic = htonl(PKT_MAGIC);
    client->tcp_send_buf.head.idx = htonl(client->tcp_send_idx);
    client->tcp_send_buf.head.data_len = ALIGN4(len);

    if(len > 0)
        memcpy(client->tcp_send_buf.data, buf, len);

    ret = send(client->tcp_fd, (const void *)&client->tcp_send_buf,
        ALIGN4(len) + sizeof(client->tcp_send_buf.head),
        0);
    if(ret != -1)
        client->tcp_send_idx++;
    return ret;
}

int udp_send_pkt(struct rt_net_client *client, const uint8_t *buf, uint16_t len)
{
    int ret;
    if(!client->udp_state)
    {
        func_error("UDP link does not exist.");
        return -1;
    }

    if(ALIGN4(len) > MAX_PKT_SIZE - sizeof(client->udp_send_buf.head))
    {
        func_error("data size is too long.");
        return -1;
    }

    if(len > 0)
        _show_protocol(client, buf, len, true);

    client->udp_send_buf.head.magic = htonl(PKT_MAGIC);
    client->udp_send_buf.head.idx = htonl(client->udp_send_idx);
    client->udp_send_buf.head.data_len = ALIGN4(len);

    if(len > 0)
        memcpy(client->udp_send_buf.data, buf, len);

    ret = sendto(client->udp_fd, (const void *)&client->udp_send_buf,
        ALIGN4(len) + sizeof(client->udp_send_buf.head),
        0,
        (struct sockaddr*)&client->udp_client_addr,
        sizeof(client->udp_client_addr));

    if(ret != -1)
        client->udp_send_idx++;

    return ret;
}

static int _tcp_recv_len(struct rt_net_client *client, uint8_t *buf, int len)
{
    int recv_tmp;
    int recv_len = 0;
    while(recv_len < len)
    {
        recv_tmp = recv(client->tcp_fd, buf + recv_len, len - recv_len, 0);
        if(recv_tmp <= 0)
            return -1;
        recv_len += recv_tmp;
    }
    return len;
}

static int _tcp_recv_pkt(struct rt_net_client *client, struct pkt_buf *buf)
{
    int ret;

    ret = _tcp_recv_len(client, (uint8_t *)&buf->head.idx, sizeof(buf->head) - sizeof(buf->head.magic));
    if(ret == -1)
        return -1;

    if(buf->head.data_len > 0)
    {
        if(buf->head.data_len <= sizeof(buf->data))
        {
            ret = _tcp_recv_len(client, buf->data, buf->head.data_len);
        }else{
            func_error("pkt data len is too large.");
        }
    }

    return ret;
}

static int tcp_try_recv(struct rt_net_client *client, struct pkt_buf *buf)
{  
    int ret = _tcp_recv_len(client, (uint8_t *)&buf->head.magic, sizeof(buf->head.magic));
    if(ret != -1)
    {
        if(ntohl(buf->head.magic) == PKT_MAGIC)
        {
            ret = _tcp_recv_pkt(client, buf);
        }else{
            log_warning("tcp recv: magic is invaild. %lx %lx %lx", buf->head.magic, ntohl(buf->head.magic), htonl(PKT_MAGIC));
            ret = 0;
        }
    }
 
    return ret;
}

static void *_tcp_recv_process(void *arg)
{
    int ret = 0;
    struct rt_net_client *client = (struct rt_net_client *)arg;
    struct pkt_buf *buf = malloc(sizeof(struct pkt_buf));

    log_info("tcp start recv from ip:%s port:%d", 
            inet_ntoa(client->tcp_client_addr.sin_addr),
            ntohs(client->tcp_client_addr.sin_port));

    while(ret != -1)
    {
        ret = tcp_try_recv(client, buf);
        if(ret > 0)
        {
            _on_data_recv(client, buf->data, buf->head.data_len);
        }
    }

    log_info("tcp stop recv from ip:%s port:%d", 
            inet_ntoa(client->tcp_client_addr.sin_addr),
            ntohs(client->tcp_client_addr.sin_port));

    close(client->tcp_fd);
    free(buf);
    free(client);
    return NULL;
}

int tcp_accept(struct rt_net_server *server, struct rt_net_client *client)
{
    uint32_t length = sizeof(client->tcp_client_addr);
    client->tcp_fd = accept(server->tcp_listen_fd, (struct sockaddr*)&client->tcp_client_addr, &length);
    if(client->tcp_fd < 0)
    {
        func_error("tcp accept failed.");
        return -1;
    }

    pthread_create(&client->tcp_recv_thread, NULL, _tcp_recv_process, client);  
    return 0;
}

static void *_udp_recv_process(void *arg)
{
    int ret = 1;
    struct pkt_buf *buf = malloc(sizeof(struct pkt_buf));
    struct rt_net_client *client = (struct rt_net_client *)arg;
    uint32_t len = sizeof(client->udp_recv_addr);

    log_info("upd start recv from ip:%s,port:%d",
        inet_ntoa(client->udp_client_addr.sin_addr),
        ntohs(client->udp_client_addr.sin_port));
    while(ret > 0)
    {
        ret = recvfrom(client->udp_fd, (uint8_t *)buf, MAX_PKT_SIZE, 0, (struct sockaddr*)&client->udp_recv_addr, &len);

        if(ret > 0)
        {
            if(client->udp_state == false)
            {
                client->udp_state = true;
                log_info("udp connected");
            }
            if(ntohl(buf->head.magic) == PKT_MAGIC)
            {
                if(buf->head.data_len <= sizeof(buf->data) && buf->head.data_len > 0)
                    _on_data_recv(client, buf->data, buf->head.data_len);
            }else{
                log_warning("udp magic error.");
            }
        }
    }
    log_info("upd stop recv from ip:%s,port:%d",
        inet_ntoa(client->udp_client_addr.sin_addr),
        ntohs(client->udp_client_addr.sin_port));

    free(buf);
    return NULL;
}

int net_server_init(struct rt_net_server *server, char *addr, uint16_t port)
{
    int flag = 1;

    server->tcp_listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    server->udp_client.udp_fd = socket(PF_INET, SOCK_DGRAM, 0);

    setsockopt(server->tcp_listen_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));

    bzero(&server->tcp_server_addr, sizeof(server->tcp_server_addr));
    bzero(&server->udp_client.udp_client_addr, sizeof(server->udp_client.udp_client_addr));

    server->tcp_server_addr.sin_family = AF_INET;
    server->tcp_server_addr.sin_port = htons(port);
    if(addr)
    {
        server->tcp_server_addr.sin_addr.s_addr = inet_addr(addr);
    }else{
        server->tcp_server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    server->udp_client.udp_client_addr.sin_family = AF_INET;
    server->udp_client.udp_client_addr.sin_port = htons(port);
    if(addr)
    {
        server->udp_client.udp_client_addr.sin_addr.s_addr = inet_addr(addr);
    }else{
        server->udp_client.udp_client_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    if(bind(server->tcp_listen_fd, (struct sockaddr *)&server->tcp_server_addr, sizeof(server->tcp_server_addr)) != 0)
    {
        func_error("tcp bind failed.");
        return -1;
    }

    if(bind(server->udp_client.udp_fd, (struct sockaddr *)&server->udp_client.udp_client_addr, sizeof(server->udp_client.udp_client_addr)) != 0)
    {
        func_error("udp bind failed.");
        goto FAIL1;
    }

    if(listen(server->tcp_listen_fd, 1024) != 0)
    {
        func_error("tcp listen failed.");
        goto FAIL2;
    }

    pthread_create(&server->udp_client.udp_recv_thread, NULL, _udp_recv_process, &server->udp_client);  
    pthread_detach(server->udp_client.udp_recv_thread);
    return 0;

FAIL2:
    close(server->udp_client.udp_fd);
FAIL1:
    close(server->tcp_listen_fd);
    return -1;
}

int net_server_release(struct rt_net_server *server)
{
    shutdown(server->udp_client.udp_fd, SHUT_RDWR);
    close(server->udp_client.udp_fd);
    shutdown(server->tcp_listen_fd, SHUT_RDWR);
    close(server->tcp_listen_fd);

    if(server->udp_client.udp_recv_thread)
        pthread_join(server->udp_client.udp_recv_thread, NULL);
    return 0;
}

int net_client_init(struct rt_net_client *client, char *addr, uint16_t port)
{
    int flag = 1;

    client->tcp_fd = socket(PF_INET, SOCK_STREAM, 0);
    client->udp_fd = socket(PF_INET, SOCK_DGRAM, 0);

    setsockopt(client->tcp_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));

    bzero(&client->tcp_client_addr, sizeof(client->tcp_client_addr));
    bzero(&client->udp_client_addr, sizeof(client->udp_client_addr));

    client->tcp_client_addr.sin_family = AF_INET;
    client->tcp_client_addr.sin_port = htons(port);
    if(addr)
    {
        client->tcp_client_addr.sin_addr.s_addr = inet_addr(addr);
    }else{
        func_error("addr is null.");
        return -1;
    }

    client->udp_client_addr.sin_family = AF_INET;
    client->udp_client_addr.sin_port = htons(port);
    client->udp_client_addr.sin_addr.s_addr = inet_addr(addr);


    if(connect(client->tcp_fd, (struct sockaddr *)&client->tcp_client_addr, sizeof(client->tcp_client_addr)) != 0)
    {
        func_error("tcp connect failed.");
        return -1;
    }

    pthread_create(&client->tcp_recv_thread, NULL, _tcp_recv_process, client);  
    pthread_create(&client->udp_recv_thread, NULL, _udp_recv_process, client);  

    client->udp_state = true;
    udp_send_pkt(client, NULL, 0);

    return 0;
}

int net_client_release(struct rt_net_client *client)
{
    shutdown(client->udp_fd, SHUT_RDWR);
    close(client->udp_fd);
    shutdown(client->tcp_fd, SHUT_RDWR);
    close(client->tcp_fd);

    if(client->tcp_recv_thread)
        pthread_join(client->tcp_recv_thread, NULL);

    if(client->udp_recv_thread)
        pthread_join(client->udp_recv_thread, NULL);
    return 0;
}