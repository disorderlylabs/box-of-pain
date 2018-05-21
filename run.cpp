#include <cstdio>
#include "run.h"

void serialize_run(struct run *run, FILE *f)
{
	for(auto p : run->proc_list) {

	}

	for(auto t : run->thread_list) {

	}

	for(auto s : run->syscall_list) {

	}

	for(auto s : run->sockets) {

	}

	for(auto c : run->connections) {

	}
}

