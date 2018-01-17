#include "sys.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
void Sysaccept::start() {
	int fd = params[0];
	serversock = sock_lookup(frompid, fd);
}

void Sysaccept::finish() {
	int sockfd = retval;
	if(sockfd >= 0) {
		len = GET(socklen_t, frompid, params[2]);
		GETOBJ(frompid, params[1], &addr);
		sock = sock_assoc(frompid, sockfd);
		/* TODO: check if tracee called with NULL arguments */
		sock_set_peer(sock, &addr, len);

		sock_discover_addresses(sock);
		sock->conn = conn_lookup(&sock->peer, sock->peerlen, &sock->addr, sock->addrlen, true);
		sock->conn->set_accside(this, sock);
	}
}

