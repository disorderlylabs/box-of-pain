#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <errno.h>
#include <functional>
#include <netinet/in.h>
#include <signal.h>
#include <string.h>
#include <string>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>

#include "helper.h"
#include "run.h"
#include "scnames.h"
#include "sockets.h"
#include "sys.h"
#include "tracee.h"
#define LOG_SYSCALLS

#ifndef PTRACE_EVENT_STOP
#define PTRACE_EVENT_STOP 128
#endif

/* TODO: we can get all of the registers in one syscall. Maybe do that instead of PEEKUSER? */

struct options options = { .wait = false, .dump = false, .step = false, .log_syscalls = false };

enum opmode {
	OPMODE_TRACE,
	OPMODE_FOLLOW,
} current_mode = OPMODE_TRACE;

struct run current_run;

struct thread_tr *find_tracee(int tid)
{
	auto trit = current_run.traces.find(tid);
	if(trit != current_run.traces.end()) {
		struct thread_tr *tracee = trit->second;
		return tracee;
	} else {
		return NULL;
	}
}
/* this is just a convoluted way to "make" new syscall objects, by
 * letting us call constructors in an array based on syscall numbers.
 * See the top of main() for how these are assigned. */
template<typename T>
Syscall *make(int fpid, long n)
{
	return new T(fpid, n);
}
Syscall *(*syscallmap[1024])(int, long) = {};

template<typename T>
Syscall *make_inactive()
{
	return new T();
}
Syscall *(*syscallmap_inactive[1024])() = {};

// A means to end the process and dump out the graph, in case no elegant way to do so exists in the
// tracees.
volatile bool keyboardinterrupt = false;
void keyboardinterrupthandler(int signum __attribute__((unused)))
{
	keyboardinterrupt = true;
}

/* wait for a tracee to be ready to report a syscall. There is no
 * explicit order guaranteed by this. It could be the same process
 * twice in a row. We'd probably _like_ to see round-robin, but that
 * may not be feasible (processes may block for long periods of time). */
struct thread_tr *wait_for_syscall(void)
{
	int status;
	int tid;
	while(!keyboardinterrupt) {
		/* wait for any process */
		if((tid = waitpid(-1, &status, 0)) == -1) {
			return NULL;
		}

		struct thread_tr *tracee = find_tracee(tid);
		if(tracee == NULL) {
			fprintf(stderr, "waitpid returned untraced process/thread %d!\n", tid);
			continue;
			/*
			//If waitpid returns a process not in the map
			if(status>>8 == (SIGTRAP | (PTRACE_EVENT_STOP<<8))){
			//If it's a new thread, keep going, we'll get to it later, when we find the clone()
			fprintf(stderr, "waitpid returned new thread %d\n", tid);
			continue;
			}else {
			//Otherwise, something went wrong
			fprintf(stderr, "waitpid returned untraced process/thread %d!\n", tid);
			continue;
			//exit(1);
			}
			*/
		}

		tracee->status = status;
		if(WIFSTOPPED(status) && WSTOPSIG(status) & 0x80) {
			/* got a trace event (syscall entry or exit) */
			return tracee;
		}
		if(WIFEXITED(status)) {
			/* tracee exited. Cleanup. */
			tracee->proc->exited = true;
			tracee->proc->ecode = WEXITSTATUS(status);
			return tracee;
		}
		/* otherwise, tell the tracee to continue until it hits a syscall */
		ptrace(PTRACE_SYSCALL, tracee->tid, 0, 0);
	}
	return NULL;
}

void process_event(bool traced, struct thread_tr *tracee)
{
	/* depending on the opmode, we're either following along or tracing a new
	 * graph. */

	if(current_mode == OPMODE_FOLLOW) {
		bool felloff = followrun_step(tracee);
		if(felloff) {
			current_mode = OPMODE_TRACE;
		}
	}
	if(options.step && traced) {
		followrun_dumpall();
		getchar();
	}
}

