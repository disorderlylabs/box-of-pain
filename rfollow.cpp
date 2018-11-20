#include "run.h"
#include <unordered_set>

static std::unordered_set<run *> followruns;
static std::unordered_set<run *> all_followruns;

void followrun_add(run *run)
{
	followruns.insert(run);
	all_followruns.insert(run);
}

void followrun_del(run *run)
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

bool followrun_stats()
{
	for(auto run : all_followruns) {
		fprintf(stderr, "%20s: %s\n", run->name, run->fell_off ? "fell off" : "followed");
	}
}

bool followrun_step(struct thread_tr *tracee)
{
	if(followruns.size() == 0) {
		/* no more graphs! */
		fprintf(stderr, "=== FELL OFF ALL GRAPHS ===\n");
		getchar();
		return true;
	}
	// fprintf(stderr, "STEP\n");
	if(tracee->syscall == NULL)
		return false;
	int last_event_idx = tracee->event_seq.size() - 1;
	event *last_event = tracee->event_seq[last_event_idx];
	// fprintf(stderr,
	//  "last_event # = %d ... %s %d\n",
	// last_event_idx,
	// syscall_names[last_event->sc->number],
	// last_event->entry);
	for(auto run : followruns) {
		struct thread_tr *followed_thread = run->thread_list[tracee->id];
		event *other_event = followed_thread->event_seq[last_event_idx];

		if(other_event->entry != last_event->entry || other_event->uuid != last_event->uuid
		   || other_event->sc->number != last_event->sc->number
		   || !other_event->sc->approx_eq(last_event->sc, !last_event->entry ? SC_EQ_RET : 0)) {
			fprintf(stderr, "!! %s\n", syscall_table[other_event->sc->number].name);
			getchar();
		}

		if(other_event->sc->number != last_event->sc->number
		   || other_event->uuid != last_event->uuid) {
			if(options.log_follow) {
				fprintf(stderr, "== Fell off %s ==\n", run->name);
				fprintf(stderr,
				  "  :: (%d)%s (%d)%s",
				  tracee->id,
				  tracee->proc->invoke,
				  followed_thread->id,
				  followed_thread->proc->invoke);
				fprintf(stderr,
				  "  :: event: got %ld (exp %ld) %d (exp %d)\n",
				  last_event->sc->number,
				  other_event->sc->number,
				  last_event->uuid,
				  other_event->uuid);
			}
			run->fell_off = true;
			followrun_del(run);
			return followrun_step(tracee);
		}
	}
	return false;
}
