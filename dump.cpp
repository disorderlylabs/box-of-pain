#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>

#include "run.h"

extern volatile bool keyboardinterrupt;

void dump(const char *name, struct run *run)
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

			if(e->entry) {
				fprintf(dotdefs,
				  "e%s "
				  "[label=\"%d:entry:%s:%s:%s\",group=\"G%d\",fillcolor=\"%s\",style=\"filled\"];"
				  "\n",
				  e->sc->localid.c_str(),
				  tr->tid,
				  syscall_table[e->sc->number].name,
				  sockinfo.c_str(),
				  "",
				  tr->tid,
				  "#00ff0011");

				fprintf(dotdefs,
				  "x%s "
				  "[label=\"%d:exit:%s:%s:%s\",group=\"G%d\",fillcolor=\"%s\",style=\"filled\"];\n",
				  e->sc->localid.c_str(),
				  tr->tid,
				  syscall_table[e->sc->number].name,
				  sockinfo.c_str(),
				  std::to_string((long)e->sc->retval).c_str(),
				  tr->tid,
				  "#ff000011");
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
