#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int tcp_listen_fd;
int udp_recv_fd;
pthread_t tcp_listen_thread;
pthread_t udp_recv_thread;
bool is_shut_down = false;

int tcp_send_pkt(struct ser_net_obj *obj, uint8_t *buf, size_t len)
{
    if(len > MAX_PKT_SIZE - sizeof(obj->tcp_send_buf.head))
        return -1;

    obj->tcp_send_buf.head.magic = PKT_MAGIC;
    obj->tcp_send_buf.head.idx = htonl(obj->tcp_send_idx);
    obj->tcp_send_buf.head.len = len;

    memcpy(obj->tcp_send_buf.data, buf, len);

    send(client_obj->tcp_fd, obj->tcp_send_buf, len + sizeof(obj->tcp_send_buf.head), 0);

    obj->tcp_send_idx++;
}

int udp_send_pkt(struct ser_net_obj *obj, uint8_t *buf, size_t len)
{
    if(!obj->udp_state)
    {
        func_error("UDP link does not exist.");
        return -1;
    }

    if(len > MAX_PKT_SIZE - sizeof(obj->udp_send_buf.head))
    {
        func_error("data size is too long.");
        return -1;
    }

    obj->tcp_send_buf.head.magic = PKT_MAGIC;
    obj->udp_send_buf.head.idx = htonl(obj->udp_send_idx);
    obj->udp_send_buf.head.len = len;

    memcpy(obj->udp_send_buf.data, buf, len);

    sendto(client_obj->tcp_fd, obj->udp_send_buf, len + sizeof(obj->udp_send_buf.head), 0,
        (struct sockaddr*)&obj->udp_client_addr, sizeof(obj->udp_client_addr));

    obj->udp_send_idx++;
}

static int _tcp_recv_len(struct ser_net_obj *client_obj, uint8_t *buf, int len)
{
    int recvlen;
    int len_tmp = len;
    while(len_tmp != 0)
    {
        recvlen = recv(client_obj->tcp_fd, buf, len_tmp, 0);
        if(recvlen <= 0)
            return -1;
        len_tmp -= recvlen;
    }
    return len;
}

static int _tcp_recv_pkt(struct ser_net_obj *client_obj, struct pkt_buf *buf)
{
    if(_tcp_recv_len(client_obj, (uint8_t *)&buf->head.idx, sizeof(buf->head) - sizeof(uint64_t)) == -1)
        return -1;

    if(buf->head.len <= sizeof(buf->data))
    {
        return _tcp_recv_len(client_obj, buf->data, buf->head.len);
    }else{
        func_error("pkt data len is too large.");
        return 0;
    }
}

static void *_tcp_recv_process(void *arg)
{  
    int ret;
    struct pkt_buf *buf = malloc(struct pkt_buf);
    struct ser_net_obj *client_obj = (struct ser_net_obj *)arg;

    log_info("client connect ip:%s port:%d\n", 
            inet_ntoa(client_obj->tcp_client_addr.sin_addr),
            ntohs(client_obj->tcp_client_addr.sin_port));

    add_ser_net_obj(client_obj);
    while(is_shut_down)
    {
        ret = _tcp_recv_len(client_obj, (uint8_t *)&buf->head.magic, sizeof(uint64_t));
        if(ret != -1)
        {
            if(ntohl(buf->head.magic) == PKT_MAGIC)
            {
                ret = _tcp_recv_pkt(client_obj);
                if(ret == -1)
                    goto END;
                if(ret > 0)
                    on_tcp_recv(client_obj, buf->data, buf->head.len);
            }
        }else{
            break;
        }
    }
 
 END:
    log_info("client closed ip:%s port:%d\n", 
            inet_ntoa(client_obj->tcp_client_addr.sin_addr),
            ntohs(client_obj->tcp_client_addr.sin_port));

    remove_ser_net_obj(client_obj);
    close(client_obj->tcp_fd);
    free(client_obj);
    return NULL;
}

static void *_tcp_listen_process(void *nullptr)
{
    while(is_shut_down)
    {
        struct ser_net_obj *client_obj = calloc(1, sizeof(struct ser_net_obj));
        client_obj->length = sizeof(* client_obj->tcp_client_addr);
        int client_fd = accept(tcp_listen_fd, (struct sockaddr*)&client_obj->tcp_client_addr, &client_obj->length);
        if(client_fd < 0)
        {
            func_error("tcp connect failed.\n");
            continue;
        }
        client_obj->tcp_fd = client_fd;
        pthread_create(&client_obj->tcp_recv_thread, NULL, _tcp_recv_process, (void *)client_obj);  
        pthread_detach(client_obj->tcp_recv_thread);
    }
    return NULL;
}

static void *_udp_recv_process(void *nullptr)
{
    struct ser_net_obj *client_obj;
    struct sockaddr_in client_addr;
    int len = sizeof(client_addr);
    struct pkt_buf *buf = malloc(sizeof(struct pkt_buf));

    while(is_shut_down)
    {
        recvfrom(udp_recv_fd, (uint8_t *)buf, MAX_PKT_SIZE, 0, (struct sockaddr*)&client_addr, &len);
        log_info("udp client ip:%s,port:%d\n",
            inet_ntoa(client_addr.sin_addr),
            ntohs(client_addr.sin_port));

        client_obj = find_client_obj(&client_addr);
        if(client_obj)
        {
            add_udp_conn(client_obj, &client_addr);
            if(buf->head.len <= sizeof(buf->data) && buf->head.len > 0)
                on_udp_recv(client_obj, buf->data, buf->head.len);
        }else{
            log_warning("udp ser_obj is null.");
        }
    }

    free(buf);
    return NULL;
}

int net_server_init(uint16_t port)
{
    int flag = 1;
    struct sockaddr_in tcp_server_sockaddr;
    struct sockaddr_in udp_server_sockaddr;

    tcp_listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    udp_recv_fd = socket(PF_INET, SOCK_DGRAM, 0);

    setsockopt(tcp_listen_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));

    bzero(&tcp_server_sockaddr, sizeof(tcp_server_sockaddr));
    bzero(&udp_server_sockaddr, sizeof(tcp_server_sockaddr));

    tcp_server_sockaddr.sin_family = AF_INET;
    tcp_server_sockaddr.sin_port = htons(port);
    tcp_server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    udp_server_sockaddr.sin_family = AF_INET;
    udp_server_sockaddr.sin_port = htons(port);
    udp_server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(tcp_listen_fd, (struct sockaddr *)&tcp_server_sockaddr, sizeof(tcp_server_sockaddr)) != 0)
    {
        func_error("tcp bind failed.\n");
        return -1;
    }

    if(bind(udp_recv_fd, (struct sockaddr *)&udp_server_sockaddr, sizeof(udp_server_sockaddr)) != 0)
    {
        func_error("udp bind failed.\n");
        goto FAIL1;
    }

    if(listen(tcp_listen_fd, LISTEN_QUEUE) != 0)
    {
        func_error("tcp listen failed.\n");
        goto FAIL2;
    }

    pthread_create(&tcp_listen_thread, NULL, _tcp_listen_process, NULL);  
    pthread_detach(tcp_listen_thread);
    pthread_create(&udp_recv_thread, NULL, _udp_recv_process, NULL);  
    pthread_detach(udp_recv_thread);
    return 0;

FAIL2:
    close(udp_recv_fd);
FAIL1:
    close(tcp_listen_fd);
    return -1;
}

int net_server_release()
{
    is_shut_down = true;
    pthread_join(tcp_listen_thread);
    pthread_join(udp_recv_thread);
    return 0;
}