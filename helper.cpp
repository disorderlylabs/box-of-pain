#include <sys/ptrace.h>
#include <stdlib.h>
#include <string>
#include "helper.h"
#include <tuple>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>

#include <tracee.h>
#define SYSCALL_INSTRUCTION_SZ 2
void register_syscall_rip(struct trace *t)
{
	if(t->syscall_rip == 0) {
		if(ptrace(PTRACE_GETREGS, t->pid, NULL, &t->uregs) != -1) {
			t->syscall_rip = t->uregs.rip - SYSCALL_INSTRUCTION_SZ;
			fprintf(stderr, "tracee %d discovered syscall address %lx\n", t->id, t->syscall_rip);
		}
	}
}

long inject_syscall(struct trace *t, long num, long a, long b, long c, long d, long e, long f)
{
	if(t->syscall_rip == 0) {
		fprintf(stderr, "failed to inject syscall into tracee %d\n", t->id);
		abort();
	}

	ptrace(PTRACE_GETREGS, t->pid, NULL, &t->uregs);
	struct user_regs_struct regs;
	regs.rax = num;
	regs.rdi = a;
	regs.rsi = b;
	regs.rdx = c;
	regs.r10 = d;
	regs.r8 = e;
	regs.r9 = f;
	regs.rip = t->syscall_rip;
	ptrace(PTRACE_SETREGS, t->pid, NULL, &regs);
	int status;
	int sig = 0;
	while(true) {
		ptrace(PTRACE_SINGLESTEP, t->pid, NULL, NULL);
		waitpid(t->pid, &status, 0);
		if(WIFEXITED(status) || WIFSIGNALED(status)) {
			return -2;
		}
		if(WIFSTOPPED(status)){
			if(WSTOPSIG(status) != SIGTRAP) {
				sig = WSTOPSIG(status);
				continue;
			} else {
				break;
			}
		}
	}
	ptrace(PTRACE_GETREGS, t->pid, NULL, &regs);
	if(sig) {
		kill(t->pid, sig);
	}
	ptrace(PTRACE_SETREGS, t->pid, NULL, &t->uregs);

	if((long)regs.rax < 0) {
		errno = -regs.rax;
		return -1;
	}
	return regs.rax;
}

std::string tracee_readstr(int child, unsigned long addr)
{
	std::string str = "";
	while(true) {
		unsigned long tmp = ptrace(PTRACE_PEEKDATA, child, addr++);
		if(errno != 0) {
			break;
		}
		char *buf = (char *)&tmp;
		for(unsigned int i=0;i<sizeof(tmp);i++) {
			if(!*buf) {
				return str;
			}
			str += *buf++;
		}
	}
	return str;
}

void tracee_copydata(int child, unsigned long addr, char *buf, ssize_t len)
{
	while(true) {
		unsigned long tmp = ptrace(PTRACE_PEEKDATA, child, addr++);
		if(errno != 0) {
			break;
		}
		char *b = (char *)&tmp;
		for(unsigned int i=0;i<sizeof(tmp) && len;i++, len--) {
			*buf++ = *b++;
		}
	}
}

