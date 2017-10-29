#pragma once
#include <string>
#include <sys/socket.h>
#include <cassert>
#include <vector>
#define S_ASSOC 1
#define S_PEER  2
#define S_ADDR  4

class connection;
class sock {
	public:
		long uuid;
		std::string name;
		struct sockaddr addr, peer;
		socklen_t addrlen, peerlen;
		int flags = 0;
		int sockfd;
		int frompid;
		struct connection *conn;
		sock() { }
};

class Syscall;
class connection {
	class stream {
		public:
		class tx {
			public:
				size_t start;
				size_t len;
				Syscall *s;
		};
		size_t wpos=0, rpos=0;
		std::vector<tx> txs;
	} ab, ba;

	public:
	class sock *connside = NULL, *accside = NULL;

	connection() {}

	void write(sock *s, Syscall *sys, size_t len) {
		assert(s == connside || s == accside);
		assert(s != NULL);
		class stream *stream = s == connside ? &ab : &ba;
		
		stream->txs.push_back((class stream::tx) {.start = stream->wpos, .len = len, .s = sys} );
		stream->wpos += len;
	}

	std::vector<Syscall *> read(sock *s, size_t len) {
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
};

#define ISASSOC(s) ((s)->flags & S_ASSOC)

class sock *sock_lookup(int pid, int sock);
class sock *sock_assoc(int pid, int sock, std::string name);
std::string sock_name(class sock *s);
std::string sock_name_short(class sock *s);
void sock_close(int pid, int sock);
void sock_set_peer(sock *s, struct sockaddr *peer, socklen_t plen);
void sock_set_addr(sock *s, struct sockaddr *addr, socklen_t len);

class connection *conn_lookup(struct sockaddr *caddr, socklen_t clen,
		struct sockaddr *saddr, socklen_t slen, bool create);

