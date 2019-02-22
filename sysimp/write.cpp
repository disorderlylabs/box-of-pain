#include <sys.h>
#include <tracee.h>

#include <cstdlib>

void Syswrite::start()
{
	sock = sock_lookup(&current_run, frompid, params[0]);
	if(!sock) {
		return;
	}
	/*
	if(params[2] > 1) {
	    long nl = params[2] / (rand() % 3 + 1);
	    if(nl > 0) {
	        set_param(param_map[2], nl);
	        fprintf(stderr, "Setting write param from %ld to %ld\n", params[2], nl);
	    }
	}*/
	if(options.log_sockets)
		fprintf(stderr, "[%d]: SOCKET %-26s WRITE enter\n", thread->id, sock_name(sock).c_str());
	if(sock->conn) {
		/* TODO: we need to only pass the SUCCESSFUL # OF BYTES WRITTEN!! */
		sock->conn->write(sock, this, params[2]);
	}
}
void Syswrite::finish()
{
	// if(sock)
	//	fprintf(stderr, "Write got %ld\n", retval);
	if(ret_success) {
		set_syscall_param(fromtid, RAX, params[2]);
	}
}
