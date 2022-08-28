#ifndef __RT_NET_SOCKET_H__
#define __RT_NET_SOCKET_H__
#include <stdint.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PKT_MAGIC 0xABCDEF1234567890

#define MAX_PKT_SIZE 1472

struct pkt_head{
    uint64_t magic;
    uint64_t idx;
    uint16_t data_len;
};

struct pkt_buf
{
    struct pkt_head head;
    uint8_t data[MAX_PKT_SIZE - sizeof(struct pkt_head)];
};

enum RT_PIPLE_CTRL
{
    OPEN_PIPLE,
    OPEN_REP_ERROR,
    OPEN_REP_SUCCESS,
    BIND_PIPLE,
    BIND_REP_ERROR,
    BIND_REP_SUCCESS,
    PIPLE_DATA,
    CLOSE_PIPLE,
    CLOSE_PIPLE_REP_ERROR,
    CLOSE_PIPLE_REP_SUCCESS,
};

enum RT_PIPLE_TYPE
{
    RELIABLE_PIPLE,
    RT_IMPORTANT_PIPLE,
    RT_UNIMPORTANT_PIPLE,
};

struct piple_pkt
{
    struct PIPLE_PKT_HEAD {
        enum RT_PIPLE_CTRL cmd;
        uint16_t piple_id;
        enum RT_PIPLE_TYPE piple_type;
        uint16_t data_len;
    }head;
    uint8_t data[MAX_PKT_SIZE - sizeof(struct pkt_head) - sizeof(struct PIPLE_PKT_HEAD)];
};

enum PIPLE_STATE
{
    PIPLE_OPENING,
    PIPLE_OPENED,
    PIPLE_ERROR,
    PIPLE_CLOSED,
};

struct rt_net_piple
{
    uint16_t id;
    enum PIPLE_STATE state;
    enum RT_PIPLE_TYPE piple_type;
    void *(* recv_callback)(int piple_id, uint8_t *buf, uint16_t len);
    struct piple_pkt send_pkt;
    pthread_mutex_t send_lock;
};

#define PIPLE_MAX 128
struct rt_net_client
{
    struct sockaddr_in tcp_client_addr;
    struct sockaddr_in udp_client_addr;

    struct sockaddr_in udp_recv_addr;

    struct pkt_buf tcp_send_buf;
    struct pkt_buf udp_send_buf;

    uint64_t tcp_send_idx;
    uint64_t tcp_recv_idx;
    uint64_t udp_send_idx;
    uint64_t udp_recv_idx;

    int tcp_fd;
    int udp_fd;

	bool close_flag;
    bool udp_state;
    
	pthread_t udp_recv_thread;
	pthread_t tcp_recv_thread;

	uint16_t piple_idx;
    pthread_cond_t msg_signal;
    pthread_mutex_t piple_open_lock;
	pthread_mutex_t accept_piple_lock;
	struct rt_net_piple *piples[PIPLE_MAX];
};

#define CLIENT_MAX 10240
struct rt_net_server
{
    pthread_mutex_t accept_lock;

    struct sockaddr_in tcp_server_addr;
    struct rt_net_client udp_client;

    int tcp_listen_fd;

	uint32_t client_idx;
	struct rt_net_client *clients[CLIENT_MAX];
};

int udp_send_pkt(struct rt_net_client *client, const uint8_t *buf, uint16_t len);
int tcp_send_pkt(struct rt_net_client *client, const uint8_t *buf, uint16_t len);
int tcp_accept(struct rt_net_server *server, struct rt_net_client *client);
int net_server_init(struct rt_net_server *server, char *addr, uint16_t port);
int net_server_release(struct rt_net_server *server);
int net_client_init(struct rt_net_client *client, char *addr, uint16_t port);
int net_client_release(struct rt_net_client *client);
#endif