int do_trace()
{
	/* initialize the tracees. We "continue" them in order by getting their
	 * status (should be blocked on their SIGSTOP from when they kick off)
	 * and then telling them to continue until they hit a syscall */

	for(auto tr : current_run.thread_list) {
		if(options.log_run)
			fprintf(stderr, "init trace on %d\n", tr->tid);
		int status;
		tr->sysnum = -1;

		current_run.traces[tr->tid] = tr;

		// Begin tracing
		if(ptrace(PTRACE_SEIZE, tr->tid, 0, 0) != 0) {
			perror("PTRACE_SEIZE");
		}

		// Ensure tracing was successful
		if(waitpid(tr->tid, &status, 0) == -1) {
			perror("waitpid");
		}

		// Set option to make tracing calls easier
		// Enable tracing on child threads
		if(ptrace(PTRACE_SETOPTIONS, tr->tid, 0, PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACECLONE)
		   != 0) {
			perror("ptrace SETOPTIONS");
		}

		// Continue execution until the next syscall
		if(ptrace(PTRACE_SYSCALL, tr->tid, 0, 0) != 0) {
			perror("ptrace SYSCALL");
		}
	}

	unsigned num_exited = 0;
	while(true) {
		struct thread_tr *tracee;
		if((tracee = wait_for_syscall()) == NULL)
			break;

		if(tracee->proc->exited) {
			num_exited++;
			if(options.log_run)
				fprintf(stderr,
				  "Exit %d (tid %d) exited: %d\n",
				  tracee->id,
				  tracee->tid,
				  tracee->proc->ecode);
			if(num_exited == current_run.thread_list.size())
				break;
			continue;
		}

		/* this call will ensure that we always know where a syscall instruction is, in case
		 * we need to inject a syscall. */
		register_syscall_rip(tracee);

		bool traced = false;
		if(tracee->sysnum == -1) {
			/* we're seeing an ENTRY to a syscall here. This ptrace gets the syscall number. */
			errno = 0;
			tracee->sysnum = ptrace(PTRACE_PEEKUSER, tracee->tid, sizeof(long) * ORIG_RAX);
			if(errno != 0)
				break;
			if(options.log_syscalls) {
				fprintf(stderr,
				  "[%d: %d]: %s entry\n",
				  tracee->id,
				  tracee->tid,
				  syscall_names[tracee->sysnum]);
			}
			tracee->syscall = NULL;
			if(syscallmap[tracee->sysnum]) {
				traced = true;
				/* we're tracking this syscall. Instantiate a new one. */
				tracee->syscall = syscallmap[tracee->sysnum](tracee->tid, tracee->sysnum);
				tracee->syscall->start();
				class event *e =
				  new event(tracee->syscall, true, tracee->id, tracee->event_seq.size());
				/* create the entry and exit event */
				tracee->syscall->entry_event = e;
				tracee->event_seq.push_back(e);
				tracee->proc->event_seq.push_back(e);
				int &eid = e->uuid;
				e = new event(tracee->syscall, false, tracee->id, -1);
				tracee->syscall->exit_event = e;
				e->pending = true;

				tracee->syscall->uuid = current_run.syscall_list.size();
				tracee->syscall->localid =
				  std::to_string(tracee->id) + std::to_string(tracee->proc->event_seq.size());
				current_run.syscall_list.push_back(tracee->syscall);
				if(options.log_syscalls)
					fprintf(
					  stderr, "  ++ event_id=%d, syscall_id=%d\n", eid, tracee->syscall->uuid);
			}

		} else {
			/* we're seeing an EXIT from a syscall. This ptrace gets the return value */
			errno = 0;
			long retval = ptrace(PTRACE_PEEKUSER, tracee->tid, sizeof(long) * RAX);
			if(errno != 0)
				break;
			if(options.log_syscalls) {
				fprintf(stderr,
				  "[%d: %d]: %s exit -> %ld\n",
				  tracee->id,
				  tracee->tid,
				  syscall_names[tracee->sysnum],
				  retval);
			}
			if(tracee->syscall) {
				traced = true;
				/* this syscall was tracked. Finish it up */
				tracee->syscall->retval = retval;
				tracee->syscall->state = STATE_DONE;

				tracee->syscall->exit_event->pending = false;
				tracee->syscall->exit_event->uuid = tracee->event_seq.size();
				tracee->event_seq.push_back(tracee->syscall->exit_event);
				tracee->proc->event_seq.push_back(tracee->syscall->exit_event);

				if(options.log_syscalls) {
					fprintf(stderr,
					  " ++ event_id=%d, syscall_id=%d\n",
					  tracee->syscall->exit_event->uuid,
					  tracee->syscall->uuid);
				}
				tracee->syscall->finish();
			}
			if(tracee->sysnum == SYS_execve && tracee->syscall_rip == (uint64_t)-1) {
				/* tracee has executed, start tracking. We want to ignore everything before this
				 * point because it's actually our code. Concretely, it means that syscall_rip will
				 * be wrong if we set it too early. */
				tracee->syscall_rip = 0;
			}
			tracee->sysnum = -1;
		}

		process_event(traced, tracee);

		/* ...and continue */
		ptrace(PTRACE_SYSCALL, tracee->tid, 0, 0);
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

#define SETSYS(s)                                                                                  \
	({                                                                                             \
		syscallmap[SYS_##s] = make<Sys##s>;                                                        \
		syscallmap_inactive[SYS_##s] = make_inactive<Sys##s>;                                      \
	})

int main(int argc, char **argv)
{
	/* this is how you indicate you want to track syscalls. You must
	 * also implement a Sys<whatever> class (e.g. Sysconnect) */
	SETSYS(recvfrom);
	SETSYS(sendto);
	SETSYS(read);
	SETSYS(write);
	SETSYS(accept);
	SETSYS(accept4);
	SETSYS(connect);
	SETSYS(bind);
	SETSYS(clone);

	signal(SIGINT, keyboardinterrupthandler);

	current_run.name = "current";

	enum modes { MODE_C, MODE_T, MODE_NULL, MODE_R };
	char *serialize_run = NULL;
	int containerization = MODE_NULL; // 0 on init, 1 on containers, 2 on tracer, -1 on regular mode
	int r;
	while((r = getopt(argc, argv, "e:dhTCfs:r:wSl:")) != EOF) {
		FILE *rf;
		run *run;
		switch(r) {
			case 'w':
				options.wait = true;
				break;
			case 'r':
				rf = fopen(optarg, "r");
				if(rf == NULL) {
					fprintf(stderr, "cannot open runfile %s\n", optarg);
					exit(255);
				}
				if(options.log_run)
					fprintf(stderr, "loading runfile %s\n", optarg);
				run = new struct run();
				run->name = strdup(optarg);
				run_load(run, rf);
				fclose(rf);
				followrun_add(run);
				current_mode = OPMODE_FOLLOW;

				// dump("run", run);
				// exit(0);
				break;
			case 'e': {
				if(containerization == MODE_NULL) {
					containerization = MODE_R;
				}
				if(containerization != MODE_R) {
					usage();
					return 255;
				}
				struct thread_tr *tr = new thread_tr();
				struct proc_tr *ptr = new proc_tr();
				tr->id = current_run.thread_list.size();
				ptr->id = current_run.proc_list.size();
				tr->sysnum = -1; // we're not in a syscall to start.
				tr->syscall_rip = -1;
				tr->shared_page = 0;
				tr->sp_mark = 0;
				tr->active = true;
				tr->syscall = NULL;
				ptr->exited = false;
				tr->proc = ptr;
				ptr->invoke = strdup(optarg);
				current_run.thread_list.push_back(tr);
				ptr->proc_thread_list.push_back(tr);
				current_run.proc_list.push_back(ptr);
			} break;
			case 'h':
				usage();
				return 255;
			case 'd':
				options.dump = true;
				break;
			case 'C':
				if(options.log_run)
					printf("Entering containerized mode as: %s %u\n", argv[optind], getpid());
				containerization = MODE_C;
				r = EOF;
				break;
			case 'T':
				if(options.log_run)
					printf("Entering containerized mode as Tracer\n");
				containerization = MODE_T;
				break;
			case 's':
				serialize_run = optarg;
				current_run.name = optarg;
				break;
			case 'S':
				options.step = true;
				break;
			case 'l': {
				char *tok = NULL;
				while((tok = strtok(tok ? NULL : optarg, ","))) {
					if(!strcmp(tok, "sys")) {
						options.log_syscalls = true;
					} else if(!strcmp(tok, "sock")) {
						options.log_sockets = true;
					} else if(!strcmp(tok, "follow")) {
						options.log_follow = true;
					} else if(!strcmp(tok, "run")) {
						options.log_run = true;
					} else {
						fprintf(stderr, "Unknown logging type: %s\n", tok);
						exit(255);
					}
				}
			} break;
			default:
				usage();
				return 255;
		}
	}

	switch(containerization) {
		case MODE_NULL:
			usage();
			return 255;
		case MODE_R:
			for(auto tr : current_run.thread_list) {
				/* parse the args, and start the process */
				char **args = (char **)calloc(2, sizeof(char *));
				char *prog = strdup(strtok(tr->proc->invoke, ","));
				args[0] = prog;
				char *tmp;
				int ac = 1;
				while((tmp = strtok(NULL, ","))) {
					args = (char **)realloc(args, (ac + 2) * sizeof(char *));
					args[ac] = strdup(tmp);
					args[ac + 1] = NULL;
					ac++;
				}
				int pid = fork();
				if(pid == 0) {
					// if(ptrace(PTRACE_TRACEME)!= 0){
					//	perror("PTRACE_TRACEME");
					//}
					/* wait for the tracer to get us going later (in do_trace) */
					raise(SIGSTOP);
					if(execvp(prog, args) == -1) {
						fprintf(stderr, "failed to execute %s\n", prog);
					}
					exit(255);
				}
				tr->proc->pid = pid;
				tr->tid = pid;
				if(options.log_run)
					fprintf(stderr,
					  "Tracee %d: starting %s (tid %d)\n",
					  tr->id,
					  tr->proc->invoke,
					  tr->tid);
			}
			break;
		case MODE_T: {
			printf("press enter to read tracee directory\n");
			while(getchar() != 10) {
			}

			DIR *trdir = opendir("/tracees");
			if(!trdir) {
				perror("opendir");
				exit(255);
			}
			for(struct dirent *trdirit = readdir(trdir); trdirit != NULL;
			    trdirit = readdir(trdir)) {
				if(trdirit->d_type != DT_REG) {
					continue;
				}
				std::string path = std::string("/tracees/");
				path = path + trdirit->d_name;
				FILE *trfile = fopen(path.c_str(), "r");
				if(!trfile) {
					perror("fopen");
					printf("%s\n", trdirit->d_name);
					continue;
				}
				int pid = atoi(trdirit->d_name);
				char name[100];
				fgets(name, 100, trfile);
				printf("inserted %s %u\n", name, pid);
				fclose(trfile);
				struct thread_tr *tr = new thread_tr();
				struct proc_tr *ptr = new proc_tr();
				tr->id = current_run.proc_list.size();
				ptr->id = current_run.proc_list.size();
				tr->sysnum = -1; // we're not in a syscall to start.
				tr->syscall_rip = -1;
				tr->shared_page = 0;
				tr->active = true;
				tr->sp_mark = 0;
				tr->syscall = NULL;
				ptr->exited = false;
				ptr->invoke = strdup(name);
				tr->tid = pid;
				ptr->pid = pid;
				tr->proc = ptr;
				current_run.thread_list.push_back(tr);
				ptr->proc_thread_list.push_back(tr);
				current_run.proc_list.push_back(ptr);
			}
			closedir(trdir);
		}
			printf("done inserting containers\n");
			break;
		case MODE_C:
			// ptrace(PTRACE_TRACEME);
			{
				// Tell the tracer about us
				std::string path = std::string("/tracees/");
				path = path + std::to_string((int)getpid());
				FILE *pidfile = fopen(path.c_str(), "wx");
				if(!pidfile) {
					perror("painbox:");
					exit(255);
				}
				fprintf(pidfile, "%s", argv[optind]);
				fclose(pidfile);
			}
			/* wait for the tracer to get us going later (in do_trace) */
			raise(SIGSTOP);
			if(options.log_run)
				fprintf(stderr, "Running: %s %u\n", argv[optind], getpid());
			if(execvp(argv[optind], argv + optind) == -1) {
				fprintf(stderr, "failed to execute %s\n", argv[optind]);
			}
			exit(255);
			break;
		default:
			usage();
			return 255;
	}

	if(options.wait) {
		fprintf(stderr, "=== READY TO TRACE ===\n");
		fprintf(stderr, "press any key...\n");
		signal(SIGINT, SIG_DFL);
		getchar();
		signal(SIGINT, keyboardinterrupthandler);
	}

	do_trace();
	/* done tracing, collect errors */
	if(!keyboardinterrupt) {
		for(auto ptr : current_run.proc_list) {
			if(ptr->ecode != 0) {
				if(options.log_run)
					fprintf(stderr, "Tracee %d exited non-zero exit code\n", ptr->id);
			}
		}
	} else {
		fprintf(stderr, "Tracer recieved interrupt\n");
		return 255;
	}

	fflush(stderr);
	fflush(stdout);

	int pass = 0;
	bool more = true;
	/* if we have any post-processing, do that.
	 * NOTE: this code is deprecated; hopefully we can do everything in real-time */
	while(more) {
		if(options.log_run)
			fprintf(stderr, "post-processing pass %d\n", pass);
		more = false;
		for(auto tr : current_run.proc_list) {
			for(auto e : tr->event_seq) {
				if(e->entry) {
					more = e->sc->post_process(pass) || more;
				}
			}
		}
		pass++;
	}

	if(current_mode == OPMODE_TRACE && serialize_run) {
		FILE *sout = fopen(serialize_run, "w+");
		if(!sout) {
			fprintf(stderr, "failed to open run serialize file for writing\n");
		} else {
			run_serialize(&current_run, sout);
			fclose(sout);
		}
	}

	if(current_mode == OPMODE_TRACE && options.dump) {
		dump(current_run.name, &current_run);
	}
	return current_mode == OPMODE_FOLLOW ? 0 : 1;
}
