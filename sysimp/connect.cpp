#include "sys.h"

void Sysconnect::start()
{
	struct sockaddr addr;
	socklen_t len;
	int sockfd = params[0];
	GETOBJ(fromtid, params[1], &addr);
	/* TODO: handle other types of address families. */
	if(addr.sa_family != AF_INET)
		return;
	len = params[2];
	sock = sock_assoc(&current_run, thread, sockfd);
	sock_set_peer(sock, &addr, len);
}

void Sysconnect::finish()
{
	if(!sock)
		return;
	sock_discover_addresses(sock);
	/* okay, now look up the connection */
	sock->conn =
	  conn_lookup(&current_run, &sock->addr, sock->addrlen, &sock->peer, sock->peerlen, true);
	sock->conn->set_connside(this, sock);
}
