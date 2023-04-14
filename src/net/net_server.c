#include "tcp_server.h"

struct server_net *ser_net;
int server_net_init(struct server_net *ser)
{

	tcp_server_init("0.0.0.0", 9999);

	return 0;
}

void server_net_release(struct server_net * ser_net)
{
}
