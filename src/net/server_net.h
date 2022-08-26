#ifndef __SERVER_NET_H__
#define __SERVER_NET_H__
#include <stdint.h>
#include <stdbool.h>

#define PKT_MAGIC 0xABCDEF1234567890

#define MAX_PKT_SIZE 1472

struct pkt_buf
{
    struct PKT_HEAD{
        uint64_t magic;
        uint64_t idx;
        uint16_t len;
    } head;
    uint8_t data[MAX_PKT_SIZE - sizeof(struct PKT_HEAD)];
};

struct ser_net_obj
{
    int tcp_fd;
    socklen_t length;
    struct sockaddr_in tcp_client_addr;
    struct sockaddr_in udp_client_addr;

    pthread_t tcp_recv_thread;
    pthread_t tcp_send_thread;

    uint64_t tcp_send_idx;
    uint64_t tcp_recv_idx;
    uint64_t udp_send_idx;
    uint64_t udp_recv_idx;

    struct pkt_buf tcp_send_buf;
    struct pkt_buf udp_send_buf;

    bool udp_state;
    bool exit_flag;
    struct list_node node;
}

int add_ser_net_obj(struct ser_net_obj *obj);
struct ser_net_obj *find_client_obj(struct sockaddr_in *client_addr);
int add_udp_conn(struct ser_net_obj *obj, struct sockaddr_in *client_addr);

int on_udp_recv(struct ser_net_obj *obj, uint8_t *buf, size_t len);
int on_tcp_recv(struct ser_net_obj *obj, uint8_t *buf, size_t len);
#endif