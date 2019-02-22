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
	return 0;
}

bool followrun_step(struct thread_tr *tracee)
{
	if(followruns.size() == 0) {
		/* no more graphs! */
		fprintf(stderr, "=== FELL OFF ALL GRAPHS ===\n");
		// getchar();
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
		if((size_t)tracee->id >= run->thread_list.size()) {
			run->fell_off = true;
			followrun_del(run);
			return followrun_step(tracee);
		}
		struct thread_tr *followed_thread = run->thread_list[tracee->id];
		if((size_t)last_event_idx >= followed_thread->event_seq.size()) {
			run->fell_off = true;
			followrun_del(run);
			return followrun_step(tracee);
		}
		event *other_event = followed_thread->event_seq[last_event_idx];

		if(other_event->entry != last_event->entry || other_event->uuid != last_event->uuid
		   || other_event->sc->number != last_event->sc->number
		   || !other_event->sc->approx_eq(last_event->sc, !last_event->entry ? 0 : 0)) {
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
				fprintf(stderr,
				  "  :: event->sc approx? %d\n",
				  other_event->sc->approx_eq(last_event->sc, !last_event->entry ? SC_EQ_RET : 0));
			}
			run->fell_off = true;
			followrun_del(run);
			return followrun_step(tracee);
		}
		/* this is still a valid graph */
		if(other_event->fault_event) {
			fprintf(stderr, ":: %d %d\n", other_event->uuid, last_event_idx);
			/* inject a fault */
			last_event->sc->fault();
			// last_event->err_code = other_event->err_code;
			// last_event->sc->ret_err = other_event->err_code;
			// last_event->sc->set_return_value(other_event->err_code);
		}
	}
	return false;
}
