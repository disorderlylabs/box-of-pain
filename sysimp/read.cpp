#include <sys.h>
#include <tracee.h>

void Sysread::start() {
	sock = sock_lookup(frompid, params[0]);
	if(!sock) {
		return;
	}
	fprintf(stderr, "[%d]: SOCKET %-26s READ  enter\n",
			find_tracee(frompid)->id, sock_name(sock).c_str());
}
void Sysread::finish() {
	if(!sock) {
		return;
	}
	fprintf(stderr, "[%d]: SOCKET %-26s READ  retur\n",
			find_tracee(frompid)->id, sock_name(sock).c_str());
	
	if(sock->conn && retval > 0) {
		std::vector<Syscall *> rcs = sock->conn->read(sock, retval);
		for(auto wsys : rcs) {
			exit_event->extra_parents.push_back(wsys->entry_event);
		}
	}
}
