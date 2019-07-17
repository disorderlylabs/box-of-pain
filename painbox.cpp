#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
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
#include "sockets.h"
#include "sys.h"
#include "tracee.h"
#define LOG_SYSCALLS

#ifndef PTRACE_EVENT_STOP
#define PTRACE_EVENT_STOP 128
#endif

/* TODO: we can get all of the registers in one syscall. Maybe do that instead of PEEKUSER? */

struct options options = {};

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

void create_thread_from_clone(struct thread_tr *thread, int newtid, bool new_process)
{
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
		  "[%d : %d]: clone created new %s with tid %d, id %d :: proc %d has %ld "
		  "threads\n",
		  thread->id,
		  thread->tid,
		  new_process ? "process" : "thread",
		  newtid,
		  tr->id,
		  tr->proc->id,
		  tr->proc->num_threads);


}

/* wait for a tracee to be ready to report a syscall. There is no
 * explicit order guaranteed by this. It could be the same process
 * twice in a row. We'd probably _like_ to see round-robin, but that
 * may not be feasible (processes may block for long periods of time). */
#include <list>
static std::list<std::pair<int, int>> wait_queue;
struct thread_tr *wait_for_syscall(void)
{
	static size_t rr_c = 0;
	int status;
	int tid = -1;
	while(!keyboardinterrupt) {
		/* Find a thread to process. First, check the wait queue to see if we have any threads that
		 * we waited on but didn't yet 'know about'. */
		struct thread_tr *tracee = NULL;
		for(auto it = wait_queue.begin(); it != wait_queue.end(); it++) {
			int t = (*it).first;
			int s = (*it).second;
			tracee = find_tracee(t);
			if(tracee) {
				fprintf(stderr, "removing thread %d from wait_queue (status = %x).\n", t, s);
				wait_queue.erase(it);
				status = s;
				break;
			}

		}
		if(tracee == NULL) {
			/* we didn't find any that we were waiting on that we _could_ service yet. Try all threads in a semi-fair manner. */
			/* TODO: determine a better method */
			for(size_t i=0;i<current_run.thread_list.size();i++) {
				size_t ti = (rr_c+i) % current_run.thread_list.size();
				struct thread_tr *t = current_run.thread_list[ti];
				tid = waitpid(t->tid, &status, __WALL | WNOHANG);
				if(tid == t->tid) break;
				tid = -1;
				break;
			}
			rr_c++;
			if(tid == -1) {
				/* no threads found yet. wait and actually block this time */
				if((tid = waitpid(-1, &status, __WALL)) == -1) {
					return NULL;
				}
			}

			tracee = find_tracee(tid);
		}
		if(tracee == NULL) {
			fprintf(stderr, "waitpid returned untraced process/thread %d (status=%x). Enqueuing for later.\n", tid, status);
			assert(tid != -1);
			wait_queue.push_back(std::make_pair(tid, status));
			continue;
		}
		if((((status >> 16) & 0xffff) == PTRACE_EVENT_CLONE)
				|| (((status >> 16) & 0xffff) == PTRACE_EVENT_FORK)
				|| (((status >> 16) & 0xffff) == PTRACE_EVENT_VFORK)) {
			unsigned long newtid;
			long r = ptrace(PTRACE_GETEVENTMSG, tid, 0, &newtid);
			assert(newtid != tid);
			if(r == -1) {
				perror("trace geteventmsg");
			}
			create_thread_from_clone(tracee, newtid, ((status >> 16) & 0xffff) != PTRACE_EVENT_CLONE);
		}

		tracee->status = status;
		int signal = 0;
		if(WIFSTOPPED(status) && WSTOPSIG(status) & 0x80) {
			/* got a trace event (syscall entry or exit) */
			return tracee;
		} else if(WIFSTOPPED(status) && WSTOPSIG(status)) {
			signal = WSTOPSIG(status);
		}
		if(WIFEXITED(status)) {
			/* tracee exited. Cleanup. */
			if(options.log_run)
				fprintf(stderr,
				  "[%d : %d] thread exited; process %d has %ld threads\n",
				  tracee->id,
				  tracee->tid,
				  tracee->proc->id,
				  tracee->proc->num_threads - 1);
			if(--tracee->proc->num_threads == 0) {
				if(options.log_run)
					fprintf(stderr,
					  "[%d : %d] final thread exit in process %d\n",
					  tracee->id,
					  tracee->tid,
					  tracee->proc->id);
				tracee->proc->exited = true;
				tracee->proc->ecode = WEXITSTATUS(status);
			} else {
				continue;
			}
			return tracee;
		}
		/* otherwise, tell the tracee to continue until it hits a syscall */
		if(!tracee->frozen) {
			ptrace(PTRACE_SYSCALL, tracee->tid, 0, (long)signal);
		}
	}
	return NULL;
}

void freeze_thread(struct thread_tr *tracee)
{
	tracee->frozen = true;
}

void unfreeze_thread(struct thread_tr *tracee)
{
	tracee->frozen = false;
	/* if frozen, we need to signal the process to continue */
	ptrace(PTRACE_SYSCALL, tracee->tid, 0, 0);
}

