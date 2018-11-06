#include "run.h"
#include "scnames.h"
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

void followrun_dumpall()
{
	for(auto r : followruns) {
		dump(r->name, r);
	}
	dump(current_run.name, &current_run);
}

bool followrun_step(struct thread_tr *tracee)
{
	if(followruns.size() == 0) {
		/* no more graphs! */
		fprintf(stderr, "=== FELL OFF ALL GRAPHS ===\n");
		return true;
	}
	// fprintf(stderr, "STEP\n");
	if(tracee->syscall == NULL)
		return false;
	int last_event_idx = tracee->event_seq.size() - 1;
	event *last_event = tracee->event_seq[last_event_idx];
	fprintf(stderr,
	  "last_event # = %d ... %s %d\n",
	  last_event_idx,
	  syscall_names[last_event->sc->number],
	  last_event->entry);
	for(auto run : followruns) {
		struct thread_tr *rthread = run->thread_list[tracee->id];
		event *rte = rthread->event_seq[last_event_idx];
		fprintf(stderr, " :: %s %s\n", tracee->proc->invoke, rthread->proc->invoke);
		fprintf(stderr,
		  "  :: event: got %ld (exp %ld) %d (exp %d)\n",
		  last_event->sc->number,
		  rte->sc->number,
		  last_event->uuid,
		  rte->uuid);
		if(rte->sc->number != last_event->sc->number || rte->uuid != last_event->uuid) {
			fprintf(stderr, "\n\n******************** FELL OFF\n\n");
			followrun_del(run);
			return followrun_step(tracee);
		}
	}
	return false;
}
