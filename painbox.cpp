#include <cstdio>
#include <cstdlib>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/syscall.h>
#include <functional>
#include <sys/socket.h>

#include "scnames.h"

class Syscall;
struct trace {
	int pid;
	int id;
	int status;
	long sysnum;
	Syscall *syscall;
	bool exited;
};

int num_traces=0;
struct trace *traces = NULL;

#define MAX_PARAMS 6 /* linux has 6 register parameters */

int param_map[MAX_PARAMS] = {
	RDI,
	RSI,
	RDX,
	R10,
	R8,
	R9
};

enum syscall_state {
	STATE_UNCALLED,
	STATE_CALLED,
	STATE_DONE,
};

class Syscall {
	public:
		int frompid;
		unsigned long number;
		unsigned long params[MAX_PARAMS];
		unsigned long retval;
		enum syscall_state state;

		Syscall() {
			state = STATE_CALLED;
			number = ptrace(PTRACE_PEEKUSER, frompid, sizeof(long)*ORIG_RAX);
			for(int i=0;i<MAX_PARAMS;i++) {
				params[i] = ptrace(PTRACE_PEEKUSER, frompid, sizeof(long)*param_map[i]);
			}
			init();
		}

		virtual void finish() { }
		virtual void start() { }
		virtual void init() { };

		virtual bool operator ==(const Syscall &other) const {
			return number == other.number;
		}
};

class SysRead : public Syscall {
	public:
};

class SysRecvfrom : public Syscall {
	public:
		int socket;
		void *buffer;
		size_t length;
		int flags;
		struct sockaddr *addr;
		socklen_t *addrlen;

		void init() {
			socket = params[0];
			buffer = (void *)params[1];
			length = params[2];
			flags = params[3];
			addr = (struct sockaddr *)params[4];
			addrlen = (socklen_t *)params[5];
		}

		bool operator ==(const SysRecvfrom &other) const {
			/* Simple "fuzzy" comparison: socket is the same */
			/* TODO: check addr for source, etc */
			return socket == other.socket;
		}

		void start() { }
		void finish() { }
};

#if 0
/* HACK: there are not this many syscalls, but there is no defined "num syscalls" to
 * use. */
std::function<Syscall *()> syscallmap[1024] = {
	[SYS_read] = [](){return new SysRead();},
	[SYS_recvfrom] = [](){return new SysRecvfrom();},
};
#endif

struct trace *wait_for_syscall(void)
{
	int status;
	while(1) {
		int pid;
		if((pid=waitpid(-1, &status, 0)) == -1) {
			return NULL;
		}

		struct trace *tracee = NULL;
		for(int i=0;i<num_traces;i++) {
			if(pid == traces[i].pid) {
				tracee = &traces[i];
				break;
			}
		}

		if(tracee == NULL) {
			fprintf(stderr, "waitpid returned untraced process %d!\n", pid);
			exit(1);
		}

		tracee->status = status;
		if(WIFSTOPPED(status) && WSTOPSIG(status) & 0x80) {
			return tracee;
		}
		if(WIFEXITED(status)) {
			tracee->exited = true;
			return tracee;
		}
		ptrace(PTRACE_SYSCALL, tracee->pid, 0, 0);
	}
}

int do_trace()
{
	for(int i=0;i<num_traces;i++) {
		fprintf(stderr, "init trace on %d\n", traces[i].pid);
		int status;
		traces[i].sysnum = -1;
    	if(waitpid(traces[i].pid, &status, 0) == -1) {
			perror("waitpid");
		}
		ptrace(PTRACE_SETOPTIONS, traces[i].pid, 0, PTRACE_O_TRACESYSGOOD);
		if(errno != 0) { perror("ptrace SETOPTIONS"); }
		ptrace(PTRACE_SYSCALL, traces[i].pid, 0, 0);
		if(errno != 0) { perror("ptrace SYSCALL"); }
	}

	int num_exited = 0;
	while(true) {
		struct trace *tracee;
		if((tracee=wait_for_syscall()) == NULL) break;

		if(tracee->exited) {
			num_exited++;
			fprintf(stderr, "PID %d exited\n", tracee->pid);
			if(num_exited == num_traces) break;
			continue;
		}

		if(tracee->sysnum == -1) {
			tracee->sysnum = ptrace(PTRACE_PEEKUSER, tracee->pid, sizeof(long)*ORIG_RAX);
			if(errno != 0) break;

			fprintf(stderr, "[%d]: %s entry\n", tracee->id, syscall_names[tracee->sysnum]);
			tracee->syscall = NULL;
	//		try { tracee->syscall = syscallmap[tracee->sysnum](pid); } catch (std::bad_function_call e) {}

			if(tracee->syscall) {
				tracee->syscall->frompid = tracee->pid;
				tracee->syscall->start();
			}
		} else {
			int retval = ptrace(PTRACE_PEEKUSER, tracee->pid, sizeof(long)*RAX);
			if(errno != 0) break;
			fprintf(stderr, "[%d]: %s exit -> %d\n", tracee->id, syscall_names[tracee->sysnum], retval);
			if(tracee->syscall) {
				tracee->syscall->retval = retval;
				tracee->syscall->state = STATE_DONE;
				tracee->syscall->finish();
			}
			tracee->sysnum = -1;
		}
		ptrace(PTRACE_SYSCALL, tracee->pid, 0, 0);

	}
	return 0;
}

int main(int argc, char **argv)
{
	static int _id = 0;
	for(int i=1;i<argc;i++) {
		fprintf(stderr, "starting %s\n", argv[i]);
		int pid = fork();
		if(pid == 0) {
			ptrace(PTRACE_TRACEME);
			kill(getpid(), SIGSTOP);
			if(execlp(argv[i], argv[i], NULL) == -1) {
				fprintf(stderr, "failed to execute %s\n", argv[i]);
			}
			exit(255);
		}
		num_traces++;
		traces = (trace *)realloc(traces, num_traces * sizeof(struct trace));
		traces[num_traces-1].id = _id++;
		traces[num_traces-1].pid = pid;
		traces[num_traces-1].sysnum = -1; //we're not in a syscall to start.
		traces[num_traces-1].syscall = NULL;
		traces[num_traces-1].exited = false;
	}
	do_trace();
}

