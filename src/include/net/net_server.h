#ifndef __NET_SERVER_H__
#define __NET_SERVER_H__

struct server_net {

}

struct server_client {
	void *priv;
};

struct server_net *server_net_init();

#endif