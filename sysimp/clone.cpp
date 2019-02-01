#include <sys.h>
#include <tracee.h>
#include <unordered_map>

#include "run.h"

void Sysclone::start()
{
}

void Sysclone::finish()
{
	int newtid = retval;
	// ptrace(PTRACE_GETEVENTMSG, fromtid, 0, &newtid);
	bool new_process = params[2] == 0;

	// Create a tracee object for the new thread
	struct thread_tr *tr = new thread_tr();
	tr->sysnum = -1;     // we're not in a syscall to start.
	tr->syscall_rip = 0; // NOTE: This differs from the regular initialization
	tr->shared_page = 0;
	tr->sp_mark = 0;
	tr->syscall = NULL;
	tr->active = true;
	tr->id = current_run.thread_list.size();
	tr->tid = newtid;
	current_run.traces[tr->tid] = tr;
	current_run.thread_list.push_back(tr);

	if(new_process) {
		struct proc_tr *ptr = new proc_tr();
		ptr->id = current_run.proc_list.size();
		tr->syscall_rip = -1;
		ptr->exited = false;
		tr->proc = ptr;
		ptr->invoke = thread->proc->invoke;
		ptr->proc_thread_list.push_back(tr);
		current_run.proc_list.push_back(ptr);
	} else {
		tr->proc = thread->proc;
		thread->proc->proc_thread_list.push_back(tr);
		thread->proc->num_threads++;
	}

	if(options.log_run)
		fprintf(stderr,
		  "[%d : %d]: clone(%x) created new %s with tid %d, id %d :: proc %d has %ld "
		  "threads\n",
		  thread->id,
		  thread->tid,
		  (int)params[2],
		  new_process ? "process" : "thread",
		  newtid,
		  tr->id,
		  tr->proc->id,
		  tr->proc->num_threads);

	// Tell the new thread to run
	if(ptrace(PTRACE_SYSCALL, newtid, 0, 0) != 0) {
		perror("clone: ptrace SYSCALL");
	}
}
