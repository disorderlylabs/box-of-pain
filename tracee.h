#pragma once

#include <sys/reg.h>
#include <sys/user.h>
#include <vector>

class event;
class Syscall;

struct proc_tr {
	int id;
	int pid;
	int ecode;
	bool exited;
	/* TODO (major): thread synchro between threads in a process? */
	std::vector<event *> event_seq;
	std::vector<thread_tr *> proc_thread_list;
	char *invoke;
	size_t num_threads = 1;
};

struct thread_tr {
	int id;
	int uuid;
	int tid;
	int status;
	long sysnum;
	bool active;
	bool frozen = false;
	Syscall *syscall;
	struct proc_tr *proc;
	std::vector<event *> event_seq;
	/* TODO: this stuff may be broken for threads */
	uint64_t syscall_rip;
	uintptr_t shared_page;
	size_t sp_mark;
	struct user_regs_struct uregs;
};

struct thread_tr *find_tracee(int tid);
