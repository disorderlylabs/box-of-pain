#include <cstdio>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <string>

#include "sockets.h"


static std::unordered_map<int, std::unordered_map<int, class sock *> > sockets;

void sock_close(int pid, int sock)
{
	sockets[pid][sock] = NULL;
}

class sock *sock_assoc(int pid, int _sock, std::string name, struct sockaddr *addr, socklen_t addrlen)
{
	static long __id = 0;
	class sock *s = new sock;
	s->uuid = __id++;
	s->name = name;
	s->addr = *addr;
	s->addrlen = addrlen;
	s->flags = S_ASSOC;
	s->sockfd = _sock;
	s->frompid = pid;
	sockets[pid][_sock] = s;
	fprintf(stderr, "Assoc sock (%d,%d) to %s\n", pid, _sock, sock_name(sockets[pid][_sock]).c_str());
	return s;
}

class sock *sock_lookup(int pid, int sock)
{
	if(sockets.find(pid) != sockets.end()) {
		if(sockets[pid].find(sock) != sockets[pid].end()) {
			return sockets[pid][sock];
		}
	}
	return NULL;
}

std::string sock_name(class sock *s)
{
	if(s->name != "") {
		return s->name;
	}
	struct sockaddr_in *in = (struct sockaddr_in *)(&s->addr);
	std::string ret = std::to_string(s->frompid);
	ret += "::";
	ret += std::to_string(s->sockfd);
	ret += "::";
	ret += std::to_string(s->uuid);
	ret += "::";
	ret += inet_ntoa(in->sin_addr);
	ret += ":";
	ret += std::to_string(ntohs(in->sin_port));
	return ret;
}

