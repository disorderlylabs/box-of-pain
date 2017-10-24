#pragma once
#include <string>
#include <sys/socket.h>
#include <cassert>
#include <vector>
#define S_ASSOC 1


class connection;
class sock {
	public:
		long uuid;
		std::string name;
		struct sockaddr addr;
		socklen_t addrlen;
		int flags;
		int sockfd;
		int frompid;
		struct connection *conn;
		sock() { }
};

class Syscall;
class connection {
	class sock *a, *b;

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
	connection(sock *a, sock *b) : a(a), b(b) {}
	void write(sock *s, Syscall *sys, size_t len) {
		assert(s == a || s == b);
		class stream *stream = s == a ? &ab : &ba;
		
		stream->txs.push_back((class stream::tx) {.start = stream->wpos, .len = len, .s = sys} );
		stream->wpos += len;
	}

	std::vector<Syscall *> read(sock *s, size_t len) {
		std::vector<Syscall *> rcs;

		assert(s == a || s == b);
		class stream *stream = s == a ? &ba : &ab; //reverse of above

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
class sock *sock_assoc(int pid, int sock, std::string name, struct sockaddr *addr, socklen_t addrlen);
std::string sock_name(class sock *s);
void sock_close(int pid, int sock);

