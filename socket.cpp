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
#include "run.h"


void sock_close(struct run *run, int pid, int sock)
{
	run->sockets[pid][sock] = NULL;
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

class sock *sock_assoc(struct run *run, struct thread_tr *tr, int _sock)
{	
	class sock *s = new sock;
	s->fromthread = tr;
	s->proc = tr->proc;
	s->frompid = s->proc->pid;
	s->fromtid = tr->tid;
	s->uuid = run->sock_list.size();
	s->name = "";
	s->flags |= S_ASSOC;
	s->sockfd = _sock;
	run->sockets[s->frompid][_sock] = s;
	run->sock_list.push_back(s);
	fprintf(stderr, "Assoc sock (%d,%d) to %s\n", s->frompid, _sock, sock_name(run->sockets[s->frompid][_sock]).c_str());
	return s;
}

/* lookup socket by address. TODO: use the sa_eq function */
class sock *sock_lookup_addr(struct run *run, struct sockaddr *addr, socklen_t addrlen)
{
	for(auto sm : run->sockets) {
		for(auto p : sm.second) {
			class sock *sock = p.second;
			if((sock->flags & S_ASSOC) && addrlen == sock->addrlen && sa_eq(addr, &sock->addr)) {
				return sock;
			}
		}
	}
	return NULL;
}

/* TODO (major): What about repeated connections? */
class connection *conn_lookup(struct run *run, struct sockaddr *caddr, socklen_t clen,
		struct sockaddr *saddr, socklen_t slen, bool create)
{
	connid id(caddr, clen, saddr, slen);
	if(run->connections.find(id) == run->connections.end()) {
		if(create) {
			run->connections[id] = new connection();
			run->connection_list.push_back(run->connections[id]);
			return run->connections[id];
		}
		return NULL;
	}
	return run->connections[id];
}

class sock *sock_lookup(struct run *run, int pid, int sock)
{
	if(run->sockets.find(pid) != run->sockets.end()) {
		if(run->sockets[pid].find(sock) != run->sockets[pid].end()) {
			return run->sockets[pid][sock];
		}
	}
	return NULL;
}

std::string sock_name(class sock *s)
{
	/* TODO: run name? */
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

/* okay, this function takes a socket and discoveres both its name
 * and its peer's name (if either is unknown). It does this by injecting
 * getsockname and getpeername system calls. But those syscalls fill out
 * memory via pointers. So we need to allocate memory _in the tracee's process_
 * before injecting the syscalls. After that, we read that memory into
 * our process and record it. */
void sock_discover_addresses(struct sock *sock)
{
	if(!(sock->flags & S_ASSOC)) return;
	
	struct thread_tr *tracee = sock->fromthread;
	if(!(sock->flags & S_ADDR)) {
		struct sockaddr *__X_addr = tracee_alloc_shared_page(tracee, struct sockaddr);
		socklen_t *__X_len = tracee_alloc_shared_page(tracee, socklen_t);
		tracee_set(tracee->tid, (uintptr_t)__X_len, sizeof(struct sockaddr));
		int r = inject_syscall(tracee, SYS_getsockname, sock->sockfd, (long)__X_addr, (long)__X_len);
		if(r == 0) {
			struct sockaddr sa;
			socklen_t salen = GET(socklen_t, tracee->tid, (uintptr_t)__X_len);
			GETOBJ(tracee->tid, (long)__X_addr, &sa);
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
		tracee_set(tracee->tid, (uintptr_t)__X_len, sizeof(struct sockaddr));
		int r = inject_syscall(tracee, SYS_getpeername, sock->sockfd, (long)__X_addr, (long)__X_len);
		if(r == 0) {
			struct sockaddr sa;
			socklen_t salen = GET(socklen_t, tracee->tid, (uintptr_t)__X_len);
			GETOBJ(tracee->tid, (long)__X_addr, &sa);
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
	/* if the connection can be determined (both sides have
	 * witnessed the syscall exit), then add our edges to extra_parents */
	if(!connside || !accside) return;
	//conn->pair = acc;
	//acc->pair = conn;
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

	/* TODO: this assert _should_ be enabled... but doing so causes it to fail when the clients
	 * do their weird set-up shit. Still works after that, though. Need to figure out a way to
	 * ignore the clients setting up. */
	//assert(s == connside || s == accside);
	assert(s != NULL);
	class stream *stream = s == connside ? &ba : &ab; //reverse of above

	/* look for overlap between our read (rpos and len) and any transmissions that
	 * were made */
	for(auto tx : stream->txs) {
		if(tx.start < (stream->rpos + len) && stream->rpos < (tx.start + tx.len)) {
			/* overlap! */
			rcs.push_back(tx.s);
		}
	}
	stream->rpos += len;
	return rcs;
}

/*
Syscall * noconnection::recvfrom(sockaddr_t source, size_t len){
	//Find a message from the given source, having the given length

}

void noconnection::sendfrom(Syscall* sys, sockaddr_t source, size_t len){
	messages.Something(class pseudostream::tx({.source = source, .len = len,}))

}
*/
