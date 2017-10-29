#pragma once
#include <string>
#include <sys/socket.h>
#include <cassert>
#include <vector>
#define S_ASSOC 1
#define S_PEER  2
#define S_ADDR  4

class connection;
class Syscall;
class Sysconnect;
class Sysaccept;
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
	Sysconnect *conn = NULL;
	Sysaccept *acc = NULL;

	connection() {}

	std::vector<Syscall *> read(sock *s, size_t len);
	void write(sock *s, Syscall *sys, size_t len);
	void set_accside(Sysaccept *sys, sock *s);
	void set_connside(Sysconnect *sys, sock *s);
	void __established();
};

#define ISASSOC(s) ((s)->flags & S_ASSOC)

class sock *sock_lookup(int pid, int sock);
class sock *sock_assoc(int pid, int sock);
std::string sock_name(class sock *s);
std::string sock_name_short(class sock *s);
void sock_close(int pid, int sock);
void sock_set_peer(sock *s, struct sockaddr *peer, socklen_t plen);
void sock_set_addr(sock *s, struct sockaddr *addr, socklen_t len);
void sock_discover_addresses(struct sock *sock);

class connection *conn_lookup(struct sockaddr *caddr, socklen_t clen,
		struct sockaddr *saddr, socklen_t slen, bool create);

