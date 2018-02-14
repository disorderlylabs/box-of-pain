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

/* TODO: we can get all of the registers in one syscall. Maybe do that instead of PEEKUSER? */

struct options {
	bool dump;
} options = {.dump = false};

/* list of all traced processes in the system */
std::vector<struct trace *> traces;
/* list of all syscalls that happen, in order that they are observed */
std::vector<Syscall *> syscall_list;

struct trace *find_tracee(int pid)
{
	for(auto t : traces) {
		if(t->pid == pid)
			return t;
	}
	return NULL;
}

/* this is just a convoluted way to "make" new syscall objects, by
 * letting us call constructors in an array based on syscall numbers.
 * See the top of main() for how these are assigned. */
template <typename T>
Syscall * make(int fpid, long n) { return new T(fpid, n); }
Syscall * (*syscallmap[1024])(int, long) = { };

/* wait for a tracee to be ready to report a syscall. There is no
 * explicit order guaranteed by this. It could be the same process
 * twice in a row. We'd probably _like_ to see round-robin, but that
 * may not be feasible (processes may block for long periods of time). */
struct trace *wait_for_syscall(void)
{
	int status;
	while(1) {
		int pid;
		/* wait for any process */
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
			/* got a trace event (syscall entry or exit) */
			return tracee;
		}
		if(WIFEXITED(status)) {
			/* tracee exited. Cleanup. */
			tracee->exited = true;
			tracee->ecode = WEXITSTATUS(status);
			return tracee;
		}
		/* otherwise, tell the tracee to continue until it hits a syscall */
		ptrace(PTRACE_SYSCALL, tracee->pid, 0, 0);
	}
}

