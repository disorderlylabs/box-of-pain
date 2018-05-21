#include "sys.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
void Sysaccept::start() {
	int fd = params[0];
	serversock = sock_lookup(&current_run, frompid, fd);
}

void Sysaccept::finish() {
	int sockfd = retval;
	if(sockfd >= 0) {
		len = GET(socklen_t, fromtid, params[2]);
		GETOBJ(fromtid, params[1], &addr);
		sock = sock_assoc(&current_run, thread, sockfd);
		/* TODO: check if tracee called with NULL arguments */
		sock_set_peer(sock, &addr, len);

		sock_discover_addresses(sock);
		sock->conn = conn_lookup(&current_run, &sock->peer, sock->peerlen, &sock->addr, sock->addrlen, true);
		sock->conn->set_accside(this, sock);
	}
}

