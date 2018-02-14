#pragma once

#include <sys/ptrace.h>
#include <sys/reg.h>

#include <vector>

#include "sockets.h"
#include "helper.h"
#include "tracee.h"

#define MAX_PARAMS 6 /* linux has 6 register parameters */
enum syscall_state {
	STATE_UNCALLED,
	STATE_CALLED,
	STATE_DONE,
};

/* x86_64-linux specific. parameters map (first param is RDI) */
static const int param_map[MAX_PARAMS] = {
	RDI,
	RSI,
	RDX,
	R10,
	R8,
	R9
};

static int set_syscall_param(int pid, int reg, long value)
{
	return ptrace(PTRACE_POKEUSER, pid, sizeof(long)*reg, value);
}

class Syscall;
/* an event is a syscall entry or exit. NOTE: we may extend this to
 * include signals, perhaps. Need to think about the semantics of this. */
class event {
	public:
	Syscall *sc;
	bool entry;
	/* this vector lists the extra partial-order "parents" of this event */
	std::vector<event *> extra_parents;
	event(Syscall *s, bool e) : sc(s), entry(e) {}
};

struct trace;
class Syscall {
	public:
		class event *entry_event, *exit_event;
		int frompid;
		int uuid;
		std::string localid;
		struct trace *tracee;
		unsigned long number;
		unsigned long params[MAX_PARAMS];
		unsigned long retval;
		/* if this gets set, it will force the syscall implementation
		 * to return success to the process, regardless of what happens.
		 * This can be used to, for example, make a write look like it
		 * succeeded while making it fail (by setting fd to -1). */
		bool ret_success = false;
		enum syscall_state state;

		Syscall(int fpid, long num) {
			frompid = fpid;
			state = STATE_CALLED;
			number = num;
			for(int i=0;i<MAX_PARAMS;i++) {
				params[i] = ptrace(PTRACE_PEEKUSER, frompid, sizeof(long)*param_map[i]);
			}
			tracee = find_tracee(frompid);
		}

		virtual void finish() {
			if(ret_success) {
				set_syscall_param(frompid, RAX, 0);
			}
		}
		virtual void start() { }

		virtual bool operator ==(const Syscall &other) const {
			return number == other.number;
		}

		virtual bool post_process(int pass __unused) { return false; }
};

class sockop {
	public:
		class sock *sock;
		class sock *get_socket() { return sock; }
};

extern std::vector<Syscall *> syscall_list;

class Sysclose : public Syscall, public sockop {
	public:
		Sysclose(int p, long n) : Syscall(p, n) {}
		void start() {
			int fd = params[0];
			sock = sock_lookup(frompid, fd);
			sock_close(frompid, fd);
		}
};

class Sysbind : public Syscall, public sockop {
	public:
		struct sockaddr addr;
		socklen_t len;
		Sysbind(int p, long n) : Syscall(p, n) {}
		void start() {
			int sockfd = params[0];
			GETOBJ(frompid, params[1], &addr);
			len = params[2];
			sock = sock_assoc(frompid, sockfd);
			sock_set_addr(sock, &addr, len);
		};
};

class Sysconnect : public Syscall, public sockop {
	public:
		class Sysaccept *pair;
		struct sockaddr addr;
		socklen_t len;
		Sysconnect(int p, long n) : Syscall(p, n) {}
		void start();
		void finish();
};

class Sysaccept : public Syscall, public sockop {
	public:
		class Sysconnect *pair;
		struct sock *serversock = NULL;
		struct sockaddr addr;
		socklen_t len;
		Sysaccept(int p, long n) : Syscall(p, n) {}
		void start();
		void finish();
};

class Syswrite : public Syscall, public sockop {
	public:
		Syswrite(int p, long n) : Syscall(p, n) {}
		void finish();
		void start();
};

class Sysread : public Syscall, public sockop {
	public:
		Sysread(int fpid, long n) : Syscall(fpid, n) {}
		void finish();
		void start();
};

class Sysrecvfrom : public Syscall, public sockop {
	public:
		int sockfd;
		void *buffer;
		size_t length;
		int flags;
		struct sockaddr *addr;
		socklen_t *addrlen;

		Sysrecvfrom(int fpid, long n) : Syscall(fpid, n) {}

		void start();
		void finish();
};

class Syssendto : public Syscall, public sockop {
	public:
		int sockfd;
		void *buffer;
		size_t length;
		int flags;
		struct sockaddr *dest;
		socklen_t dest_len;

		Syssendto(int fpid, long n) : Syscall(fpid, n) {}

        void start();
        void finish();

};

