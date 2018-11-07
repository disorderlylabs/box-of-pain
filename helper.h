#pragma once
#include <cassert>
#include <stdexcept>
#include <string>
#include <sys/ptrace.h>
#include <sys/syscall.h>
#include <tuple>

struct options {
	bool wait;
	bool dump;
	bool step;
	bool log_syscalls;
	bool log_sockets;
	bool log_follow;
	bool log_run;
	bool follow_stats;
};

extern struct options options;

std::string tracee_readstr(int child, unsigned long addr);
void tracee_copydata(int child, unsigned long addr, char *buf, ssize_t len);
void register_syscall_rip(struct thread_tr *t);
long inject_syscall(struct thread_tr *t,
  long num,
  long a = 0,
  long b = 0,
  long c = 0,
  long d = 0,
  long e = 0,
  long f = 0);
size_t __tracee_alloc_shared_page(struct thread_tr *t, size_t len);
void tracee_free_shared_page(struct thread_tr *t);
uintptr_t tracee_get_shared_page(struct thread_tr *t);
void tracee_set(int child, unsigned long addr, unsigned long val);

#define tracee_alloc_shared_page(tr, type)                                                         \
	(type *)(tracee_get_shared_page(tr) + __tracee_alloc_shared_page(tr, sizeof(type)))

#define GETOBJ(ch, addr, obj) tracee_copydata(ch, addr, (char *)obj, sizeof(obj))

template<typename T, size_t n = 1>
T tracee_readvals(int child, unsigned long addr)
{
	/* TODO: use n as a 'read n values' Right now this only works with n == 1 because that's all we
	 * need for now. */
	errno = 0;
	unsigned long tmp = ptrace(PTRACE_PEEKDATA, child, addr);
	return (T)tmp;
}

#define GET(T, ch, addr)                                                                           \
	({                                                                                             \
		auto t = tracee_readvals<T>(ch, addr);                                                     \
		if(errno != 0) {                                                                           \
			throw std::runtime_error(std::string("errno: ") + std::to_string(errno));              \
		};                                                                                         \
		t;                                                                                         \
	})
