#include <sys.h>
#include <tracee.h>
#include <unordered_map>

extern std::vector<struct thread_tr *> thread_list;
extern std::unordered_map<int, struct thread_tr *> traces;

void Sysclone::start() {}



void Sysclone::finish() {
	int newtid = retval;
	//ptrace(PTRACE_GETEVENTMSG, fromtid, 0, &newtid);

	fprintf(stderr, "[%d]: clone() created new thread with tid: %d\n",
			thread->id, newtid);

	//Create a tracee object for the new thread
	struct thread_tr *tr = new thread_tr();
	tr->sysnum = -1; //we're not in a syscall to start.
	tr->syscall_rip = -1;
	tr->shared_page = 0;
	tr->sp_mark = 0;
	tr->syscall = NULL;
	tr->proc = thread->proc;
	tr->id = thread->id;
	tr->tid = newtid;
	traces[tr->tid] = tr;
	thread_list.push_back(tr);

	//Tell the new thread to run
	if( ptrace(PTRACE_SYSCALL, newtid, 0, 0) != 0)
	 { perror("clone: ptrace SYSCALL"); }
}


