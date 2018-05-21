#include <cstdio>
#include "run.h"

void run_serialize(struct run *run, FILE *f)
{
	for(auto p : run->proc_list) {
		fprintf(f, "PROCESS \n");
	}

	for(auto t : run->thread_list) {
		fprintf(f, "THREAD \n");

	}

	for(auto s : run->syscall_list) {
		fprintf(f, "SYSCALL \n");

	}

	for(auto s : run->sock_list) {
		fprintf(f, "SOCKET \n");

	}

	for(auto c : run->connection_list) {
		fprintf(f, "CONNECTION \n");

	}
}