void process_event(bool traced, struct thread_tr *tracee)
{
	/* depending on the opmode, we're either following along or tracing a new
	 * graph. */

	if(!traced) return;

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
		if(ptrace(PTRACE_SETOPTIONS, tr->tid, 0, PTRACE_O_TRACESYSGOOD | PTRACE_O_TRACECLONE | PTRACE_O_EXITKILL | PTRACE_O_TRACEFORK | PTRACE_O_TRACEVFORK)
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
#if 1
		/* this call will ensure that we always know where a syscall instruction is, in case
		 * we need to inject a syscall. */
		register_syscall_rip(tracee);

		bool traced = false;
		if(tracee->sysnum == -1) {
			/* we're seeing an ENTRY to a syscall here. This ptrace gets the syscall number. */
			errno = 0;
			tracee->sysnum = ptrace(PTRACE_PEEKUSER, tracee->tid, sizeof(long) * ORIG_RAX);
			if(errno != 0) {
				warn("ptrace peek");
				break;
			}
			if(options.log_syscalls) {
				fprintf(stderr,
				  "[%d: %d]: %s entry\n",
				  tracee->id,
				  tracee->tid,
				  syscall_table[tracee->sysnum].name);
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
						if(errno != 0) {
				warn("ptrace peek");
				break;
			}
			if(options.log_syscalls) {
				fprintf(stderr,
				  "[%d: %d]: %s exit -> %ld\n",
				  tracee->id,
				  tracee->tid,
				  syscall_table[tracee->sysnum].name,
				  retval);
			}
if(retval == -38) {
				fprintf(stderr, "ERR: ENOSYS\n");
				abort();
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
#endif

		/* ...and continue */
		if(!tracee->frozen)
			ptrace(PTRACE_SYSCALL, tracee->tid, 0, 0);
	}
	return 0;
}

void alarm_handler(int s)
{
	(void)s;
	for(auto tr : current_run.thread_list) {
		if(tr->delay >= 0) {
			tr->delay -= 1000;
			if(tr->delay < 0) {
				fprintf(stderr, "starting delayed execution of %s\n", tr->proc->invoke);
				unfreeze_thread(tr);
			}
			ualarm(1000, 0);
		}
	}
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

#define SETSYS2(num, s)                                                                            \
	({                                                                                             \
		syscallmap[SYS_##num] = make<Sys##s>;                                                      \
		syscallmap_inactive[SYS_##num] = make_inactive<Sys##s>;                                    \
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
	SETSYS2(accept4, accept);
	SETSYS(connect);
	SETSYS(bind);
	SETSYS(clone);

	signal(SIGINT, keyboardinterrupthandler);

	current_run.name = "current";

	enum modes { MODE_C, MODE_T, MODE_NULL, MODE_R };
	char *serialize_run = NULL;
	int containerization = MODE_NULL; // 0 on init, 1 on containers, 2 on tracer, -1 on regular mode
	int r;
	while((r = getopt(argc, argv, "e:dhTCfs:r:R:wSl:")) != EOF) {
		FILE *rf;
		run *run;
		switch(r) {
			case 'w':
				options.wait = true;
				break;
			case 'R':
			case 'r':
				rf = fopen(optarg, "r");
				if(rf == NULL) {
					fprintf(stderr, "cannot open runfile %s\n", optarg);
					exit(255);
				}
				if(options.log_run)
					fprintf(stderr, "loading runfile %s\n", optarg);
				run = new class run();
				run->name = strdup(optarg);
				run_load(run, rf);
				fclose(rf);
				followrun_add(run);
				current_mode = OPMODE_FOLLOW;

				if(r == 'R') {
					dump("run", run);
					exit(0);
				}
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
					} else if(!strcmp(tok, "stats")) {
						options.follow_stats = true;
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
				int outfd = -1, infd = -1;
				while((tmp = strtok(NULL, ","))) {
					if(!strcmp(tmp, ">")) {
						tmp = strtok(NULL, ",");
						if(!tmp) {
							fprintf(stderr, "invalid syntax: need '> filename'.");
							exit(1);
						}
						outfd = open(tmp, O_WRONLY);
					} else if(!strcmp(tmp, "<")) {
						tmp = strtok(NULL, ",");
						if(!tmp) {
							fprintf(stderr, "invalid syntax: need '> filename'.");
							exit(1);
						}
						infd = open(tmp, O_RDONLY | O_NONBLOCK);
						const int flags = fcntl(infd, F_GETFL, 0);
						fcntl(infd, F_SETFL, flags & ~O_NONBLOCK);
					} else if(tmp[0] == '!') {
						float delay;
						if(sscanf(tmp + 1, "%f", &delay) != 1) {
							fprintf(stderr, "syntax err: need !%%f\n");
							exit(1);
						}
						fprintf(stderr,
						  "delaying execution of process %s by %d usec (%f)\n",
						  prog,
						  (int)(delay * 1000000),
						  delay);
						signal(SIGALRM, alarm_handler);
						alarm((int)delay);
						tr->delay = delay; // * 1000000;
						freeze_thread(tr);
					} else {
						args = (char **)realloc(args, (ac + 2) * sizeof(char *));
						args[ac] = strdup(tmp);
						args[ac + 1] = NULL;
						ac++;
					}
				}
				int pid = fork();
				if(pid == 0) {
					// if(ptrace(PTRACE_TRACEME)!= 0){
					//	perror("PTRACE_TRACEME");
					//}
					/* wait for the tracer to get us going later (in do_trace) */
					if(outfd != -1) {
						close(1);
						dup(outfd);
						close(outfd);
					}
					if(infd != -1) {
						close(0);
						dup(infd);
						close(infd);
					}
					raise(SIGSTOP);
					if(execvp(prog, args) == -1) {
						fprintf(stderr, "failed to execute %s\n", prog);
					}
					exit(255);
				}
				if(outfd != -1)
					close(outfd);
				if(infd != -1)
					close(infd);
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
		// return 255;
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

	if(options.follow_stats) {
		followrun_stats();
	}

	if(current_mode == OPMODE_TRACE && options.dump) {
		dump(current_run.name, &current_run);
	}

	bool clean = true;
	for(auto p : current_run.proc_list) {
		if(p->ecode) {
			clean = false;
		}
	}

	return (current_mode == OPMODE_FOLLOW ? 0 : 1) | (clean ? 0 : 2);
}
