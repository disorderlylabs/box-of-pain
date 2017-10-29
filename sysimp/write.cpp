#include <sys.h>
#include <tracee.h>

void Syswrite::start() { 
	sock = sock_lookup(frompid, params[0]);
	if(!sock) {
		return;
	}
	fprintf(stderr, "[%d]: SOCKET %-26s WRITE enter\n",
			find_tracee(frompid)->id, sock_name(sock).c_str());
} 
void Syswrite::finish() {
	if(ret_success) {
		set_syscall_param(frompid, RAX, params[2]);
	}
}

bool Syswrite::post_process(int pass) {
	if(pass == 0) {
		return true;
	} else if(pass == 1) {
		class sock *s = get_socket();
		if(s && s->conn && retval > 0) {
			s->conn->write(s, this, retval);
		}
	}
	return false;
}

