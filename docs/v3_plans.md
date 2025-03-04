# V3 Plans

> Okay, so basically the switching system is unreliable in this V2. After about 1 million job execution cycles, it just breaks down and stops running.

> I think this time around the whole system needs to be built up in theory, and then code written out in the exact theory.


## The theory

The way that this job system works will be the same as before.

All cores are consuming jobs, then when a section of jobs are finished, then new jobs are fetched which then are appended onto the queue.

I think that before, where there was a whole system of organizing job orders and stuff like that, I think that's way too much metadata/overhead.
Perhaps the best way to approach all of this is simply having more callbacks to insert in the jobs, and maybe a fast pattern can get established.
Something like the example below (a job system that has a few jobs that must be run one after the other, and then has a time floor):

```cpp

std::vector<Job*> queue_up_jobs_callback()
{
	// @NOTE: This needs to be accessed beginning to end by only one thread at a time.
	//        Probably doing this with atomics is best, but I want to just make the boilerplate
	//        as little as possible.
	// @NOTE: Gaaaaahhhh it's gonna need to have a callback name to show it's only run one at a time.
	//        Mmmmmm ig just putting "thread_safe" should work? But specifying that it is run on random
	//        threads depending on the iteration. Mmmmm maybe using the word "critical_func" could work?
	//        It shows that atomics are needed, but it's gonna be just fine if you decide to do state machine
	//        stuff.
	static std::atomic_uint8_t state_idx{ 0 };
	static std::atomic_uint64_t start_time{ os_get_time() };
	constexpr uint64_t time_interval{ 1000 };
	std::vector<Job*> jobs;

	uint8_t state_idx_copy{ state_idx };
	switch (state_idx_copy)
	{
	// Copy job list for state.
	case 0:
	case 1:
	case 2:
	{
		static const std::vector<std::vector<Job*>> job_lists{
			{
				...,
				...,
				...,
			},
			{
				...,
				...,
				...,
				...,
			},
			{
				...,
				...,
			},
		};
		jobs = job_lists[state_idx_copy];

		// Increment state.
		state_idx++;

		break;
	}

	// Check to see if ready for restarting.
	case 3:
		if (os_get_time() >= start_time + time_interval)
		{
			start_time = os_get_time();
			state_idx = 0;
		}
		break;

	// Error checking.
	default:
		assert(false);
		break;
	}

	return jobs;
}

```


Okay, so only these critical callbacks are able to add functions into the job queue as they're solicited.
No more being able to add in jobs within jobs. I think the callbacks should be the sole managers of the jobs.

So bc of that, no more need of having a pending queue.


## Job Sources.

There will likely be 3 to 4 different ones of these. 

Example:
```cpp

static std::array<JobSource, 3> s_job_sources{
	{ "Simulation Source",    simulation_source_queue_callback          },
	{ "Rendering Source",     rendering_source_queue_callback           },
	{ "Resource Mgmt Source", resource_management_source_queue_callback },
};

```

They are simply callbacks that can be called to query more jobs to insert into the job queue.

In order to detect when inserting jobs is needed, I guess there needs to be a check for callbacks made before any
job reservations.
I guess that makes the nagare:
```cpp
// @NOTE: Upon startup, `m_source_checking_idx` is calculated. It should simply
//        be the same as `thread_idx`, however, this is not the case if there are
//        fewer threads than job sources. If a job runner is not responsible for checking
//        in on a job source, then `m_source_checking_idx` is set to -1.
if (m_source_checking_idx != (size_t)-1)
{
	// Check in job source for all jobs finished.
	size_t zero{ 0 };
	if (s_job_sources[m_source_checking_idx].num_unfinished_jobs.compare_exchange_weak(zero, (size_t)-1))
	{
		auto new_jobs{
			s_job_sources[m_source_checking_idx].critical_callback_func()
		};
		size_t num_new_jobs{ new_jobs.size() };
		append_jobs_onto_job_queue(new_jobs);
		s_job_sources[m_source_checking_idx].num_unfinished_jobs = num_new_jobs;
		m_num_unreserved_jobs += num_new_jobs;
	}
}
```

Only one thread is in charge of each job source. If there are more threads than job sources, then the remaining
threads will not have a job source to check. If there are more job sources than threads, then threads may have
more than one job source to check.

@THOUIGHT: Actually, if there are more threads than job sources, it might be good to have redundancy. Try it
if there are issues with jobs not getting loaded in in time.

@NOTE: In the future, I think having this check be done at the end of marking a job as complete would be really
good too, however, that could cause more complication and code duplication. At least for this iteration there
should only be a pure pull scheme going on.


## Job Runners.

There will be one per thread on the cpu, and the primary function will always be for running jobs.

The steps a job runner will run thru are:

