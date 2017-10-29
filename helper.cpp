#include <sys/ptrace.h>
#include <stdlib.h>
#include <string>
#include "helper.h"
#include <tuple>
#include <errno.h>
#include <signal.h>
#include <err.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <cstring>

#include <tracee.h>
#define SYSCALL_INSTRUCTION_SZ 2
void register_syscall_rip(struct trace *t)
{
	if(t->syscall_rip == 0) {
		memset(&t->uregs, 0, sizeof(t->uregs));
		if(ptrace(PTRACE_GETREGS, t->pid, NULL, &t->uregs) != -1) {
			t->syscall_rip = t->uregs.rip - SYSCALL_INSTRUCTION_SZ;
			fprintf(stderr, "tracee %d discovered syscall address %lx\n", t->id, t->syscall_rip);
		}
	}
}

long inject_syscall(struct trace *t, long num, long a, long b, long c, long d, long e, long f)
{
	if(t->syscall_rip == 0 || (long)t->syscall_rip == -1) {
		fprintf(stderr, "failed to inject syscall into tracee %d\n", t->id);
		abort();
	}

	ptrace(PTRACE_GETREGS, t->pid, NULL, &t->uregs);
	struct user_regs_struct regs = t->uregs;
	regs.rax = num;
	regs.orig_rax = num;
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
		if(WIFCONTINUED(status)) {
			break;
		}
		if(WIFEXITED(status) || WIFSIGNALED(status)) {
			return -2;
		}
		if(WIFSTOPPED(status)) {
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

	if(WIFCONTINUED(status)) {
		return -3;
	}

	if((long)regs.rax < 0) {
		errno = -regs.rax;
		return -1;
	}
	return regs.rax;
}

size_t __tracee_alloc_shared_page(struct trace *t, size_t len)
{
	if(t->sp_mark + len >= 0x1000) {
		fprintf(stderr, "failed to allocate memory in shared page for tracee %d\n", t->id);
		abort();
	}
	size_t off = t->sp_mark;
	t->sp_mark += ((len - 1) & ~7) + 8;
	return off;
}

void tracee_free_shared_page(struct trace *t)
{
	t->sp_mark = 0;
}

uintptr_t tracee_get_shared_page(struct trace *t)
{
	if(t->shared_page == 0) {
		long ret = inject_syscall(t, SYS_mmap, 0, 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
		if((void *)ret == MAP_FAILED) {
			err(1, "failed to map shared page for tracee %d", t->id);
		}
		t->shared_page = ret;
	}
	return t->shared_page;
}

std::string tracee_readstr(int child, unsigned long addr)
{
	std::string str = "";
	errno = 0;
	while(true) {
		unsigned long tmp = ptrace(PTRACE_PEEKDATA, child, addr);
		addr += sizeof(tmp);
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

void tracee_set(int child, unsigned long addr, unsigned long val)
{
	errno = 0;
	ptrace(PTRACE_POKEDATA, child, addr, (void *)val);
}

void tracee_copydata(int child, unsigned long addr, char *buf, ssize_t len)
{
	errno = 0;
	while(len > 0) {
		unsigned long tmp = ptrace(PTRACE_PEEKDATA, child, addr);
		addr += sizeof(tmp);
		if(errno != 0) {
			break;
		}
		char *b = (char *)&tmp;
		for(unsigned int i=0;i<sizeof(tmp) && len;i++, len--) {
			*buf++ = *b++;
		}
	}
}

