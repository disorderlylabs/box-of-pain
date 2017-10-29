#include <cstdio>
#include <cstdlib>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/syscall.h>
#include <functional>
#include <sys/socket.h>
#include <netinet/in.h>

#include "helper.h"
#include "sockets.h"
#include "scnames.h"
#include "sys.h"
#include "tracee.h"
#define LOG_SYSCALLS 0

struct options {
	bool dump;
} options = {.dump = false};

std::vector<struct trace *> traces;
std::vector<Syscall *> syscall_list;

struct trace *find_tracee(int pid)
{
	for(auto t : traces) {
		if(t->pid == pid)
			return t;
	}
	return NULL;
}

template <typename T>
Syscall * make(int fpid, long n) { return new T(fpid, n); }
Syscall * (*syscallmap[1024])(int, long) = { };

struct trace *wait_for_syscall(void)
{
	int status;
	while(1) {
		int pid;
		if((pid=waitpid(-1, &status, 0)) == -1) {
			return NULL;
		}

		struct trace *tracee = find_tracee(pid);
		if(tracee == NULL) {
			fprintf(stderr, "waitpid returned untraced process %d!\n", pid);
			exit(1);
		}

		tracee->status = status;
		if(WIFSTOPPED(status) && WSTOPSIG(status) & 0x80) {
			return tracee;
		}
		if(WIFEXITED(status)) {
			tracee->exited = true;
			tracee->ecode = WEXITSTATUS(status);
			return tracee;
		}
		ptrace(PTRACE_SYSCALL, tracee->pid, 0, 0);
	}
}

int do_trace()
{
	for(auto tr : traces) {
		fprintf(stderr, "init trace on %d\n", tr->pid);
		int status;
		tr->sysnum = -1;
    	if(waitpid(tr->pid, &status, 0) == -1) {
			perror("waitpid");
		}
		ptrace(PTRACE_SETOPTIONS, tr->pid, 0, PTRACE_O_TRACESYSGOOD);
		if(errno != 0) { perror("ptrace SETOPTIONS"); }
		ptrace(PTRACE_SYSCALL, tr->pid, 0, 0);
		if(errno != 0) { perror("ptrace SYSCALL"); }
	}

	unsigned num_exited = 0;
	while(true) {
		struct trace *tracee;
		if((tracee=wait_for_syscall()) == NULL) break;

		if(tracee->exited) {
			num_exited++;
			fprintf(stderr, "Exit %d (pid %d) exited: %d\n", tracee->id, tracee->pid, tracee->ecode);
			if(num_exited == traces.size()) break;
			continue;
		}

		register_syscall_rip(tracee);

		if(tracee->sysnum == -1) {
			tracee->sysnum = ptrace(PTRACE_PEEKUSER, tracee->pid, sizeof(long)*ORIG_RAX);
			if(errno != 0) break;

#if LOG_SYSCALLS
			fprintf(stderr, "[%d]: %s entry\n", tracee->id, syscall_names[tracee->sysnum]);
#endif
			tracee->syscall = NULL;
			if(syscallmap[tracee->sysnum]) {
				tracee->syscall = syscallmap[tracee->sysnum](tracee->pid, tracee->sysnum);
				tracee->syscall->start();
				class event *e = new event(tracee->syscall, true);
				tracee->syscall->entry_event = e;
				tracee->event_seq.push_back(e);

				e = new event(tracee->syscall, false);
				tracee->syscall->exit_event = e;

				tracee->syscall->uuid = syscall_list.size();
				syscall_list.push_back(tracee->syscall);
			}
		} else {
			long retval = ptrace(PTRACE_PEEKUSER, tracee->pid, sizeof(long)*RAX);
			if(errno != 0) break;
#if LOG_SYSCALLS
			fprintf(stderr, "[%d]: %s exit -> %d\n", tracee->id, syscall_names[tracee->sysnum], retval);
#endif
			if(tracee->syscall) {
				tracee->syscall->retval = retval;
				tracee->syscall->state = STATE_DONE;
				tracee->event_seq.push_back(tracee->syscall->exit_event);
				tracee->syscall->finish();
			}
			if(tracee->sysnum == SYS_execve) {
				/* tracee has executed, start tracking */
				tracee->syscall_rip = 0;
			}
			tracee->sysnum = -1;
		}
		ptrace(PTRACE_SYSCALL, tracee->pid, 0, 0);

	}
	return 0;
}

