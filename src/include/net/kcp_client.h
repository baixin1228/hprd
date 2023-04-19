#ifndef __KCP_CLIENT_H__
#define __KCP_CLIENT_H__

int kcp_client_init(char *ip, uint16_t port, uint32_t kcp_id);
int kcp_client_send_pkt(char *buf, size_t len);
bool kcp_client_enable();
void kcp_client_release();

#endif