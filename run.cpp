#include "run.h"
#include <cstdio>
#include <cstring>
template<typename T>
static T *growcreate(std::vector<T *> *v, size_t idx)
{
	if(idx >= v->size())
		v->resize(idx + 1);
	if((*v)[idx] == NULL)
		(*v)[idx] = new T();
	return (*v)[idx];
}

void event::serialize(FILE *f)
{
	fprintf(f, "EVENT %d %d %d %d %ld\n", uuid, trid, sc->uuid, entry, extra_parents.size());
}

void sock::serialize(FILE *f)
{
	fprintf(f, "SOCK %ld %d %d %d %d\n", uuid, flags, sockfd, proc->id, fromthread->id);
	fprintf(f, "  addr ");
	serialize_sockaddr(f, &addr, addrlen);
	fprintf(f, "\n  peer ");
	serialize_sockaddr(f, &peer, peerlen);
	fprintf(f, "\n");
}

void connection::serialize(FILE *f)
{
	fprintf(f,
	  "CONN %d %ld %ld %d %d\n",
	  uuid,
	  connside->uuid,
	  accside ? accside->uuid : -1,
	  conn->uuid,
	  acc ? acc->uuid : -1);
}

void serialize_thread(struct thread_tr *t, FILE *f)
{
	fprintf(f, "THREAD %d %d %d %ld\n", t->id, t->tid, t->proc->id, t->event_seq.size());
}

void serialize_proc(struct proc_tr *p, FILE *f)
{
	fprintf(f,
	  "PROCESS %d %d %d %d %ld %s\n",
	  p->id,
	  p->pid,
	  p->ecode,
	  p->exited,
	  p->proc_thread_list.size(),
	  p->invoke);
}

void sockop::run_load(class run *run, FILE *f)
{
	int sid;
	fscanf(f, "  sockop %d\n", &sid);
	if(sid >= 0)
		sock = growcreate(&run->sock_list, sid);
	else
		sock = NULL;
}

void Sysaccept::run_load(class run *run, FILE *f)
{
	sockop::run_load(run, f);
	int ssockid;
	fscanf(f, "  serversock %d\n", &ssockid);
	if(ssockid >= 0)
		serversock = growcreate(&run->sock_list, ssockid);
	else
		serversock = NULL;
}

void Sysconnect::run_load(class run *run, FILE *f)
{
	sockop::run_load(run, f);
}

void Sysaccept::serialize(FILE *f)
{
	Syscall::serialize(f);
	sockop::serialize(f);
	SPACE(f, 2);
	fprintf(f, "serversock %ld\n", serversock ? serversock->uuid : -1);
}

void Sysconnect::serialize(FILE *f)
{
	Syscall::serialize(f);
	sockop::serialize(f);
}

void run_serialize(class run *run, FILE *f)
{
	for(auto s : run->syscall_list) {
		s->serialize(f);
	}

	for(auto p : run->proc_list) {
		serialize_proc(p, f);
		for(auto t : p->proc_thread_list) {
			SPACE(f, 2);
			fprintf(f, "thread %d\n", t->id);
		}
	}

	for(auto t : run->thread_list) {
		serialize_thread(t, f);
		for(auto e : t->event_seq) {
			SPACE(f, 2);
			e->serialize(f);
			for(auto p : e->extra_parents) {
				SPACE(f, 4);
				fprintf(f, "event %d %d\n", p->uuid, p->trid);
			}
		}
	}

	for(auto s : run->sock_list) {
		s->serialize(f);
	}

	for(auto c : run->connection_list) {
		c->serialize(f);
	}
}

static inline bool startswith(const char *str, const char *sw)
{
	return strncmp(str, sw, strlen(sw)) == 0;
}

