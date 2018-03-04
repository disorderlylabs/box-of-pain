#pragma once

#include <vector>
#include <sys/reg.h>
#include <sys/user.h>

class event;
class Syscall;

struct proc_tr {
	int id;
	int pid;
	int ecode;
	bool exited;
	std::vector<event *> event_seq;
	char *invoke;
};

struct thread_tr {
	int id; //This is equal to process->id
	int tid;
	int status;
	long sysnum;
	Syscall *syscall;
	struct proc_tr *proc;
	std::vector<event *> event_seq;
	uint64_t syscall_rip;
	uintptr_t shared_page;
	size_t sp_mark;
	struct user_regs_struct uregs;
};

struct thread_tr *find_tracee(int tid);

