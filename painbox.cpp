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
/* based on code from
 * https://blog.nelhage.com/2010/08/write-yourself-an-strace-in-70-lines-of-code/
 */

int pid;

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
			frompid = pid;
			state = STATE_CALLED;
			number = ptrace(PTRACE_PEEKUSER, frompid, sizeof(long)*ORIG_RAX);
			for(int i=0;i<MAX_PARAMS;i++) {
				params[i] = ptrace(PTRACE_PEEKUSER, frompid, sizeof(long)*param_map[i]);
			}
		}

		virtual void finish() { }
};

class SysRead : public Syscall {
	public:
		SysRead() { }
};

/* HACK: there are not this many syscalls, but there is no defined "num syscalls" to
 * use. */
std::function<Syscall *()> syscallmap[1024] = {
	[SYS_read] = [](){return new SysRead();},
};

int wait_for_syscall(int p)
{
	int status;
	while(1) {
		ptrace(PTRACE_SYSCALL, p, 0, 0);
		waitpid(p, &status, 0);
		if(WIFSTOPPED(status) && WSTOPSIG(status) & 0x80) {
			return 0;
		}
		if(WIFEXITED(status)) {
			return 1;
		}
	}
}

int do_trace()
{
	int status, syscall, retval;
    waitpid(pid, &status, 0);
	ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_TRACESYSGOOD);

	while(true) {
		if(wait_for_syscall(pid) != 0) break;

		syscall = ptrace(PTRACE_PEEKUSER, pid, sizeof(long)*ORIG_RAX);
		if(errno != 0) break;
		fprintf(stderr, "syscall(%d) = ", syscall);
		Syscall *s = NULL;
		try {
			s = syscallmap[syscall]();
		} catch(std::bad_function_call e) {

		}
		
		if(wait_for_syscall(pid) != 0) break;
		retval = ptrace(PTRACE_PEEKUSER, pid, sizeof(long)*RAX);
		if(errno != 0) break;
		fprintf(stderr, "%d\n", retval);

		if(s) {
			s->retval = retval;
			s->state = STATE_DONE;
			s->finish();
		}
		
	}
	return 0;
}

int main(int argc, char **argv)
{
	
	pid = fork();
	if(pid == 0) {
		ptrace(PTRACE_TRACEME);
		kill(getpid(), SIGSTOP);
		if(execvp(argv[1], &argv[1]) == -1) {
			fprintf(stderr, "failed to execute %s\n", argv[1]);
		}
		exit(255);
	}
	do_trace();
}

