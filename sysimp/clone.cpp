#include <sys.h>
#include <tracee.h>
#include <unordered_map>

#include "run.h"

void Sysclone::start()
{
}

void Sysclone::finish()
{
	fprintf(stderr, "clone: %lx %lx %lx %lx\n", params[0], params[1], params[2], params[3]);
}
