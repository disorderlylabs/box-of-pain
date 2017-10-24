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

#define LOG_SYSCALLS 0

struct options {
	bool dump;
} options = {.dump = false};

class Syscall;
class event;
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

class event {
	public:
	Syscall *sc;
	bool entry;
	event(Syscall *s, bool e) : sc(s), entry(e) {}
};

std::vector<struct trace *> traces;

struct trace *find_tracee(int pid)
{
	for(auto t : traces) {
		if(t->pid == pid)
			return t;
	}
	return NULL;
}

#define MAX_PARAMS 6 /* linux has 6 register parameters */

int param_map[MAX_PARAMS] = {
	RDI,
	RSI,
	RDX,
	R10,
	R8,
	R9
};

enum syscall_state {
	STATE_UNCALLED,
	STATE_CALLED,
	STATE_DONE,
};

int set_syscall_param(int pid, int reg, long value)
{
	return ptrace(PTRACE_POKEUSER, pid, sizeof(long)*reg, value);
}


class Syscall {
	public:
		class event *entry_event, *exit_event;
		int frompid;
		int uuid;
		unsigned long number;
		unsigned long params[MAX_PARAMS];
		unsigned long retval;
		bool ret_success = false;
		enum syscall_state state;

		Syscall(int fpid, long num) {
			frompid = fpid;
			state = STATE_CALLED;
			number = num;
			for(int i=0;i<MAX_PARAMS;i++) {
				params[i] = ptrace(PTRACE_PEEKUSER, frompid, sizeof(long)*param_map[i]);
			}
		}

		virtual void finish() {
			if(ret_success) {
				set_syscall_param(frompid, RAX, 0);
			}
		}
		virtual void start() { }

		virtual bool operator ==(const Syscall &other) const {
			return number == other.number;
		}
};

class sockop {
	public:
		class sock *sock;
		class sock *get_socket() { return sock; }
};

std::vector<Syscall *> syscall_list;
/* TODO: check for error return values. Nonblocking? */

class Sysclose : public Syscall, public sockop {
	public:
		Sysclose(int p, long n) : Syscall(p, n) {}
		void start() {
			int fd = params[0];
			sock = sock_lookup(frompid, fd);
			sock_close(frompid, fd);
		}
};

class Sysbind : public Syscall, public sockop {
	public:
		struct sockaddr addr;
		socklen_t len;
		Sysbind(int p, long n) : Syscall(p, n) {}
		void start() {
			int sockfd = params[0];
			GETOBJ(frompid, params[1], &addr);
			len = params[2];
			sock = sock_assoc(frompid, sockfd, "", &addr, len);
		};
};

class Sysaccept : public Syscall, public sockop {
	public:
		class Sysconnect *pair;
		struct sock *serversock = NULL;
		struct sockaddr addr;
		socklen_t len;
		Sysaccept(int p, long n) : Syscall(p, n) {}
		void start() {
			int fd = params[0];
			serversock = sock_lookup(frompid, fd);
		}
		void finish() {
			int sockfd = retval;
			if(sockfd >= 0) {
				len = GET(socklen_t, frompid, params[2]);
				GETOBJ(frompid, params[1], &addr);
				sock = sock_assoc(frompid, sockfd, "", &addr, len);
			}
		}
};

class Sysconnect : public Syscall, public sockop {
	public:
		class Sysaccept *pair;
		struct sockaddr addr;
		socklen_t len;
		Sysconnect(int p, long n) : Syscall(p, n) {}
		void start() {
			int sockfd = params[0];
			GETOBJ(frompid, params[1], &addr);
			len = params[2];
			sock = sock_assoc(frompid, sockfd, "", &addr, len);
		} 
		void finish() { }
};

class Syswrite : public Syscall, public sockop {
	public:
		Syswrite(int p, long n) : Syscall(p, n) {}
		void start() { 
			sock = sock_lookup(frompid, params[0]);
			if(!sock) {
				return;
			}
			fprintf(stderr, "[%d]: SOCKET %-26s WRITE enter\n",
					find_tracee(frompid)->id, sock_name(sock).c_str());
#if 0
			if(find_tracee(frompid)->id == 0) {
				static int _t = 0;
				if(_t != 1) {
					_t = 1;
				} else if(1){
					set_syscall_param(frompid, RDI, -1);
					ret_success = true;
				}
			}
#endif
		} 
		void finish() {
			if(ret_success) {
				set_syscall_param(frompid, RAX, params[2]);
			}
		}
};

class Sysread : public Syscall, public sockop {
	public:
		Sysread(int fpid, long n) : Syscall(fpid, n) {}
		void start() {
			sock = sock_lookup(frompid, params[0]);
			if(!sock) {
				return;
			}
			fprintf(stderr, "[%d]: SOCKET %-26s READ  enter\n",
					find_tracee(frompid)->id, sock_name(sock).c_str());
		}
		void finish() {
			if(!sock) {
				return;
			}
			fprintf(stderr, "[%d]: SOCKET %-26s READ  retur\n",
					find_tracee(frompid)->id, sock_name(sock).c_str());
		}
};

