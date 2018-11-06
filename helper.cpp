#include "helper.h"
#include <cstring>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <tuple>

#include <tracee.h>
#define SYSCALL_INSTRUCTION_SZ 2
void register_syscall_rip(struct thread_tr *t)
{
	assert(t->active);
	/* to inject syscalls, we'll need to find a syscall instruction in the process to
	 * "jump" to. The easiest way to do this is to wait for a syscall (which we're doing
	 * anyway) and then figure out the RIP of the process (and subtract the size of the
	 * syscall instruction) */
	if(t->syscall_rip == 0) {
		memset(&t->uregs, 0, sizeof(t->uregs));
		if(ptrace(PTRACE_GETREGS, t->tid, NULL, &t->uregs) != -1) {
			t->syscall_rip = t->uregs.rip - SYSCALL_INSTRUCTION_SZ;
			if(options.log_run)
				fprintf(
				  stderr, "tracee %d discovered syscall address %lx\n", t->id, t->syscall_rip);
		}
	}
}

long inject_syscall(struct thread_tr *t, long num, long a, long b, long c, long d, long e, long f)
{
	assert(t->active);
	if(t->syscall_rip == 0 || (long)t->syscall_rip == -1) {
		/* dont inject a syscall before we detect the first syscall. Simple! */
		fprintf(stderr, "failed to inject syscall into tracee %d\n", t->id);
		abort();
	}

	/* okay, here's the plan (it's pretty clever):
	 *   - Save current uregs, and copy to new struct.
	 *   - Set arguments and #, and new RIP that points to a syscall instruction.
	 *   - Set the regs and single-step the process.
	 *   - wait for the process, and check what happened.
	 *   - If we got a signal, then we should hold on to it for later.
	 *   - Once the process has finished the syscall (by TRAP, we'll see the
	 *     single step done), read the regs to get the return value.
	 *   - Deliver a signal if we got one
	 *   - Restore regs, and return retval (after setting errno).
	 */
	ptrace(PTRACE_GETREGS, t->tid, NULL, &t->uregs);
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
	ptrace(PTRACE_SETREGS, t->tid, NULL, &regs);
	int status;
	int sig = 0;
	while(true) {
		ptrace(PTRACE_SINGLESTEP, t->tid, NULL, NULL);
		waitpid(t->tid, &status, 0);
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
	ptrace(PTRACE_GETREGS, t->tid, NULL, &regs);
	if(sig) {
		kill(t->tid, sig);
	}
	ptrace(PTRACE_SETREGS, t->tid, NULL, &t->uregs);

	if(WIFCONTINUED(status)) {
		return -3;
	}

	if((long)regs.rax < 0) {
		errno = -regs.rax;
		return -1;
	}
	return regs.rax;
}

size_t __tracee_alloc_shared_page(struct thread_tr *t, size_t len)
{
	assert(t->active);
	/* we're treating this as a arena allocator, because we
	 * assume the memory gets freed soon after a syscall injection */
	if(t->sp_mark + len >= 0x1000) {
		fprintf(stderr, "failed to allocate memory in shared page for tracee %d\n", t->id);
		abort();
	}
	size_t off = t->sp_mark;
	t->sp_mark += ((len - 1) & ~7) + 8;
	return off;
}

void tracee_free_shared_page(struct thread_tr *t)
{
	assert(t->active);
	t->sp_mark = 0;
}

uintptr_t tracee_get_shared_page(struct thread_tr *t)
{
	assert(t->active);
	if(t->shared_page == 0) {
		/* setting up a "shared" page (it's not really shared, we just know where it is),
		 * is as simple as injecting mmap into the process! */
		long ret = inject_syscall(
		  t, SYS_mmap, 0, 0x1000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
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
		for(unsigned int i = 0; i < sizeof(tmp); i++) {
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
		for(unsigned int i = 0; i < sizeof(tmp) && len; i++, len--) {
			*buf++ = *b++;
		}
	}
}
