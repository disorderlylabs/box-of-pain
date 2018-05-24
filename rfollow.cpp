
#include "run.h"
#include <unordered_set>

static std::unordered_set<run *> followruns;

void followrun_add(struct run *run)
{
	followruns.insert(run);
}

void followrun_del(struct run *run)
{
	followruns.erase(run);
}

bool followrun_step(struct thread_tr *tracee)
{

	return true;
}