class Sysrecvfrom : public Syscall, public sockop {
	public:
		int socket;
		void *buffer;
		size_t length;
		int flags;
		struct sockaddr *addr;
		socklen_t *addrlen;

		Sysrecvfrom(int fpid, long n) : Syscall(fpid, n) {}

		void start() {
			socket = params[0];
			buffer = (void *)params[1];
			length = params[2];
			flags = params[3];
			addr = (struct sockaddr *)params[4];
			addrlen = (socklen_t *)params[5];
		}

		bool operator ==(const Sysrecvfrom &other) const {
			/* Simple "fuzzy" comparison: socket is the same */
			/* TODO: check addr for source, etc */
			return socket == other.socket;
		}

		void finish() { }
};

/* HACK: there are not this many syscalls, but there is no defined "num syscalls" to
 * use. */
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
				tracee->syscall->uuid = syscall_list.size();
				syscall_list.push_back(tracee->syscall);
			}
		} else {
			int retval = ptrace(PTRACE_PEEKUSER, tracee->pid, sizeof(long)*RAX);
			if(errno != 0) break;
#if LOG_SYSCALLS
			fprintf(stderr, "[%d]: %s exit -> %d\n", tracee->id, syscall_names[tracee->sysnum], retval);
#endif
			if(tracee->syscall) {
				tracee->syscall->retval = retval;
				tracee->syscall->state = STATE_DONE;
				tracee->syscall->finish();
				class event *e = new event(tracee->syscall, false);
				tracee->syscall->exit_event = e;
				tracee->event_seq.push_back(e);
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

	if(options.dump) {
		int uid = 0;
		FILE *dotout = fopen("out.m4", "w");
		FILE *dotdefs = fopen("out.inc", "w");
		fprintf(dotout, "digraph trace {\ninclude(`out.inc')\n");
	//	fprintf(dotdefs, "3 [label=\"FOO\"];\n");
		for(auto tr : traces) {
			printf("Trace of %s\n", tr->invoke);
			fprintf(dotout, "start%d -> ", tr->id);
			fprintf(dotdefs, "start%d [label=\"%s\"];\n", tr->id, tr->invoke);
			for(auto e : tr->event_seq) {
				fprintf(dotout, "%c%d -> ", e->entry ? 'e' : 'x', e->sc->uuid);
				fprintf(dotdefs, "%c%d [label=\"%d:%s:%s\"];\n",
						e->entry ? 'e' : 'x', e->sc->uuid, tr->id, e->entry ? "entry" : "exit", syscall_names[e->sc->number]);
				printf(" %s <%s> ",e->entry ? "entry" : "exit ", syscall_names[e->sc->number]);
				auto so = dynamic_cast<class sockop*>(e->sc);
				auto scaccept = dynamic_cast<class Sysaccept*>(e->sc);
				if(scaccept && e->entry /* only do this once */) {
					class sock *c = scaccept->get_socket();
					class sock *s = scaccept->serversock;
					if(c && s) {
						printf(":: %s -> %s\n", sock_name(s).c_str(),
								sock_name(c).c_str());
						for(auto sys : syscall_list) {
							auto scconn = dynamic_cast<class Sysconnect *>(sys);
							if(scconn != nullptr) {
								/* TODO: we should look for and track connections
								 * as they happen */
								struct sock *cl_sock = scconn->get_socket();
								if(cl_sock && ISASSOC(cl_sock) && ISASSOC(s) && ISASSOC(c)) {
									cl_sock->conn_pair = c;
									c->conn_pair = cl_sock;
									struct sockaddr_in *in_server_lis
										= (struct sockaddr_in *)&s->addr;
									struct sockaddr_in *in_server_cli
										= (struct sockaddr_in *)&c->addr;
									struct sockaddr_in *in_client_soc
										= (struct sockaddr_in *)&cl_sock->addr;

									if(in_client_soc->sin_port == in_server_lis->sin_port
											&& !memcmp(&in_client_soc->sin_addr,
												&in_server_cli->sin_addr,
												sizeof(in_server_cli->sin_addr))) {
										scaccept->pair = scconn;
										scconn->pair = scaccept;
										fprintf(dotdefs, "e%d -> x%d\n", scconn->uuid, scaccept->uuid);
										fprintf(dotdefs, "e%d -> x%d\n", scaccept->uuid, scconn->uuid);
									}
								}
							}
						}
					}
				} else if (so != nullptr) {
					class sock *s = so->get_socket();
					if(s) {
						printf(":: %s\n", sock_name(s).c_str());
					}
				}
				printf("\n");
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

