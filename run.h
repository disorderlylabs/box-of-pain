#pragma once

#include <unordered_map>

#include "helper.h"
#include "sockets.h"
#include "sys.h"
#include "tracee.h"

class connection;
class sock;
struct run {
	/* list of all traced processes in the system */
	std::vector<struct proc_tr *> proc_list;
	/* list of all traced threads in the system */
	std::vector<struct thread_tr *> thread_list;
	/* list of all syscalls that happen, in order that they are observed */
	std::vector<Syscall *> syscall_list;

	/*A map of tids to thread structures*/
	std::unordered_map<int, struct thread_tr *> traces; 

	std::unordered_map<int, std::unordered_map<int, class sock *> > sockets;
	std::unordered_map<connid, connection *> connections;
};

extern struct run current_run;

