#pragma once

#include <vector>

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
};

struct trace *find_tracee(int pid);

