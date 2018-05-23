#include <sys.h>
#include <tracee.h>
#include <unordered_map>

#include "run.h"

void Sysclone::start() {}



void Sysclone::finish() {
	int newtid = retval;
	//ptrace(PTRACE_GETEVENTMSG, fromtid, 0, &newtid);

	fprintf(stderr, "[%d : %d]: clone() created new thread with tid: %d\n",
			thread->id, thread->tid, newtid);

	//Create a tracee object for the new thread
	struct thread_tr *tr = new thread_tr();
	tr->sysnum = -1; //we're not in a syscall to start.
	tr->syscall_rip = 0;  //NOTE: This differs from the regular initialization
	tr->shared_page = 0;
	tr->sp_mark = 0;
	tr->syscall = NULL;
	tr->proc = thread->proc;
	tr->active = true;
	tr->id = current_run.thread_list.size();
	tr->tid = newtid;
	current_run.traces[tr->tid] = tr;
	current_run.thread_list.push_back(tr);
	thread->proc->proc_thread_list.push_back(tr);

	//Tell the new thread to run
	if( ptrace(PTRACE_SYSCALL, newtid, 0, 0) != 0)
	 { perror("clone: ptrace SYSCALL"); }
}