extern Syscall *(*syscallmap_inactive[1024])();
void run_load(class run *run, FILE *f)
{
	char *line = NULL;
	size_t ls = 0;
	while(getline(&line, &ls, f) > 0) {
		if(startswith(line, "PROCESS")) {
			int id, pid, ecode, exited;
			int tmp;
			size_t ptls;
			sscanf(line, "PROCESS %d %d %d %d %ld %n", &id, &pid, &ecode, &exited, &ptls, &tmp);
			char *invoke = line + tmp;
			// printf("-> %d %d %d %d %d %s\n", id, pid, ecode, exited, tmp, invoke);
			struct proc_tr *np = growcreate(&run->proc_list, id);
			np->id = id;
			np->pid = pid;
			np->ecode = ecode;
			np->exited = exited;
			np->invoke = strdup(invoke);
			for(size_t i = 0; i < ptls && getline(&line, &ls, f); i++) {
				int th_id;
				sscanf(line, "  thread %d", &th_id);
				struct thread_tr *th = growcreate(&run->thread_list, th_id);
				np->proc_thread_list.push_back(th);
			}
		} else if(startswith(line, "THREAD")) {
			int id, tid, pr_id;
			size_t ess;
			sscanf(line, "THREAD %d %d %d %ld", &id, &tid, &pr_id, &ess);
			struct thread_tr *th = growcreate(&run->thread_list, id);
			th->id = id;
			th->tid = tid;
			th->proc = growcreate(&run->proc_list, pr_id);
			for(size_t i = 0; i < ess && getline(&line, &ls, f); i++) {
				size_t pcount;
				int sc, ent, uuid, trid;
				/* TODO: proc->event_seq */
				sscanf(line, "  EVENT %d %d %d %d %ld", &uuid, &trid, &sc, &ent, &pcount);
				event *e = growcreate(&th->event_seq, uuid);
				e->entry = ent;
				e->sc = run->syscall_list[sc];
				e->trid = trid;
				e->uuid = uuid;
				if(ent)
					e->sc->entry_event = e;
				else
					e->sc->exit_event = e;
				e->sc->thread = th;
				for(size_t j = 0; j < pcount && getline(&line, &ls, f); j++) {
					int patr, paid;
					sscanf(line, "    event %d %d", &paid, &patr);
					struct thread_tr *parent_th = growcreate(&run->thread_list, patr);
					event *pe = growcreate(&parent_th->event_seq, paid);
					e->extra_parents.push_back(pe);
				}
			}
		} else if(startswith(line, "SYSCALL")) {
			int uuid, fromtid, frompid, rv, succ;
			long number, p0, p1, p2, p3, p4, p5;
			char lid[32];
			sscanf(line,
			  "SYSCALL %d %d %d %s %ld %ld %ld %ld %ld %ld %ld %d %d",
			  &uuid,
			  &fromtid,
			  &frompid,
			  lid,
			  &number,
			  &p0,
			  &p1,
			  &p2,
			  &p3,
			  &p4,
			  &p5,
			  &rv,
			  &succ);
			Syscall *sc = syscallmap_inactive[number]();
			sc->uuid = uuid;
			sc->fromtid = fromtid;
			sc->frompid = frompid;
			sc->retval = rv;
			sc->ret_success = succ;
			sc->number = number;
			sc->params[0] = p0;
			sc->params[1] = p1;
			sc->params[2] = p2;
			sc->params[3] = p3;
			sc->params[4] = p4;
			sc->params[5] = p5;
			sc->localid = std::string(lid);
			sc->run_load(run, f);
			run->syscall_list.push_back(sc);
		} else if(startswith(line, "SOCK")) {
			int uuid, flags, sockfd, procid, thid;
			sscanf(line, "SOCK %d %d %d %d %d", &uuid, &flags, &sockfd, &procid, &thid);
			sock *s = growcreate(&run->sock_list, uuid);
			s->uuid = uuid;
			s->flags = flags;
			s->sockfd = sockfd;
			s->proc = growcreate(&run->proc_list, procid);
			s->fromthread = growcreate(&run->thread_list, thid);
			int sz, port;
			char addr[64];
			struct sockaddr_in saaddr, sapeer;
			getline(&line, &ls, f);
			sscanf(line, "  addr sockaddr_in (%d)%d:%[0-9.]s\n", &sz, &port, addr);
			s->addrlen = sz;
			inet_pton(AF_INET, addr, &saaddr.sin_addr);
			saaddr.sin_port = port;
			s->addr = *(struct sockaddr *)&saaddr;
			s->addr.sa_family = AF_INET;

			getline(&line, &ls, f);
			sscanf(line, "  peer sockaddr_in (%d)%d:%[0-9.]s\n", &sz, &port, addr);
			s->peerlen = sz;
			inet_pton(AF_INET, addr, &sapeer.sin_addr);
			sapeer.sin_port = port;
			s->peer = *(struct sockaddr *)&sapeer;
			s->peer.sa_family = AF_INET;

		} else if(startswith(line, "CONN")) {
			int uuid, sysconid, sysaccid;
			long aid, cid;
			sscanf(line, "CONN %d %ld %ld %d %d", &uuid, &cid, &aid, &sysconid, &sysaccid);
			connection *c = growcreate(&run->connection_list, uuid);
			Sysconnect *syscon = (Sysconnect *)run->syscall_list[sysconid];
			Sysaccept *sysacc = (Sysaccept *)(sysaccid == -1 ? NULL : run->syscall_list[sysaccid]);
			sock *cside = growcreate(&run->sock_list, cid);
			sock *aside = aid == -1 ? NULL : growcreate(&run->sock_list, aid);
			c->connside = cside;
			c->accside = aside;
			c->conn = syscon;
			c->acc = sysacc;
		} else if(startswith(line, "FAULT")) {
			int th_id, ev_id, err_code;
			sscanf(line, "FAULT %d %d %d", &th_id, &ev_id, &err_code);
			struct thread_tr *th = run->thread_list[th_id];
			event *ev = th->event_seq[ev_id];
			ev->err_code = err_code;
		} else {
			fprintf(stderr, "Cannot parse line: %s", line);
			exit(1);
		}
	}

	/* phase 2: construct unordered maps? */
}
