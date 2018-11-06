#include <sys.h>
#include <tracee.h>

void Syswrite::start()
{
	sock = sock_lookup(&current_run, frompid, params[0]);
	if(!sock) {
		return;
	}
	if(options.log_sockets)
		fprintf(stderr, "[%d]: SOCKET %-26s WRITE enter\n", thread->id, sock_name(sock).c_str());
	if(sock->conn) {
		/* TODO: we need to only pass the SUCCESSFUL # OF BYTES WRITTEN!! */
		sock->conn->write(sock, this, params[2]);
	}
}
void Syswrite::finish()
{
	if(ret_success) {
		set_syscall_param(fromtid, RAX, params[2]);
	}
}
