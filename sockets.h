#pragma once
#include <arpa/inet.h>
#include <cassert>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <vector>

#define S_ASSOC 1
#define S_PEER 2
#define S_ADDR 4

/* test two sockaddrs for equality */
static inline bool sa_eq(const struct sockaddr *a, const struct sockaddr *b)
{
	if(a->sa_family != b->sa_family)
		return false;
	struct sockaddr_in *ain = (struct sockaddr_in *)a;
	struct sockaddr_in *bin = (struct sockaddr_in *)b;
	return ain->sin_addr.s_addr == bin->sin_addr.s_addr && ain->sin_port == bin->sin_port;
}

class connection;
class Syscall;
class Sysconnect;
class Sysaccept;
class sock
{
  public:
	long uuid;
	std::string name;
	struct sockaddr addr, peer;
	socklen_t addrlen, peerlen;
	int flags = 0;
	int sockfd;
	int frompid; // The process within which the socket exists
	struct proc_tr *proc;
	struct thread_tr *fromthread;
	int fromtid; // The thread which created the socket. Should only be used during socket creation
	struct connection *conn;
	struct noconnection *nconn;
	sock()
	{
	}

	bool approx_eq(sock *other)
	{
		fprintf(stderr, ":: %d %d\n", flags, other->flags);
		//	const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
		char buf[128];
		struct sockaddr_in *ta = (struct sockaddr_in *)&addr;
		struct sockaddr_in *oa = (struct sockaddr_in *)&other->addr;
		struct sockaddr_in *tp = (struct sockaddr_in *)&peer;
		struct sockaddr_in *op = (struct sockaddr_in *)&other->peer;
		inet_ntop(AF_INET, &ta->sin_addr, buf, sizeof(buf));
		fprintf(stderr, ":: %s:%d", buf, ntohs(ta->sin_port));
		inet_ntop(AF_INET, &oa->sin_addr, buf, sizeof(buf));
		fprintf(stderr, " %s:%d", buf, ntohs(oa->sin_port));

		inet_ntop(AF_INET, &tp->sin_addr, buf, sizeof(buf));
		fprintf(stderr, " ::::: %s:%d", buf, ntohs(tp->sin_port));
		inet_ntop(AF_INET, &op->sin_addr, buf, sizeof(buf));
		fprintf(stderr, " %s:%d", buf, ntohs(op->sin_port));
		fprintf(stderr, "\n");
		return flags == other->flags && sa_eq(&addr, &other->addr) && sa_eq(&peer, &other->peer);
	}

	void serialize(FILE *);
};

class connection
{
	/* streams are used to track the full duplex mode
	 * TCP connection. Two stream, one for each "direction".
	 * The stream keeps track of a list of transmissions,
	 * so that we can match up reads to writes. This is done
	 * by matching up windows into the tcp stream. The read
	 * checks to see if read from any of the bytes that were
	 * written to by any tranmission that it knows about.
	 * We can then know which write syscalls contributed to
	 * a particlar read return */
	class stream
	{
	  public:
		class tx
		{
		  public:
			size_t start;
			size_t len;
			Syscall *s;
		};
		size_t wpos = 0, rpos = 0;
		std::vector<tx> txs;
	} ab, ba;

  public:
	int uuid;
	class sock *connside = NULL, *accside = NULL;
	Sysconnect *conn = NULL;
	Sysaccept *acc = NULL;

	connection()
	{
	}
	void serialize(FILE *);

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

class sock *sock_lookup(struct run *, int pid, int sock);
class sock *sock_assoc(struct run *, struct thread_tr *tr, int sock);
std::string sock_name(class sock *s);
std::string sock_name_short(class sock *s);
void sock_close(struct run *, int pid, int sock);
void sock_set_peer(sock *s, struct sockaddr *peer, socklen_t plen);
void sock_set_addr(sock *s, struct sockaddr *addr, socklen_t len);
void sock_discover_addresses(struct sock *sock);

static inline void serialize_sockaddr(FILE *f, struct sockaddr *addr, socklen_t len)
{
	struct sockaddr_in *inaddr = (struct sockaddr_in *)addr;
	// fprintf(f, "sockaddr_in (%d)%d:%s", len, inaddr->sin_port, inet_ntoa(inaddr->sin_addr));
}

/* this is a simple hash function. */
static inline unsigned long djb2hash(unsigned char *str, size_t len)
{
	unsigned long hash = 5381;
	int c;

	while(len--) {
		c = *str++;
		hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
	}

	return hash;
}

class connection *conn_lookup(struct run *,
  struct sockaddr *caddr,
  socklen_t clen,
  struct sockaddr *saddr,
  socklen_t slen,
  bool create);

/* this is a sockaddr_in pair that identifies a connection */
class connid
{
  public:
	connid(struct sockaddr *caddr, socklen_t clen, struct sockaddr *saddr, socklen_t slen)
	  : peer1(*caddr)
	  , peer2(*saddr)
	  , p1len(clen)
	  , p2len(slen)
	{
	}
	bool operator==(const connid &other) const
	{
		return p1len == other.p1len && p2len == other.p2len && sa_eq(&peer1, &other.peer1)
		       && sa_eq(&peer2, &other.peer2);
	}
	struct sockaddr peer1, peer2;
	socklen_t p1len, p2len;

	void debug() const
	{
		struct sockaddr_in *in = (struct sockaddr_in *)&peer1;
		fprintf(stderr, "%d::%s:%d", p1len, inet_ntoa(in->sin_addr), ntohs(in->sin_port));
		in = (struct sockaddr_in *)&peer2;
		fprintf(stderr, " <=> %d::%s:%d\n", p2len, inet_ntoa(in->sin_addr), ntohs(in->sin_port));
	}
};

namespace std
{
/* implement hash for connid. Man, I miss Rust syntax sometimes... */
template<>
struct hash<connid> {
	size_t operator()(const connid &x) const
	{
		return 0; /* TODO: remove this. There's a bug in the below hash function, but it's the
		general idea. If we try to use the code below, it doesn't work. But it's also not well
		tested, since I developed this using the 'world's best hash function': return 0. */
		return (((djb2hash((unsigned char *)&x.peer1, x.p1len)
		           ^ (djb2hash((unsigned char *)&x.peer2, x.p2len) << 1))
		          >> 1)
		         ^ (hash<socklen_t>()(x.p1len) << 1) >> 1)
		       ^ (hash<socklen_t>()(x.p2len) << 1);
	}
};
}
