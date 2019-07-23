#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <errno.h>
#include <fstream>
#include <iostream>
#include <list>
#include <netinet/in.h>
#include <signal.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>

#include "run.h"

extern volatile bool keyboardinterrupt;

struct node {
	event *ev;
	std::list<node *> outgoing;
	std::list<node *> incoming;
	bool keep = false;
};

static size_t __next_nid = 0;
static inline size_t eid_to_nid(event *e)
{
	if(e->__nid == -1) {
		e->__nid = __next_nid++;
	}
	if(e->__nid == 205) {
		printf(":: %p\n", e);
	}
	return e->__nid;
}

void dump(const char *name, struct run *run)
{
	__next_nid = 0;
	size_t max_nid = 0;
	for(auto tr : run->thread_list) {
		for(auto e : tr->event_seq) {
			size_t nid = eid_to_nid(e);
			if(nid > max_nid)
				max_nid = nid;
		}
	}
	std::vector<node *> nodes;
	printf("reserving %ld\n", max_nid + 1);
	nodes.reserve(max_nid + 1);
	nodes.resize(max_nid + 1);
	for(size_t i = 0; i < max_nid + 1; i++) {
		nodes[i] = NULL;
	}
	// std::string m4 = std::string(name) + std::string(".m4");
	// std::string inc = std::string(name) + std::string(".inc");
	// FILE *dotout = fopen(m4.c_str(), "w");
	// FILE *dotdefs = fopen(inc.c_str(), "w");
	std::string outf = name;
	std::ofstream dot(outf + ".dot");
	dot << "digraph trace {\n";
	dot << "rankdir=TB\nsplines=line\noutputorder=nodesfirst\n";
	node start;
	node end;
	for(auto tr : run->thread_list) {
		node *last = NULL;
		for(auto e : tr->event_seq) {
			node *n;
			if(nodes[eid_to_nid(e)])
				n = nodes[eid_to_nid(e)];
			else
				n = nodes[eid_to_nid(e)] = new node();
			if(auto clone = dynamic_cast<Sysclone *>(e->sc)) {
				(void)clone;
				n->keep = true;
			}
			n->ev = e;
			for(auto pe : e->extra_parents) {
				node *pn;
				if(nodes[eid_to_nid(pe)])
					pn = nodes[eid_to_nid(pe)];
				else
					pn = nodes[eid_to_nid(pe)] = new node();
				pn->outgoing.push_back(n);
				n->incoming.push_back(pn);
			}

			if(last) {
				last->outgoing.push_front(n);
				n->incoming.push_front(last);
			}
			last = n;
		}
	}

	/* cleanup phase */
	size_t cleaned = 0;
	for(size_t i = 0; i < nodes.size(); i++) {
		auto n = nodes[i];
		if(!n)
			continue;
		fprintf(stderr,
		  "considering node: %ld (%ld %ld %d)\n",
		  eid_to_nid(n->ev),
		  n->outgoing.size(),
		  n->incoming.size(),
		  n->keep);
		if(n->outgoing.size() <= 1 && n->incoming.size() <= 1) {
			if(!n->keep) {
				if(n->incoming.size() > 0) {
					n->incoming.front()->outgoing.remove(n);
					if(n->outgoing.size() > 0)
						n->incoming.front()->outgoing.push_front(n->outgoing.front());
					else
						n->incoming.front()->outgoing.push_front(&end);
				}
				if(n->outgoing.size() > 0) {
					n->outgoing.front()->incoming.remove(n);
					if(n->incoming.size() > 0)
						n->outgoing.front()->incoming.push_front(n->incoming.front());
					else
						n->outgoing.front()->incoming.push_front(&start);
				}
				nodes[i] = NULL;
				cleaned++;
			}
		}
	}

	std::string output = "";
	std::string defs = "";
	std::string extras = "";
	for(auto tr : run->thread_list) {
		node *n = NULL;
		for(auto e : tr->event_seq) {
			/* find the first node we care about */
			if(nodes[eid_to_nid(e)]) {
				n = nodes[eid_to_nid(e)];
				break;
			}
		}
		if(!n)
			continue;

		output += "subgraph cluster_{\n";
		output += "start" + std::to_string(tr->tid) + " -> ";

		std::string inv = tr->proc->invoke;
		inv.erase(std::remove(inv.begin(), inv.end(), '\n'), inv.end());
		defs += "start" + std::to_string(tr->tid) + " [label=\"" + std::to_string(tr->id) + ":"
		        + std::to_string(tr->tid) + ":" + inv
		        + "\",style=\"filled\",fillcolor=\"#1111aa22\"];\n";

		for(; n; n = n->outgoing.front()) {
			if(n == &end)
				break;
			std::string nname = "node" + std::to_string(eid_to_nid(n->ev));
			bool is_faulted = run->fault_node_set.find(std::pair<int, int>(tr->id, n->ev->uuid))
			                  != run->fault_node_set.end();

			if(auto clone = dynamic_cast<Sysclone *>(n->ev->sc)) {
				if(n->ev->entry) {
					extras += nname + " -> start" + std::to_string(clone->retval) + ";\n";
				}
			}

			for(auto extra : n->outgoing) {
				if(extra != n->outgoing.front()) {
					/* all extra edges */
					std::string ename = "node" + std::to_string(eid_to_nid(extra->ev));
					extras += nname + " -> " + ename + ";\n";
				}
			}
			output += nname + " -> ";
			defs += nname + "[label=\"" + std::to_string(tr->id) + ":"
			        + (n->ev->entry ? "entry" : "exit") + ":"
			        + syscall_table[n->ev->sc->number].name + "\",group=\"G"
			        + std::to_string(tr->id) + "\",fillcolor=\""
			        + (is_faulted ? "#6600ff88" : "#00ff0011") + "\",style=\"filled\"];\n";
		}

		output += "exit" + std::to_string(tr->id) + " [constraint=\"true\"];\n};\n";
		defs += "exit" + std::to_string(tr->id) + " [label=\"Exit: "
		        + std::to_string(tr->proc->ecode) + "\",style=\"filled\",fillcolor=\""
		        + (tr->proc->ecode == 0 ? "#1111aa22" : "#ff111188") + "\"];\n";
	}

	dot << defs;
	dot << output;
	dot << extras;
	dot << "}";
	dot.close();
}

