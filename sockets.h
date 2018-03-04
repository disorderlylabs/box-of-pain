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
		int frompid; //The process within which the socket exists
		struct proc_tr *proc; 
		int fromtid; //The thread which created the socket. Should only be used during socket creation
		struct connection *conn;
		struct noconnection *nconn;
		sock() { }
};

class connection {
	/* streams are used to track the full duplex mode
	 * TCP connection. Two stream, one for each "direction".
	 * The stream keeps track of a list of transmissions,
	 * so that we can match up reads to writes. This is done
	 * by matching up windows into the tcp stream. The read
	 * checks to see if read from any of the bytes that were
	 * written to by any tranmission that it knows about.
	 * We can then know which write syscalls contributed to
	 * a particlar read return */
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
/*
class noconnection{
	A UDP socket does not have a connection,
		we must give it something else to work with
		A transmission can be characterized by a source and length
	
	class pseudostream {
		class tx {
			public:
				size_t len;
				sockaddr_t source;
				Syscall *s;
		}

	} messages;

	public:
		class sock *sock = NULL;

		noconnection() {}

		Syscall * recvfrom(sockaddr_t source, size_t len);
		void sendfrom(Syscall *sys, sockaddr_t source, size_t len);

}

*/
#define ISASSOC(s) ((s)->flags & S_ASSOC)

class sock *sock_lookup(int pid, int sock);
class sock *sock_assoc(struct thread_tr * tr, int sock);
std::string sock_name(class sock *s);
std::string sock_name_short(class sock *s);
void sock_close(int pid, int sock);
void sock_set_peer(sock *s, struct sockaddr *peer, socklen_t plen);
void sock_set_addr(sock *s, struct sockaddr *addr, socklen_t len);
void sock_discover_addresses(struct sock *sock);

class connection *conn_lookup(struct sockaddr *caddr, socklen_t clen,
		struct sockaddr *saddr, socklen_t slen, bool create);

