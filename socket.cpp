#include <cstdio>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <string>
#include <cstring>

#include "sockets.h"

class connid {
	public:
		connid(struct sockaddr *caddr, socklen_t clen, struct sockaddr *saddr, socklen_t slen) : peer1(*caddr), peer2(*saddr), p1len(clen), p2len(slen) {}
	bool operator==(const connid &other) const
	{
		return p1len == other.p1len
			&& p2len == other.p2len
			&& !memcmp(&peer1, &other.peer1, p1len)
			&& !memcmp(&peer2, &other.peer2, p2len);
	}
	struct sockaddr peer1, peer2;
	socklen_t p1len, p2len;
};

unsigned long
djb2hash(unsigned char *str, size_t len)
{
    unsigned long hash = 5381;
    int c;

    while (len--) {
        c = *str++;
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}

    return hash;
}

namespace std {
	template <> struct hash<connid>
	{
		size_t operator()(const connid &x) const
		{
			return (((djb2hash((unsigned char *)&x.peer1, x.p1len)
						^ (djb2hash((unsigned char *)&x.peer2, x.p2len) << 1)) >> 1)
						^ (hash<socklen_t>()(x.p1len) << 1) >> 1)
						^ (hash<socklen_t>()(x.p2len) << 1);
		}
	};
}

static std::unordered_map<int, std::unordered_map<int, class sock *> > sockets;

static std::unordered_map<connid, connection *> connections;

void sock_close(int pid, int sock)
{
	sockets[pid][sock] = NULL;
}

void sock_set_peer(sock *s, struct sockaddr *peer, socklen_t plen)
{
	s->peer = *peer;
	s->peerlen = plen;
	s->flags |= S_PEER;
}

void sock_set_addr(sock *s, struct sockaddr *addr, socklen_t len)
{
	s->addr = *addr;
	s->addrlen = len;
	s->flags |= S_ADDR;
}

class sock *sock_assoc(int pid, int _sock, std::string name)
{
	static long __id = 0;
	class sock *s = new sock;
	s->uuid = __id++;
	s->name = name;
	s->flags |= S_ASSOC;
	s->sockfd = _sock;
	s->frompid = pid;
	sockets[pid][_sock] = s;
	fprintf(stderr, "Assoc sock (%d,%d) to %s\n", pid, _sock, sock_name(sockets[pid][_sock]).c_str());
	return s;
}

class sock *sock_lookup_addr(struct sockaddr *addr, socklen_t addrlen)
{
	for(auto sm : sockets) {
		for(auto p : sm.second) {
			class sock *sock = p.second;
			if((sock->flags & S_ASSOC) && addrlen == sock->addrlen && !memcmp(addr, &sock->addr, addrlen)) {
				return sock;
			}
		}
	}
	return NULL;
}

class connection *conn_lookup(struct sockaddr *caddr, socklen_t clen,
		struct sockaddr *saddr, socklen_t slen, bool create)
{
	connid id(caddr, clen, saddr, slen);
	if(connections.find(id) == connections.end()) {
		return create ? connections[id] = new connection() : NULL;
	}
	return connections[id];
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
	std::string ret = std::to_string(s->frompid);
	ret += "::";
	ret += std::to_string(s->sockfd);
	ret += "::";
	ret += std::to_string(s->uuid);
	ret += "::";
	if(s->flags & S_ADDR) {
		struct sockaddr_in *in = (struct sockaddr_in *)(&s->addr);
		ret += inet_ntoa(in->sin_addr);
		ret += ":";
		ret += std::to_string(ntohs(in->sin_port));
	}
	if(s->flags & S_PEER) {
		struct sockaddr_in *in = (struct sockaddr_in *)(&s->peer);
		ret += "->";
		ret += inet_ntoa(in->sin_addr);
		ret += ":";
		ret += std::to_string(ntohs(in->sin_port));
	}
	return ret;
}

std::string sock_name_short(class sock *s)
{
	if(s->name != "") {
		return s->name;
	}
	std::string ret = "";
	if(s->flags & S_ADDR) {
		struct sockaddr_in *in = (struct sockaddr_in *)(&s->addr);
		ret += inet_ntoa(in->sin_addr);
		ret += ":";
		ret += std::to_string(ntohs(in->sin_port));
	}
	if(s->flags & S_PEER) {
		struct sockaddr_in *in = (struct sockaddr_in *)(&s->peer);
		ret += "->";
		ret += inet_ntoa(in->sin_addr);
		ret += ":";
		ret += std::to_string(ntohs(in->sin_port));
	}
	return ret;
}

