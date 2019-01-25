#pragma once

#include <unordered_map>

struct run;
extern struct run current_run;

#define SPACE(f, n) fprintf(f, "%*s", n, "")
#include "helper.h"
#include "sockets.h"
#include "sys.h"
#include "tracee.h"
#include <set>
class connection;
class sock;
class run
{
  public:
	const char *name;
	/* list of all traced processes in the system */
	std::vector<struct proc_tr *> proc_list;
	/* list of all traced threads in the system */
	std::vector<struct thread_tr *> thread_list;
	/* list of all syscalls that happen, in order that they are observed */
	std::vector<Syscall *> syscall_list;

	std::vector<sock *> sock_list;

	std::vector<connection *> connection_list;
	/*A map of tids to thread structures*/
	std::unordered_map<int, struct thread_tr *> traces;

	std::unordered_map<int, std::unordered_map<int, class sock *>> sockets;
	std::unordered_map<connid, connection *> connections;

	std::set<std::pair<int, int>> fault_node_set;

	bool fell_off = false;
};

extern struct run current_run;

#include <cstdio>
void run_serialize(struct run *run, FILE *f);
void run_load(struct run *run, FILE *f);

void followrun_del(struct run *run);
void followrun_add(struct run *run);
bool followrun_step(struct thread_tr *tracee);
void followrun_dumpall();
bool followrun_stats();
void dump(const char *, struct run *run);
