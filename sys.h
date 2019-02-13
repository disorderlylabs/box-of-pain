#pragma once

#include <sys/ptrace.h>
#include <sys/reg.h>

#include <vector>

#include "helper.h"
#include "run.h"
#include "sockets.h"
#include "tracee.h"

#define MAX_PARAMS 6 /* linux has 6 register parameters */
enum syscall_state {
	STATE_UNCALLED,
	STATE_CALLED,
	STATE_DONE,
};

/* x86_64-linux specific. parameters map (first param is RDI) */
static const int param_map[MAX_PARAMS] = { RDI, RSI, RDX, R10, R8, R9 };

struct x86_64_syscall {
	int nr_args;
	int flags;
	int _un;
	const char *name;
};
extern struct x86_64_syscall syscall_table[];

static int set_syscall_param(int tid, int reg, long value)
{
	return ptrace(PTRACE_POKEUSER, tid, sizeof(long) * reg, value);
}

#define SC_EQ_RET 1
#define SC_EQ_ADDR_EPH 2
#define SC_EQ_PEER_EPH 4

class Syscall;
/* an event is a syscall entry or exit. NOTE: we may extend this to
 * include signals, perhaps. Need to think about the semantics of this. */
class event
{
  public:
	Syscall *sc;
	bool entry, pending, fault_event;
	// int err_code;
	int trid, uuid;
	/* this vector lists the extra partial-order "parents" of this event */
	std::vector<event *> extra_parents;
	event(Syscall *s, bool e, int trid, int uuid)
	  : sc(s)
	  , entry(e)
	  , trid(trid)
	  , uuid(uuid)
	{
	}
	event()
	{
	}
	void serialize(FILE *f);
};

struct thread_tr;
class Syscall
{
  public:
	class event *entry_event, *exit_event;
	int fromtid;
	int frompid;
	int uuid;
	std::string localid;
	struct thread_tr *thread;
	unsigned long number;
	unsigned long params[MAX_PARAMS];
	unsigned long retval = 0;
	/* if this gets set, it will force the syscall implementation
	 * to return success to the process, regardless of what happens.
	 * This can be used to, for example, make a write look like it
	 * succeeded while making it fail (by setting fd to -1). */
	bool ret_success = false;
	enum syscall_state state;
	int ret_err = 0;

	virtual void serialize(FILE *f)
	{
		fprintf(f,
		  "SYSCALL %d %d %d %s %ld %ld %ld %ld %ld %ld %ld %ld %d\n",
		  uuid,
		  fromtid,
		  frompid,
		  localid.c_str(),
		  number,
		  params[0],
		  params[1],
		  params[2],
		  params[3],
		  params[4],
		  params[5],
		  retval,
		  ret_success);
	}

	virtual void run_load(class run *run, FILE *f)
	{
		(void)run;
		(void)f;
	};

	virtual bool approx_eq(Syscall *other, int flags)
	{
		fprintf(stderr, ":: %ld %ld\n", other->params[0], this->params[0]);
		fprintf(stderr, ":: %ld %ld\n", other->params[1], this->params[1]);
		fprintf(stderr, ":: %ld %ld\n", other->params[2], this->params[2]);
		int nr_args = syscall_table[this->number].nr_args;
		if(other->number != this->number)
			return false;
		for(int i = 0; i < nr_args; i++)
			if(other->params[i] != this->params[i])
				return false;
		return flags & SC_EQ_RET ? other->retval == this->retval : true;
	}

	virtual void fault(void)
	{
		fprintf(stderr, "--- tried to inject fault in a syscall without fault injection ---\n");
	}

	Syscall(int ftid, long num)
	{
		fromtid = ftid;
		state = STATE_CALLED;
		number = num;
		for(int i = 0; i < MAX_PARAMS && ftid; i++) {
			params[i] = ptrace(PTRACE_PEEKUSER, fromtid, sizeof(long) * param_map[i]);
		}
		thread = find_tracee(fromtid);
		frompid = thread->proc->pid;
	}

	Syscall()
	{
	}

	void set_return_value(int val)
	{
		set_syscall_param(fromtid, RAX, val);
	}

	virtual void finish()
	{
		if(ret_success) {
			set_syscall_param(fromtid, RAX, 0);
		}
	}
	virtual void start()
	{
	}

	virtual bool operator==(const Syscall &other) const
	{
		return number == other.number;
	}

	virtual bool post_process(int pass __unused)
	{
		return false;
	}
};

class sockop
{
  public:
	class sock *sock = NULL;

	virtual bool approx_eq(Syscall *_o, int flags __unused)
	{
		sockop *other = dynamic_cast<sockop *>(_o);
		if(sock != NULL && other->get_socket() != NULL)
			return sock->approx_eq(
			  other->get_socket(), flags & SC_EQ_ADDR_EPH, flags & SC_EQ_PEER_EPH);
		else
			return sock == other->get_socket();
	}

	class sock *get_socket()
	{
		return sock;
	}
	virtual void serialize(FILE *f)
	{
		SPACE(f, 2);
		fprintf(f, "sockop %ld\n", sock ? sock->uuid : -1);
		// sock->serialize(f);
	}
	virtual void run_load(struct run *run, FILE *f);
};

class Sysclone : public Syscall
{
  public:
	Sysclone(int p, long n)
	  : Syscall(p, n)
	{
	}
	Sysclone()
	  : Syscall()
	{
	}
	void start();
	void finish();
};

