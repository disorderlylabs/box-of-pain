#include <cstdio>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <string>

#include "sockets.h"


static std::unordered_map<int, std::unordered_map<int, struct socket> > sockets;

void sock_register(int pid, int sock)
{
	sockets[pid][sock].frompid = pid;
	sockets[pid][sock].sockfd = sock;
	sockets[pid][sock].flags = 0;
}

void sock_close(int pid, int sock)
{
	struct socket *s = sock_lookup(pid, sock);
	if(s) {
		s->flags = s->sockfd = s->frompid = 0;
	}
}

void sock_assoc(int pid, int sock, std::string name, struct sockaddr *addr, socklen_t addrlen)
{
	sockets[pid][sock] = (struct socket) {name : name, addr : *addr, addrlen : addrlen, flags : S_ASSOC, sockfd : sock, frompid : pid};
	fprintf(stderr, "Assoc sock (%d,%d) to %s\n", pid, sock, sock_name(&sockets[pid][sock]).c_str());
}

struct socket *sock_lookup(int pid, int sock)
{
	if(sockets.find(pid) != sockets.end()) {
		if(sockets[pid].find(sock) != sockets[pid].end()) {
			if(sockets[pid][sock].frompid > 0)
				return &sockets[pid][sock];
			else
				return NULL;
		}
	}
	return NULL;
}

std::string sock_name(struct socket *s)
{
	if(s->name != "") {
		return s->name;
	}
	struct sockaddr_in *in = (struct sockaddr_in *)(&s->addr);
	std::string ret = std::to_string(s->frompid);
	ret += "::";
	ret += inet_ntoa(in->sin_addr);
	ret += ":";
	ret += std::to_string(ntohs(in->sin_port));
	return ret;
}