int do_trace()
{
	/* initialize the tracees. We "continue" them in order by getting their
	 * status (should be blocked on their SIGSTOP from when they kick off)
	 * and then telling them to continue until they hit a syscall */
	for(auto tr : traces) {
		fprintf(stderr, "init trace on %d\n", tr->pid);
		int status;
		tr->sysnum = -1;
		if(ptrace(PTRACE_SEIZE, tr->pid, 0, 0) != 0){
			perror("PTRACE_SEIZE");
		}
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

		/* this call will ensure that we always know where a syscall instruction is, in case
		 * we need to inject a syscall. */
		register_syscall_rip(tracee);

		if(tracee->sysnum == -1) {
			/* we're seeing an ENTRY to a syscall here. This ptrace gets the syscall number. */
			tracee->sysnum = ptrace(PTRACE_PEEKUSER, tracee->pid, sizeof(long)*ORIG_RAX);
			if(errno != 0) break;

#if LOG_SYSCALLS
			fprintf(stderr, "[%d]: %s entry\n", tracee->id, syscall_names[tracee->sysnum]);
#endif
			tracee->syscall = NULL;
			if(syscallmap[tracee->sysnum]) {
				/* we're tracking this syscall. Instantiate a new one. */
				tracee->syscall = syscallmap[tracee->sysnum](tracee->pid, tracee->sysnum);
				tracee->syscall->start();
				class event *e = new event(tracee->syscall, true);
				/* create the entry and exit event */
				tracee->syscall->entry_event = e;
				tracee->event_seq.push_back(e);
				e = new event(tracee->syscall, false);
				tracee->syscall->exit_event = e;

				tracee->syscall->uuid = syscall_list.size();
				tracee->syscall->localid = std::to_string(tracee->id) + std::to_string(tracee->event_seq.size());
				syscall_list.push_back(tracee->syscall);
			}
		} else {
			/* we're seeing an EXIT from a syscall. This ptrace gets the return value */
			long retval = ptrace(PTRACE_PEEKUSER, tracee->pid, sizeof(long)*RAX);
			if(errno != 0) break;
#if LOG_SYSCALLS
			fprintf(stderr, "[%d]: %s exit -> %d\n", tracee->id, syscall_names[tracee->sysnum], retval);
#endif
			if(tracee->syscall) {
				/* this syscall was tracked. Finish it up */
				tracee->syscall->retval = retval;
				tracee->syscall->state = STATE_DONE;
				tracee->event_seq.push_back(tracee->syscall->exit_event);
				tracee->syscall->finish();
			}
			if(tracee->sysnum == SYS_execve && tracee->syscall_rip == -1) {
				/* tracee has executed, start tracking. We want to ignore everything before this point
				 * because it's actually our code. Concretely, it means that syscall_rip will be wrong
				 * if we set it too early. */
				tracee->syscall_rip = 0;
			}
			tracee->sysnum = -1;
		}
		/* ...and continue */
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
	/* this is how you indicate you want to track syscalls. You must
	 * also implement a Sys<whatever> class (e.g. Sysconnect) */
	SETSYS(recvfrom);
	SETSYS(sendto);
	SETSYS(read);
	SETSYS(write);
	SETSYS(accept);
	SETSYS(connect);
	SETSYS(bind);

	int containerization = 0; //0 on init, 1 on containers, 2 on tracer, -1 on regular mode
	int r;
	while((r = getopt(argc, argv, "e:dhTC")) != EOF) {
		switch(r) {
			case 'e':
				{
					if(containerization==0) containerization = -1;
					if(containerization!=-1) {usage(); return 1;}
					struct trace *tr = new trace();
					tr->id = traces.size();
					tr->sysnum = -1; //we're not in a syscall to start.
					tr->syscall_rip = -1;
					tr->shared_page = 0;
					tr->sp_mark = 0;
					tr->syscall = NULL;
					tr->exited = false;
					tr->invoke = strdup(optarg);
					traces.push_back(tr);
				} break;
			case 'h':
				usage();
				return 0;
			case 'd':
				if(containerization==1) {usage(); return 1;}
				options.dump = true;
				break;
			case 'C':
				printf("Entering containerized mode as: %s %u\n", argv[optind], getpid());
				containerization = 1;
				r = EOF;			
				break;
			case 'T':
				printf("Entering containerized mode as Tracer\n");
				containerization = 2;
				break;
			default:
				usage();
				return 1;
		}
	}

	switch(containerization){
		case 0:
			usage();
			return 1;
		case -1:
			for(auto tr : traces) {
				/* parse the args, and start the process */
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
					//if(ptrace(PTRACE_TRACEME)!= 0){
					//	perror("PTRACE_TRACEME");
					//}
					/* wait for the tracer to get us going later (in do_trace) */
					raise(SIGSTOP);
					if(execvp(prog, args) == -1) {
						fprintf(stderr, "failed to execute %s\n", prog);
					}
					exit(255);
				}
				tr->pid = pid;
				fprintf(stderr, "Tracee %d: starting %s (pid %d)\n", tr->id, tr->invoke, tr->pid);
			}
			break;
		case 2:
			{
				int pid;
				char line[30] = {0};
				char name[20] = {0};
				printf("insert a \"name pid\" pair\n");
				while( (fgets(line, 30, stdin )) != NULL )
				{
					sscanf(line,"%20s %u ", name, &pid);
					printf("inserted %s %u\n", name, pid);
					struct trace *tr = new trace();
					tr->id = traces.size();
					tr->sysnum = -1; //we're not in a syscall to start.
					tr->syscall_rip = -1;
					tr->shared_page = 0;
					tr->sp_mark = 0;
					tr->syscall = NULL;
					tr->exited = false;
					tr->invoke = strdup(name);
					tr->pid = pid;
					traces.push_back(tr);
				}
			}
			printf("done inserting containers\n");
			break;
		case 1:
			//ptrace(PTRACE_TRACEME);
			/* wait for the tracer to get us going later (in do_trace) */
			raise(SIGSTOP);
			printf("Running: %s %u\n", argv[optind], getpid());
			if(execvp(argv[optind], argv + optind) == -1) {
				fprintf(stderr, "failed to execute %s\n", argv[optind]);
			}
			exit(255);
		default:
			usage();
			return 1;
	}
	do_trace();
	/* done tracing, collect errors */
	for(auto tr : traces) {
		if(tr->ecode != 0) {
			fprintf(stderr, "Tracee %d exited non-zero exit code\n", tr->id);
		}
	}

	int pass = 0;
	bool more = true;
	/* if we have any post-processing, do that.
	 * NOTE: this code is deprecated; hopefully we can do everything in real-time */
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
		/* output to a dotout and dotdefs file. The dotdefs is a file that you can write
		 * fully new entries to each iteration. The dotout file is a file that gets entries
		 * written to over the course of the loop. They form m4 file, where the dotdefs file
		 * gets included. To view, run `m4 out.m4 && dot -Tpdf -o out.pdf out.dot` */
		FILE *dotout = fopen("out.m4", "w");
		FILE *dotdefs = fopen("out.inc", "w");
		fprintf(dotout, "digraph trace {\ninclude(`out.inc')\n");
		fprintf(dotdefs, "rankdir=TB\nsplines=line\noutputorder=nodesfirst\n");
		for(auto tr : traces) {
			printf("Trace of %s\n", tr->invoke);
			fprintf(dotout, "edge[weight=2, color=gray75, fillcolor=gray75];\n");

			fprintf(dotout, "start%d -> ", tr->id);
			fprintf(dotdefs, "start%d [label=\"%s\",style=\"filled\",fillcolor=\"#1111aa22\"];\n", tr->id, tr->invoke);
			for(auto e : tr->event_seq) {
				std::string sockinfo = "";
				if(auto sop = dynamic_cast<sockop *>(e->sc)) {
					if(sop->get_socket())
						sockinfo += "" + sock_name_short(sop->get_socket());
				}
				fprintf(dotout, "%c%s -> ", e->entry ? 'e' : 'x', e->sc->localid.c_str());
				for(auto p : e->extra_parents) {
					fprintf(dotdefs, "%c%s -> %c%s [constraint=\"true\"];\n",
							p->entry ? 'e' : 'x', p->sc->localid.c_str(),
							e->entry ? 'e' : 'x', e->sc->localid.c_str());
				}

				if(e->entry) {
					//fprintf(dotdefs, "subgraph cluster_%d_%s {group=\"G%d\";\tlabel=\"%s\";\n\tgraph[style=dotted];\n",
					//		tr->id, e->sc->localid.c_str(), tr->id, syscall_names[e->sc->number]);
					fprintf(dotdefs, "e%s [label=\"%d:entry:%s:%s%s\",group=\"G%d\",fillcolor=\"%s\",style=\"filled\"];\n",
						e->sc->localid.c_str(), tr->id,
						syscall_names[e->sc->number], "",
						sockinfo.c_str(), tr->id, "#00ff0011");

					fprintf(dotdefs, "x%s [label=\"%d:exit:%s:%s%s\",group=\"G%d\",fillcolor=\"%s\",style=\"filled\"];\n",
						e->sc->localid.c_str(), tr->id,
						syscall_names[e->sc->number], std::to_string((long)e->sc->retval).c_str(),
						sockinfo.c_str(), tr->id, "#ff000011");

					//fprintf(dotdefs, "}\n");


				}
			}
			fprintf(dotout, "exit%d;\n", tr->id);
			fprintf(dotdefs, "exit%d [label=\"Exit code=%d\",style=\"filled\",fillcolor=\"%s\"];\n",
					tr->id, tr->ecode, tr->ecode == 0 ? "#1111aa22" : "#ff111188");
			printf(" Exited with %d\n", tr->ecode);
		}
		fprintf(dotout, "\n}\n");
		fclose(dotdefs);
		fclose(dotout);
	}
	return 0;
}

