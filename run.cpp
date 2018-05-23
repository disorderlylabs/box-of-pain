#include <cstdio>
#include "run.h"

void event::serialize(FILE *f)
{
	fprintf(f, "EVENT %d %d\n", sc->uuid, entry);
}

void sock::serialize(FILE *f)
{
	/* TODO: name, addr, etc */
	fprintf(f, "SOCK %ld %d %d %d %d\n",
			uuid, flags, sockfd, proc->pid, fromthread->tid);
	fprintf(f, "  addr "); serialize_sockaddr(f, &addr, addrlen);
	fprintf(f, "\n  peer "); serialize_sockaddr(f, &peer, peerlen);
	fprintf(f, "\n");
}

void connection::serialize(FILE *f)
{
	fprintf(f, "CONN %ld %ld %d %d\n",
			connside->uuid, accside ? accside->uuid : -1, conn->uuid, acc ? acc->uuid : -1);
}

void serialize_thread(struct thread_tr *t, FILE *f)
{
	fprintf(f, "THREAD %d %d %d\n",
			t->id, t->tid, t->proc->pid);
}

void serialize_proc(struct proc_tr *p, FILE *f)
{
	fprintf(f, "PROCESS %d %d %d %d %s\n",
			p->id, p->pid, p->ecode, p->exited, p->invoke);
}

void Sysaccept::serialize(FILE *f)
{
	Syscall::serialize(f);
	sockop::serialize(f);
	SPACE(f, 2);
	fprintf(f, "pair %d\n", pair ? pair->uuid : -1);
	SPACE(f, 2);
	fprintf(f, "serversock %ld\n", serversock ? serversock->uuid : -1);
}
void Sysconnect::serialize(FILE *f) {
	Syscall::serialize(f);
	SPACE(f, 2);
	fprintf(f, "pair %d\n", pair ? pair->uuid : -1);
}



void run_serialize(struct run *run, FILE *f)
{
	for(auto p : run->proc_list) {
		serialize_proc(p, f);
		for(auto t : p->proc_thread_list) {
			SPACE(f, 2);
			serialize_thread(t, f);
		}
	}

	for(auto t : run->thread_list) {
		serialize_thread(t, f);
		for(auto e : t->event_seq) {
			SPACE(f, 2);
			e->serialize(f);
			for(auto p : e->extra_parents) {
				SPACE(f, 4);
				p->serialize(f);
			}
		}
	}

	for(auto s : run->syscall_list) {
		s->serialize(f);
	}

	for(auto s : run->sock_list) {
		s->serialize(f);
	}

	for(auto c : run->connection_list) {
		c->serialize(f);
	}
}

