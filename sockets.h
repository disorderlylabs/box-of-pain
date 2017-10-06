#pragma once
#include <string>
#include <sys/socket.h>
#define S_ASSOC 1
class sock {
	public:
		long uuid;
		std::string name;
		struct sockaddr addr;
		socklen_t addrlen;
		int flags;
		int sockfd;
		int frompid;
		class sock *conn_pair = NULL;
		sock() { }
};

#define ISASSOC(s) ((s)->flags & S_ASSOC)

class sock *sock_lookup(int pid, int sock);
class sock *sock_assoc(int pid, int sock, std::string name, struct sockaddr *addr, socklen_t addrlen);
std::string sock_name(class sock *s);
void sock_close(int pid, int sock);