class Sysclose
  : public Syscall
  , public sockop
{
  public:
	Sysclose(int p, long n)
	  : Syscall(p, n)
	{
	}
	Sysclose()
	  : Syscall()
	{
	}
	void start()
	{
		int fd = params[0];
		sock = sock_lookup(&current_run, frompid, fd);
		sock_close(&current_run, frompid, fd);
	}
	void serialize(FILE *f)
	{
		Syscall::serialize(f);
		sockop::serialize(f);
	}

	void run_load(struct run *run, FILE *f)
	{
		sockop::run_load(run, f);
	}
	bool approx_eq(Syscall *o, int flags)
	{
		return sockop::approx_eq(o, flags)
		       && ((flags & SC_EQ_RET) ? o->retval == this->retval : true);
	}
};

class Sysbind
  : public Syscall
  , public sockop
{
  public:
	Sysbind(int p, long n)
	  : Syscall(p, n)
	{
	}
	Sysbind()
	  : Syscall()
	{
	}

	void start()
	{
		struct sockaddr addr;
		socklen_t len;
		int sockfd = params[0];
		GETOBJ(fromtid, params[1], &addr);
		len = params[2];
		sock = sock_assoc(&current_run, thread, sockfd);
		sock_set_addr(sock, &addr, len);
	};
	void serialize(FILE *f)
	{
		Syscall::serialize(f);
		sockop::serialize(f);
	}
	void run_load(struct run *run, FILE *f)
	{
		sockop::run_load(run, f);
	}
	bool approx_eq(Syscall *o, int flags)
	{
		return sockop::approx_eq(o, flags)
		       && ((flags & SC_EQ_RET) ? o->retval == this->retval : true);
	}
};

class Sysconnect
  : public Syscall
  , public sockop
{
  public:
	//	class Sysaccept *pair;
	Sysconnect(int p, long n)
	  : Syscall(p, n)
	{
	}
	Sysconnect()
	  : Syscall()
	{
	}
	void start();
	void finish();
	void serialize(FILE *);
	void run_load(struct run *run, FILE *f);
	bool approx_eq(Syscall *o, int flags)
	{
		if(flags & SC_EQ_RET) {
			return sockop::approx_eq(o, flags | SC_EQ_ADDR_EPH) && o->retval == this->retval;
		}
		return true;
	}
};

class Sysaccept
  : public Syscall
  , public sockop
{
  public:
	//	class Sysconnect *pair;
	struct sock *serversock = NULL;
	Sysaccept(int p, long n)
	  : Syscall(p, n)
	{
	}
	Sysaccept()
	  : Syscall()
	{
	}
	void start();
	void finish();
	void serialize(FILE *);
	void run_load(struct run *run, FILE *f);
	bool approx_eq(Syscall *o, int flags)
	{
		Sysaccept *other = dynamic_cast<Sysaccept *>(o);
		if(other->serversock && serversock) {
			if(!serversock->approx_eq(other->serversock))
				return false;
		}
		if(flags & SC_EQ_RET) {
			/* TODO: this may be too strict. What about other ordering of fd returns? we should
			 * allow for return values from functions like these to vary a bit. */
			return sockop::approx_eq(o, flags | SC_EQ_PEER_EPH) && o->retval == this->retval;
		}
		return true;
	}
};

class Syswrite
  : public Syscall
  , public sockop
{
  public:
	Syswrite(int p, long n)
	  : Syscall(p, n)
	{
	}
	Syswrite()
	  : Syscall()
	{
	}
	void finish();
	void start();
	void serialize(FILE *f)
	{
		Syscall::serialize(f);
		sockop::serialize(f);
	}
	void run_load(struct run *run, FILE *f)
	{
		sockop::run_load(run, f);
	}
	bool approx_eq(Syscall *o, int flags)
	{
		return sockop::approx_eq(o, flags)
		       && ((flags & SC_EQ_RET) ? o->retval == this->retval : true);
	}
};

class Sysread
  : public sockop
  , public Syscall
{
  public:
	Sysread(int fpid, long n)
	  : Syscall(fpid, n)
	{
	}
	Sysread()
	  : Syscall()
	{
	}
	void finish();
	void start();
	void serialize(FILE *f)
	{
		Syscall::serialize(f);
		sockop::serialize(f);
	}
	void run_load(struct run *run, FILE *f)
	{
		sockop::run_load(run, f);
	}
	bool approx_eq(Syscall *o, int flags)
	{
		return sockop::approx_eq(o, flags)
		       && ((flags & SC_EQ_RET) ? o->retval == this->retval : true);
	}
};

class Sysrecvfrom
  : public Syscall
  , public sockop
{
  public:
	Sysrecvfrom(int fpid, long n)
	  : Syscall(fpid, n)
	{
	}
	Sysrecvfrom()
	  : Syscall()
	{
	}

	void start();
	void finish();
	void serialize(FILE *f)
	{
		Syscall::serialize(f);
		sockop::serialize(f);
	}
	void run_load(struct run *run, FILE *f)
	{
		sockop::run_load(run, f);
	}
	bool approx_eq(Syscall *o, int flags)
	{
		return sockop::approx_eq(o, flags)
		       && ((flags & SC_EQ_RET) ? o->retval == this->retval : true);
	}
};

class Syssendto
  : public Syscall
  , public sockop
{
  public:
	Syssendto(int fpid, long n)
	  : Syscall(fpid, n)
	{
	}
	Syssendto()
	  : Syscall()
	{
	}

	void start();
	void finish();

	void serialize(FILE *f)
	{
		Syscall::serialize(f);
		sockop::serialize(f);
	}
	void run_load(struct run *run, FILE *f)
	{
		sockop::run_load(run, f);
	}
	bool approx_eq(Syscall *o, int flags)
	{
		return sockop::approx_eq(o, flags)
		       && ((flags & SC_EQ_RET) ? o->retval == this->retval : true);
	}
};