1. Check if all jobs are finished in assigned job source(s) (if any)
    - Query job source for new jobs and append them to the job queue if so.
1. Reserve a job.
	1. If no more jobs left to reserve, then exit unsuccessfully.
	1. Decrement the num jobs left to reserve in a compare-exchange.
	1. "Take a number" type of counter for knowing which spot in queue to access.
	1. Wait for any memory barriers.
	1. Read the job queue at the designated number spot for job pointer.
	1. Return pointer.
    - @NOTE: if reservation unsuccessful, loop back to step 1\*.
1. Execute reserved job.
1. Decrement unfinished job count for corresponding job source.
1. Loop back to step 1\*.

_\* Unless application is fully shut down_


## Job reservation.

So the main concern is getting the job queue all sync'd up with all the threads. I thjink that there could be memory barriers,
or there could be just creating an atomic array. Idk if making an array of atomic thingoes would work. I kinda feel like it should though.
The issue would be reserving a range for writing new jobs while copying in the list of jobs.

I think doing this would be good for appending jobs:

1. Solicit/get list of jobs.
1. Exit if number of jobs to add is 0.
1. Add `num_jobs` to the job reservation index.
1. Copy in all the jobs into the atomic ring buffer.
1. Add `num_jobs` to the job consuming end index.

> @NOTE: this should probably be its own structure and process (i.e. have steps 2 and beyond be its own function `append_jobs()`).
I think that the appending structure may be able to be atomic, but if not, it would be really easy to switch out the apppend jobs function.
Ahhhhh actually it would just be fine to be its own member function. That would allow for there to be easy enough editing bc I don't think
we need an abstracted interface for this.


## Job abstraction.

> @TODO FIGURE THIS OUT!!!!


## Wishlist

- No mutex uses. Use fences instead.
- Use atomics where possible
- No deadlocking this time.


## Interfaces

```cpp

// Job Queue Interface.
Job* pop_front_job__thread_safe();  // returns `nullptr` if no jobs to pop.
bool append_jobs_back__thread_safe(std::vector<Job*> jobs);  // returns `false` if underlying structure is unable to accommodate the jobs.

// Job Source Interface.
std::vector<Job*> fetch_next_job_batch_if_all_jobs_complete__thread_safe_weak();  // Contains the check, "try lock" (just a compare-exchange), and the fetch of the jobs. Just run this at every tick for each thread in charge of the jobs thread.
void notify_one_job_complete__thread_safe();  // Essentially just counts down by 1 atomically.

// The way I'm thinking of using the interfaces.
for (auto& my_job_source : my_job_sources)
{
	auto new_jobs{ my_job_source.fetch_next_job_batch_if_all_jobs_complete__thread_safe_weak() };
	if (!new_jobs.empty())
		if (!append_jobs_back__thread_safe(new_jobs))
		{
			std::err << "Appending jobs to queue failed." << std::endl;
			assert(false);
		}
}
if (auto job{ pop_front_job__thread_safe() })
{
	job->execute();
	job->job_source->notify_one_job_complete__thread_safe();
}

```

> NOTE: After doing a jobs pull from a source and appending the jobs to the job queue, a memory fence is needed to ensure that the atomics all got written correctly.


## Debugging notes for concurrency.

Goal for a single job source:

1. `fetch_next_job_batch_if_all_jobs_complete__thread_safe_weak()`
1. Place returned jobs into the job queue
1. OKAYU UPDATE!!! I figured out the `front_idx` and `back_idx` were just getting off in the ring buffer. After fixing those issues it works great!!!


For the issues with multiple job sources:
1. Mmmmmm it loks like there's no issues with 


FOR NEW DESIGN (no sizing, but rather front and back indices run independently)
1. Just have a "reserve next read position in queue" to the core, so now pop front and push back are just completely separate.
1. When the core that has reserved a read position, then do a CAS to see if a job has been populated. If it is, then get it and write it back to nullptr.
    1. @NOTE: I thought that this CAS wasn't required, but especially with 36 cores running all this all at once it proved to be necessary. (Thea 2024/12/21)
1. Then after processing the job, decrement the job source count and then reserve the next read position.
1. ON THE OTHER HAND, for each thread assigned a job source tracking thing, keep doing the same thing. Just add more onto the queue as the time keeps moving on.


## Benchmarking with profiler (debug mode).

- `pop_front_job__thread_safe_weak()`: 145ns
- `append_jobs_back__thread_safe()`: 875ns (4 jobs, 1 job source)
- `notify_one_job_complete__thread_safe()`: 192ns
- `fetch_next_job_batch_if_all_jobs_complete__thread_safe_weak()`: 453ns
- TOTAL: 1665ns (0.001665ms) <- I think this is a really good metric for overhead.