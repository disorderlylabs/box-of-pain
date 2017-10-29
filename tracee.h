#pragma once

#include <vector>
#include <sys/reg.h>
#include <sys/user.h>

class event;
class Syscall;
struct trace {
	int pid;
	int ecode;
	int id;
	int status;
	long sysnum;
	Syscall *syscall;
	bool exited;
	std::vector<event *> event_seq;
	char *invoke;
	uint64_t syscall_rip;
	uintptr_t shared_page;
	size_t sp_mark;
	struct user_regs_struct uregs;
};

struct trace *find_tracee(int pid);

