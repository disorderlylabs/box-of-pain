
#include "sys.h"


void Sysconnect::start() {
	int sockfd = params[0];
	GETOBJ(frompid, params[1], &addr);
	len = params[2];
	sock = sock_assoc(frompid, sockfd, "");
	sock_set_peer(sock, &addr, len);
} 

void Sysconnect::finish() { 
}

