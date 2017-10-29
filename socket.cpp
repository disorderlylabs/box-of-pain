#include <cstdio>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unordered_map>
#include <string>
#include <cstring>

#include "sockets.h"
#include "helper.h"
#include "tracee.h"
#include "sys.h"

static inline bool sa_eq(const struct sockaddr *a, const struct sockaddr *b)
{
	if(a->sa_family != b->sa_family) return false;
	struct sockaddr_in *ain = (struct sockaddr_in *)a;
	struct sockaddr_in *bin = (struct sockaddr_in *)b;
	return ain->sin_addr.s_addr == bin->sin_addr.s_addr && ain->sin_port == bin->sin_port;
}

class connid {
	public:
		connid(struct sockaddr *caddr, socklen_t clen, struct sockaddr *saddr, socklen_t slen) : peer1(*caddr), peer2(*saddr), p1len(clen), p2len(slen) {}
	bool operator==(const connid &other) const
	{
		return p1len == other.p1len
			&& p2len == other.p2len
			&& sa_eq(&peer1, &other.peer1)
			&& sa_eq(&peer2, &other.peer2);
	}
	struct sockaddr peer1, peer2;
	socklen_t p1len, p2len;

	void debug() const {
		struct sockaddr_in *in = (struct sockaddr_in *)&peer1;
		fprintf(stderr, "%d::%s:%d", p1len, inet_ntoa(in->sin_addr), ntohs(in->sin_port));
		in = (struct sockaddr_in *)&peer2;
		fprintf(stderr, " <=> %d::%s:%d\n", p2len, inet_ntoa(in->sin_addr), ntohs(in->sin_port));
	}
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
			return 0;
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

class sock *sock_assoc(int pid, int _sock)
{
	static long __id = 0;
	class sock *s = new sock;
	s->uuid = __id++;
	s->name = "";
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

void sock_discover_addresses(struct sock *sock)
{
	if(!(sock->flags & S_ASSOC)) return;
	
	struct trace *tracee = find_tracee(sock->frompid);
	if(!(sock->flags & S_ADDR)) {
		struct sockaddr *__X_addr = tracee_alloc_shared_page(tracee, struct sockaddr);
		socklen_t *__X_len = tracee_alloc_shared_page(tracee, socklen_t);
		tracee_set(tracee->pid, (uintptr_t)__X_len, sizeof(struct sockaddr));
		int r = inject_syscall(tracee, SYS_getsockname, sock->sockfd, (long)__X_addr, (long)__X_len);
		if(r == 0) {
			struct sockaddr sa;
			socklen_t salen = GET(socklen_t, tracee->pid, (uintptr_t)__X_len);
			GETOBJ(tracee->pid, (long)__X_addr, &sa);
			if(errno != 0) err(1, "failed to read sockname");
			sock_set_addr(sock, &sa, salen);
		} else {
			fprintf(stderr, "failed to get name in tracee %d\n", tracee->id);
			abort();
		}
		tracee_free_shared_page(tracee);
	}

	if(!(sock->flags & S_PEER)) {
		struct sockaddr *__X_addr = tracee_alloc_shared_page(tracee, struct sockaddr);
		socklen_t *__X_len = tracee_alloc_shared_page(tracee, socklen_t);
		tracee_set(tracee->pid, (uintptr_t)__X_len, sizeof(struct sockaddr));
		int r = inject_syscall(tracee, SYS_getsockname, sock->sockfd, (long)__X_addr, (long)__X_len);
		if(r == 0) {
			struct sockaddr sa;
			socklen_t salen = GET(socklen_t, tracee->pid, (uintptr_t)__X_len);
			GETOBJ(tracee->pid, (long)__X_addr, &sa);
			if(errno != 0) err(1, "failed to read sockname");
			sock_set_peer(sock, &sa, salen);
		} else {
			fprintf(stderr, "failed to get peer in tracee %d\n", tracee->id);
			abort();
		}
		tracee_free_shared_page(tracee);
	}
}

void connection::__established() {
	if(!connside || !accside) return;

	conn->pair = acc;
	acc->pair = conn;
	conn->exit_event->extra_parents.push_back(acc->entry_event);
	acc->exit_event->extra_parents.push_back(conn->entry_event);
}

void connection::set_connside(Sysconnect *sys, sock *s) {
	connside = s;
	conn = sys;
	__established();
}

void connection::set_accside(Sysaccept *sys, sock *s) {
	accside = s;
	acc = sys;
	__established();
}

void connection::write(sock *s, Syscall *sys, size_t len) {
	assert(s == connside || s == accside);
	assert(s != NULL);
	class stream *stream = s == connside ? &ab : &ba;

	stream->txs.push_back((class stream::tx) {.start = stream->wpos, .len = len, .s = sys} );
	stream->wpos += len;
}

std::vector<Syscall *> connection::read(sock *s, size_t len) {
	std::vector<Syscall *> rcs;

	assert(s == connside || s == accside);
	assert(s != NULL);
	class stream *stream = s == connside ? &ba : &ab; //reverse of above

	for(auto tx : stream->txs) {
		if(tx.start < (stream->rpos + len) && stream->rpos < (tx.start + tx.len)) {
			/* overlap! */
			rcs.push_back(tx.s);
		}
	}
	stream->rpos += len;
	return rcs;
}

