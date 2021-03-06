#include <sys.h>
#include <tracee.h>

/*TODO everything. Sendto is probably easier*/

void Sysrecvfrom::start()
{
	int sockfd;
	// void *buffer;
	// size_t length;
	// int flags;
	struct sockaddr *addr;
	// socklen_t *addrlen;

	sockfd = params[0];
	// buffer = (void *)params[1];
	// length = params[2];
	// flags = params[3];
	addr = (struct sockaddr *)params[4];
	// addrlen = (socklen_t *)params[5];

	if(addr == NULL) {
		sock = sock_lookup(&current_run, frompid, sockfd);
		if(!sock) {
			return;
		}
		if(options.log_sockets)
			fprintf(
			  stderr, "[%d]: SOCKET %-26s RECVFROM enter\n", thread->id, sock_name(sock).c_str());
	} else {
		sock = sock_lookup(&current_run, frompid, sockfd);
		if(!sock) {
			return;
		}
		if(options.log_sockets)
			fprintf(stderr,
			  "[%d]: SOCKET %-26s UNCONNECTED RECVFROM enter\n",
			  thread->id,
			  sock_name(sock).c_str());
		sock = NULL;
	}
}

void Sysrecvfrom::finish()
{
	if(!sock) {
		return;
	}
	fprintf(stderr, "[%d]: SOCKET %-26s RECVFROM  retur\n", thread->id, sock_name(sock).c_str());

	if(sock->conn && retval > 0) {
		std::vector<Syscall *> rcs = sock->conn->read(sock, retval);
		for(auto wsys : rcs) {
			exit_event->extra_parents.push_back(wsys->entry_event);
		}
	}
}
