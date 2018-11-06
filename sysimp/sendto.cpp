#include <sys.h>
#include <tracee.h>

void Syssendto::start()
{
	int sockfd;
	void *buffer;
	size_t length;
	int flags;
	struct sockaddr *dest;
	socklen_t dest_len;

	sockfd = params[0];
	buffer = (void *)params[1];
	length = params[2];
	flags = params[3];
	GETOBJ(frompid, params[4], &dest);
	dest_len = (socklen_t)params[5];

	if(dest_len == 0) {
		// With no address, sendto is equivalent to send, which is equivalent to write
		sock = sock_lookup(&current_run, frompid, sockfd);
		if(!sock) {
			return;
		}
		if(options.log_sockets)
			fprintf(
			  stderr, "[%d]: SOCKET %-26s SENDTO enter\n", thread->id, sock_name(sock).c_str());
		if(sock->conn) {
			/* TODO: we need to only pass the SUCCESSFUL # OF BYTES WRITTEN!! */
			sock->conn->write(sock, this, params[2]);
		}
	} else {
		// Otherwise, we use the real, udp-style sendto
		if(options.log_sockets)
			fprintf(stderr, "[%d]: SENDTO with DESTINATION enter\n", thread->id);
	}
}

void Syssendto::finish()
{
	if(ret_success) {
		set_syscall_param(fromtid, RAX, params[2]);
	}
}
