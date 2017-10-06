#pragma once
#include <string>
#include <sys/socket.h>
#define S_ASSOC 1
class sock {
	public:
	std::string name;
	struct sockaddr addr;
	socklen_t addrlen;
	int flags;
	int sockfd;
	int frompid;
	sock() { }
};


struct sock *sock_lookup(int pid, int sock);
void sock_assoc(int pid, int sock, std::string name, struct sockaddr *addr, socklen_t addrlen);
std::string sock_name(struct sock *s);
void sock_close(int pid, int sock);