void usage(void)
{
	printf("usage: box-of-pain [-d] -e prog,arg1,arg2,... [-e prog,arg1,arg2,...]...\n");
	printf("options:\n");
	printf("   -e prog,arg1,...     : Trace program 'prog' with arguments arg1...\n");
	printf("   -d                   : Dump tracing info\n");
}

#define SETSYS(s) syscallmap[SYS_ ## s] = make<Sys ## s>
int main(int argc, char **argv)
{
	SETSYS(recvfrom);
	SETSYS(read);
	SETSYS(write);
	SETSYS(accept);
	SETSYS(connect);
	SETSYS(bind);

	int r;
	while((r = getopt(argc, argv, "e:dh")) != EOF) {
		switch(r) {
			case 'e':
			{
				struct trace *tr = new trace();
				tr->id = traces.size();
				tr->sysnum = -1; //we're not in a syscall to start.
				tr->syscall_rip = -1;
				tr->syscall = NULL;
				tr->exited = false;
				tr->invoke = strdup(optarg);
				traces.push_back(tr);
			} break;
			case 'h':
				usage();
				return 0;
			case 'd':
				options.dump = true;
				break;
			default:
				usage();
				return 1;
		}
	}
	
	for(auto tr : traces) {
		char **args = (char **)calloc(2, sizeof(char *));
		char *prog = strdup(strtok(tr->invoke, ","));
		args[0] = prog;
		char *tmp;
		int ac = 1;
		while((tmp = strtok(NULL, ","))) {
			args = (char **)realloc(args, (ac+2) * sizeof(char *));
			args[ac] = strdup(tmp);
			args[ac+1] = NULL;
			ac++;
		}
		int pid = fork();
		if(pid == 0) {
			ptrace(PTRACE_TRACEME);
			kill(getpid(), SIGSTOP);
			if(execvp(prog, args) == -1) {
				fprintf(stderr, "failed to execute %s\n", prog);
			}
			exit(255);
		}
		tr->pid = pid;
		fprintf(stderr, "Tracee %d: starting %s (pid %d)\n", tr->id, tr->invoke, tr->pid);
	}
	do_trace();
	for(auto tr : traces) {
		if(tr->ecode != 0) {
			fprintf(stderr, "Tracee %d exited non-zero exit code\n", tr->id);
		}
	}

	int pass = 0;
	bool more = true;
	while(more) {
		fprintf(stderr, "post-processing pass %d\n", pass);
		more = false;
		for(auto tr : traces) {
			for(auto e : tr->event_seq) {
				if(e->entry) {
					more = e->sc->post_process(pass) || more;
				}
			}
		}
		pass++;
	}

	if(options.dump) {
		FILE *dotout = fopen("out.m4", "w");
		FILE *dotdefs = fopen("out.inc", "w");
		fprintf(dotout, "digraph trace {\ninclude(`out.inc')\n");
		for(auto tr : traces) {
			printf("Trace of %s\n", tr->invoke);
			fprintf(dotout, "edge[weight=2, color=gray75, fillcolor=gray75];\n");

			fprintf(dotout, "start%d -> ", tr->id);
			fprintf(dotdefs, "start%d [label=\"%s\"];\n", tr->id, tr->invoke);
			for(auto e : tr->event_seq) {
				std::string sockinfo = "";
				if(auto sop = dynamic_cast<sockop *>(e->sc)) {
					if(sop->get_socket())
						sockinfo += "\n" + sock_name_short(sop->get_socket());
				}
				fprintf(dotout, "%c%d -> ", e->entry ? 'e' : 'x', e->sc->uuid);
				fprintf(dotdefs, "%c%d [label=\"%d:%s:%s%s\"];\n",
						e->entry ? 'e' : 'x', e->sc->uuid, tr->id, e->entry ? "entry" : "exit",
						syscall_names[e->sc->number],
						sockinfo.c_str());

				for(auto p : e->extra_parents) {
					fprintf(dotdefs, "%c%d -> %c%d;\n",
							p->entry ? 'e' : 'x', p->sc->uuid,
							e->entry ? 'e' : 'x', e->sc->uuid);
				}
			}
			fprintf(dotout, "exit%d;\n", tr->id);
			fprintf(dotdefs, "exit%d [label=\"Exit code=%d\"];\n", tr->id, tr->ecode);
			printf(" Exited with %d\n", tr->ecode);
		}
		fprintf(dotout, "\n}\n");
		fclose(dotdefs);
		fclose(dotout);
	}
	return 0;
}

