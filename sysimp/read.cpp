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
}

bool Sysread::post_process(int pass) {
	if(pass < 2) {
		return true;
	} else if(pass == 2) {
		class sock *s = get_socket();
		if(s && s->conn && retval > 0) {
			std::vector<Syscall *> rcs = s->conn->read(s, retval);
			for(auto wsys : rcs) {
				exit_event->extra_parents.push_back(wsys->entry_event);
			}
		}
	}
	return false;
}