void dump_old(const char *name, struct run *run)
{
	/* output to a dotout and dotdefs file. The dotdefs is a file that you can write
	 * fully new entries to each iteration. The dotout file is a file that gets entries
	 * written to over the course of the loop. They form m4 file, where the dotdefs file
	 * gets included. To view, run `m4 out.m4 && dot -Tpdf -o out.pdf out.dot`
	 * Where out is the thread or proc
	 * */

	std::string m4 = std::string(name) + std::string(".m4");
	std::string inc = std::string(name) + std::string(".inc");
	FILE *dotout = fopen(m4.c_str(), "w");
	FILE *dotdefs = fopen(inc.c_str(), "w");
	fprintf(dotout, "digraph trace {\ninclude(`%s.inc')\n", name);
	fprintf(dotdefs, "rankdir=TB\nsplines=line\noutputorder=nodesfirst\n");
	for(auto tr : run->thread_list) {
		fprintf(dotout, "edge[weight=2, color=gray75, fillcolor=gray75];\n");

		fprintf(dotout, "start%d -> ", tr->tid);
		fprintf(dotdefs, "subgraph cluster_{ \n");
		fprintf(dotdefs,
		  "start%d [label=\"%s\",style=\"filled\",fillcolor=\"#1111aa22\"];\n",
		  tr->tid,
		  tr->proc->invoke);
		for(auto e : tr->event_seq) {
			std::string sockinfo = "";
			if(auto clone = dynamic_cast<Sysclone *>(e->sc)) {
				if(e->entry)
					fprintf(dotdefs,
					  "x%s -> start%d [constraint=\"true\"];\n",
					  e->sc->localid.c_str(),
					  (int)clone->retval);
			}
			if(auto sop = dynamic_cast<sockop *>(e->sc)) {
				if(sop->get_socket())
					sockinfo += "" + sock_name_short(sop->get_socket());
			}
			fprintf(dotout, "%c%s -> ", e->entry ? 'e' : 'x', e->sc->localid.c_str());
			for(auto p : e->extra_parents) {
				fprintf(dotdefs,
				  "%c%s -> %c%s [constraint=\"true\"];\n",
				  p->entry ? 'e' : 'x',
				  p->sc->localid.c_str(),
				  e->entry ? 'e' : 'x',
				  e->sc->localid.c_str());
			}

			bool is_faulted = run->fault_node_set.find(std::pair<int, int>(tr->id, e->uuid))
			                  != run->fault_node_set.end();
			if(e->entry) {
				fprintf(dotdefs,
				  "e%s "
				  "[label=\"%d:entry:%s:%s:%s\",group=\"G%d\",fillcolor=\"%s\",style="
				  "\"filled\"];"
				  "\n",
				  e->sc->localid.c_str(),
				  tr->tid,
				  syscall_table[e->sc->number].name,
				  sockinfo.c_str(),
				  "",
				  tr->tid,
				  is_faulted ? "#6600ff88" : "#00ff0011");

				fprintf(dotdefs,
				  "x%s "
				  "[label=\"%d:exit:%s:%s:%s\",group=\"G%d\",fillcolor=\"%s\",style=\"filled\"]"
				  ";\n",
				  e->sc->localid.c_str(),
				  tr->tid,
				  syscall_table[e->sc->number].name,
				  sockinfo.c_str(),
				  std::to_string((long)e->sc->retval).c_str(),
				  tr->tid,
				  is_faulted ? "#ff00ff88" : "#ff000011");
			}
		}
		fprintf(dotout, "exit%d;\n", tr->tid);
		/* TODO: this only applies to current run... */
		if(!keyboardinterrupt) {
			fprintf(dotdefs,
			  "exit%d [label=\"Exit code=%d\",style=\"filled\",fillcolor=\"%s\"];\n",
			  tr->tid,
			  tr->proc->ecode,
			  tr->proc->ecode == 0 ? "#1111aa22" : "#ff111188");
		} else {
			fprintf(dotdefs,
			  "exit%d [label=\"Interrupted\",style=\"filled\",fillcolor=\"%s\"];\n",
			  tr->tid,
			  "#1111aa22");
		}
		fprintf(dotdefs, "}\n");
	}
	fprintf(dotout, "\n}\n");
	fclose(dotdefs);
	fclose(dotout);
}
