#include "sys.h"

#include <sys/socket.h>
#include <netinet/in.h>
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
		sock = sock_assoc(frompid, sockfd, "", &addr, len);

		class sock *c = this->get_socket();
		class sock *s = this->serversock;
		if(c && s) {
			for(auto sys : syscall_list) {
				auto scconn = dynamic_cast<class Sysconnect *>(sys);
				if(scconn != nullptr) {
					/* TODO: we should look for and track connections
					 * as they happen */
					struct sock *cl_sock = scconn->get_socket();
					if(cl_sock && ISASSOC(cl_sock) && ISASSOC(s) && ISASSOC(c)) {
						c->conn = cl_sock->conn = new connection(cl_sock, c);
						struct sockaddr_in *in_server_lis
							= (struct sockaddr_in *)&s->addr;
						struct sockaddr_in *in_server_cli
							= (struct sockaddr_in *)&c->addr;
						struct sockaddr_in *in_client_soc
							= (struct sockaddr_in *)&cl_sock->addr;

						if(in_client_soc->sin_port == in_server_lis->sin_port
								&& !memcmp(&in_client_soc->sin_addr,
									&in_server_cli->sin_addr,
									sizeof(in_server_cli->sin_addr))) {
							this->pair = scconn;
							scconn->pair = this;
							scconn->exit_event->extra_parents.push_back(this->entry_event);
							this->exit_event->extra_parents.push_back(scconn->entry_event);
						}
					}
				}
			}
		}

	}
}
