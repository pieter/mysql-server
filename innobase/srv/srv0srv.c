/******************************************************
The database server main program

NOTE: SQL Server 7 uses something which the documentation
calls user mode scheduled threads (UMS threads). One such
thread is usually allocated per processor. Win32
documentation does not know any UMS threads, which suggests
that the concept is internal to SQL Server 7. It may mean that
SQL Server 7 does all the scheduling of threads itself, even
in i/o waits. We should maybe modify InnoDB to use the same
technique, because thread switches within NT may be too slow.

SQL Server 7 also mentions fibers, which are cooperatively
scheduled threads. They can boost performance by 5 %,
according to the Delaney and Soukup's book.

Windows 2000 will have something called thread pooling
(see msdn website), which we could possibly use.

Another possibility could be to use some very fast user space
thread library. This might confuse NT though.

(c) 1995 Innobase Oy

Created 10/8/1995 Heikki Tuuri
*******************************************************/
/* Dummy comment */
#include "srv0srv.h"

#include "ut0mem.h"
#include "os0proc.h"
#include "mem0mem.h"
#include "mem0pool.h"
#include "sync0sync.h"
#include "sync0ipm.h"
#include "thr0loc.h"
#include "com0com.h"
#include "com0shm.h"
#include "que0que.h"
#include "srv0que.h"
#include "log0recv.h"
#include "odbc0odbc.h"
#include "pars0pars.h"
#include "usr0sess.h"
#include "lock0lock.h"
#include "trx0purge.h"
#include "ibuf0ibuf.h"
#include "buf0flu.h"
#include "btr0sea.h"
#include "dict0load.h"
#include "srv0start.h"
#include "row0mysql.h"

/* Buffer which can be used in printing fatal error messages */
char	srv_fatal_errbuf[5000];

/* The following counter is incremented whenever there is some user activity
in the server */
ulint	srv_activity_count	= 0;

ibool	srv_lock_timeout_and_monitor_active = FALSE;
ibool	srv_error_monitor_active = FALSE;

char*	srv_main_thread_op_info = "";

/* Server parameters which are read from the initfile */

/* The following three are dir paths which are catenated before file
names, where the file name itself may also contain a path */

char*	srv_data_home 	= NULL;
char*	srv_logs_home 	= NULL;
char*	srv_arch_dir 	= NULL;

ulint	srv_n_data_files = 0;
char**	srv_data_file_names = NULL;
ulint*	srv_data_file_sizes = NULL;	/* size in database pages */ 

ulint*  srv_data_file_is_raw_partition = NULL;

/* If the following is TRUE we do not allow inserts etc. This protects
the user from forgetting the 'newraw' keyword to my.cnf */

ibool	srv_created_new_raw	= FALSE;

char**	srv_log_group_home_dirs = NULL; 

ulint	srv_n_log_groups	= ULINT_MAX;
ulint	srv_n_log_files		= ULINT_MAX;
ulint	srv_log_file_size	= ULINT_MAX;	/* size in database pages */ 
ibool	srv_log_archive_on	= TRUE;
ulint	srv_log_buffer_size	= ULINT_MAX;	/* size in database pages */ 
ibool	srv_flush_log_at_trx_commit = TRUE;

byte	srv_latin1_ordering[256]	/* The sort order table of the latin1
					character set. The following table is
					the MySQL order as of Feb 10th, 2002 */
= {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07
, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17
, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27
, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F
, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37
, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F
, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47
, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F
, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57
, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F
, 0x60, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47
, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F
, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57
, 0x58, 0x59, 0x5A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F
, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87
, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F
, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97
, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F
, 0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7
, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF
, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7
, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF
, 0x41, 0x41, 0x41, 0x41, 0x5C, 0x5B, 0x5C, 0x43
, 0x45, 0x45, 0x45, 0x45, 0x49, 0x49, 0x49, 0x49
, 0x44, 0x4E, 0x4F, 0x4F, 0x4F, 0x4F, 0x5D, 0xD7
, 0xD8, 0x55, 0x55, 0x55, 0x59, 0x59, 0xDE, 0xDF
, 0x41, 0x41, 0x41, 0x41, 0x5C, 0x5B, 0x5C, 0x43
, 0x45, 0x45, 0x45, 0x45, 0x49, 0x49, 0x49, 0x49
, 0x44, 0x4E, 0x4F, 0x4F, 0x4F, 0x4F, 0x5D, 0xF7
, 0xD8, 0x55, 0x55, 0x55, 0x59, 0x59, 0xDE, 0xFF
};

ibool	srv_use_native_aio	= FALSE;
		
ulint	srv_pool_size		= ULINT_MAX;	/* size in database pages;
						MySQL originally sets this
						value in megabytes */ 
ulint	srv_mem_pool_size	= ULINT_MAX;	/* size in bytes */ 
ulint	srv_lock_table_size	= ULINT_MAX;

ulint	srv_n_file_io_threads	= ULINT_MAX;

ibool	srv_archive_recovery	= 0;
dulint	srv_archive_recovery_limit_lsn;

ulint	srv_lock_wait_timeout	= 1024 * 1024 * 1024;

char*   srv_unix_file_flush_method_str = NULL;
ulint   srv_unix_file_flush_method = 0;

/* If the following is != 0 we do not allow inserts etc. This protects
the user from forgetting the innodb_force_recovery keyword to my.cnf */

ulint	srv_force_recovery	= 0;
/*-----------------------*/
/* The following controls how many threads we let inside InnoDB concurrently:
threads waiting for locks are not counted into the number because otherwise
we could get a deadlock. MySQL creates a thread for each user session, and
semaphore contention and convoy problems can occur withput this restriction.
Value 10 should be good if there are less than 4 processors + 4 disks in the
computer. Bigger computers need bigger values. */

ulint	srv_thread_concurrency	= 8;

os_fast_mutex_t	srv_conc_mutex;		/* this mutex protects srv_conc data
					structures */
lint	srv_conc_n_threads	= 0;	/* number of OS threads currently
					inside InnoDB; it is not an error
					if this drops temporarily below zero
					because we do not demand that every
					thread increments this, but a thread
					waiting for a lock decrements this
					temporarily */
ulint	srv_conc_n_waiting_threads = 0;	/* number of OS threads waiting in the
					FIFO for a permission to enter InnoDB
					*/

typedef struct srv_conc_slot_struct	srv_conc_slot_t;
struct srv_conc_slot_struct{
	os_event_t			event;		/* event to wait */
	ibool				reserved;	/* TRUE if slot
							reserved */
	ibool				wait_ended;	/* TRUE when another
							thread has already set
							the event and the
							thread in this slot is
							free to proceed; but
							reserved may still be
							TRUE at that point */
	UT_LIST_NODE_T(srv_conc_slot_t)	srv_conc_queue;	/* queue node */
};

UT_LIST_BASE_NODE_T(srv_conc_slot_t)	srv_conc_queue;	/* queue of threads
							waiting to get in */
srv_conc_slot_t	srv_conc_slots[OS_THREAD_MAX_N];	/* array of wait
							slots */

/* Number of times a thread is allowed to enter InnoDB within the same
SQL query after it has once got the ticket at srv_conc_enter_innodb */
#define SRV_FREE_TICKETS_TO_ENTER	500

/*-----------------------*/
/* If the following is set TRUE then we do not run purge and insert buffer
merge to completion before shutdown */

ibool	srv_fast_shutdown	= FALSE;

ibool	srv_use_doublewrite_buf	= TRUE;

ibool   srv_set_thread_priorities = TRUE;
int     srv_query_thread_priority = 0;
/*-------------------------------------------*/
ulint	srv_n_spin_wait_rounds	= 20;
ulint	srv_spin_wait_delay	= 5;
ibool	srv_priority_boost	= TRUE;
char	srv_endpoint_name[COM_MAX_ADDR_LEN];
ulint	srv_n_com_threads	= ULINT_MAX;
ulint	srv_n_worker_threads	= ULINT_MAX;

ibool	srv_print_thread_releases	= FALSE;
ibool	srv_print_lock_waits		= FALSE;
ibool	srv_print_buf_io		= FALSE;
ibool	srv_print_log_io		= FALSE;
ibool	srv_print_latch_waits		= FALSE;

ulint	srv_n_rows_inserted		= 0;
ulint	srv_n_rows_updated		= 0;
ulint	srv_n_rows_deleted		= 0;
ulint	srv_n_rows_read			= 0;
ulint	srv_n_rows_inserted_old		= 0;
ulint	srv_n_rows_updated_old		= 0;
ulint	srv_n_rows_deleted_old		= 0;
ulint	srv_n_rows_read_old		= 0;

ibool	srv_print_innodb_monitor	= FALSE;
ibool   srv_print_innodb_lock_monitor   = FALSE;
ibool   srv_print_innodb_tablespace_monitor = FALSE;
ibool   srv_print_innodb_table_monitor = FALSE;

/* The parameters below are obsolete: */

ibool	srv_print_parsed_sql		= FALSE;

ulint	srv_sim_disk_wait_pct		= ULINT_MAX;
ulint	srv_sim_disk_wait_len		= ULINT_MAX;
ibool	srv_sim_disk_wait_by_yield	= FALSE;
ibool	srv_sim_disk_wait_by_wait	= FALSE;

ibool	srv_measure_contention	= FALSE;
ibool	srv_measure_by_spin	= FALSE;
	
ibool	srv_test_extra_mutexes	= FALSE;
ibool	srv_test_nocache	= FALSE;
ibool	srv_test_cache_evict	= FALSE;

ibool	srv_test_sync		= FALSE;
ulint	srv_test_n_threads	= ULINT_MAX;
ulint	srv_test_n_loops	= ULINT_MAX;
ulint	srv_test_n_free_rnds	= ULINT_MAX;
ulint	srv_test_n_reserved_rnds = ULINT_MAX;
ulint	srv_test_array_size	= ULINT_MAX;
ulint	srv_test_n_mutexes	= ULINT_MAX;

/* Array of English strings describing the current state of an
i/o handler thread */

char* srv_io_thread_op_info[SRV_MAX_N_IO_THREADS];

/*
	IMPLEMENTATION OF THE SERVER MAIN PROGRAM
	=========================================

There is the following analogue between this database
server and an operating system kernel:

DB concept			equivalent OS concept
----------			---------------------
transaction		--	process;

query thread		--	thread;

lock			--	semaphore;

transaction set to
the rollback state	--	kill signal delivered to a process;

kernel			--	kernel;

query thread execution:
(a) without kernel mutex
reserved	 	-- 	process executing in user mode;
(b) with kernel mutex reserved
			--	process executing in kernel mode;

The server is controlled by a master thread which runs at
a priority higher than normal, that is, higher than user threads.
It sleeps most of the time, and wakes up, say, every 300 milliseconds,
to check whether there is anything happening in the server which
requires intervention of the master thread. Such situations may be,
for example, when flushing of dirty blocks is needed in the buffer
pool or old version of database rows have to be cleaned away.

The threads which we call user threads serve the queries of
the clients and input from the console of the server.
They run at normal priority. The server may have several
communications endpoints. A dedicated set of user threads waits
at each of these endpoints ready to receive a client request.
Each request is taken by a single user thread, which then starts
processing and, when the result is ready, sends it to the client
and returns to wait at the same endpoint the thread started from.

So, we do not have dedicated communication threads listening at
the endpoints and dealing the jobs to dedicated worker threads.
Our architecture saves one thread swithch per request, compared
to the solution with dedicated communication threads
which amounts to 15 microseconds on 100 MHz Pentium
running NT. If the client
is communicating over a network, this saving is negligible, but
if the client resides in the same machine, maybe in an SMP machine
on a different processor from the server thread, the saving
can be important as the threads can communicate over shared
memory with an overhead of a few microseconds.

We may later implement a dedicated communication thread solution
for those endpoints which communicate over a network.

Our solution with user threads has two problems: for each endpoint
there has to be a number of listening threads. If there are many
communication endpoints, it may be difficult to set the right number
of concurrent threads in the system, as many of the threads
may always be waiting at less busy endpoints. Another problem
is queuing of the messages, as the server internally does not
offer any queue for jobs.

Another group of user threads is intended for splitting the
queries and processing them in parallel. Let us call these
parallel communication threads. These threads are waiting for
parallelized tasks, suspended on event semaphores.

A single user thread waits for input from the console,
like a command to shut the database.

Utility threads are a different group of threads which takes
care of the buffer pool flushing and other, mainly background
operations, in the server.
Some of these utility threads always run at a lower than normal
priority, so that they are always in background. Some of them
may dynamically boost their priority by the pri_adjust function,
even to higher than normal priority, if their task becomes urgent.
The running of utilities is controlled by high- and low-water marks
of urgency. The urgency may be measured by the number of dirty blocks
in the buffer pool, in the case of the flush thread, for example.
When the high-water mark is exceeded, an utility starts running, until
the urgency drops under the low-water mark. Then the utility thread
suspend itself to wait for an event. The master thread is
responsible of signaling this event when the utility thread is
again needed.

For each individual type of utility, some threads always remain
at lower than normal priority. This is because pri_adjust is implemented
so that the threads at normal or higher priority control their
share of running time by calling sleep. Thus, if the load of the
system sudenly drops, these threads cannot necessarily utilize
the system fully. The background priority threads make up for this,
starting to run when the load drops.

When there is no activity in the system, also the master thread
suspends itself to wait for an event making
the server totally silent. The responsibility to signal this
event is on the user thread which again receives a message
from a client.

There is still one complication in our server design. If a
background utility thread obtains a resource (e.g., mutex) needed by a user
thread, and there is also some other user activity in the system,
the user thread may have to wait indefinitely long for the
resource, as the OS does not schedule a background thread if
there is some other runnable user thread. This problem is called
priority inversion in real-time programming.

One solution to the priority inversion problem would be to
keep record of which thread owns which resource and
in the above case boost the priority of the background thread
so that it will be scheduled and it can release the resource.
This solution is called priority inheritance in real-time programming.
A drawback of this solution is that the overhead of acquiring a mutex 
increases slightly, maybe 0.2 microseconds on a 100 MHz Pentium, because
the thread has to call os_thread_get_curr_id.
This may be compared to 0.5 microsecond overhead for a mutex lock-unlock
pair. Note that the thread
cannot store the information in the resource, say mutex, itself,
because competing threads could wipe out the information if it is
stored before acquiring the mutex, and if it stored afterwards,
the information is outdated for the time of one machine instruction,
at least. (To be precise, the information could be stored to
lock_word in mutex if the machine supports atomic swap.)

The above solution with priority inheritance may become actual in the
future, but at the moment we plan to implement a more coarse solution,
which could be called a global priority inheritance. If a thread
has to wait for a long time, say 300 milliseconds, for a resource,
we just guess that it may be waiting for a resource owned by a background
thread, and boost the the priority of all runnable background threads
to the normal level. The background threads then themselves adjust
their fixed priority back to background after releasing all resources
they had (or, at some fixed points in their program code).

What is the performance of the global priority inheritance solution?
We may weigh the length of the wait time 300 milliseconds, during
which the system processes some other thread
to the cost of boosting the priority of each runnable background
thread, rescheduling it, and lowering the priority again.
On 100 MHz Pentium + NT this overhead may be of the order 100
microseconds per thread. So, if the number of runnable background
threads is not very big, say < 100, the cost is tolerable.
Utility threads probably will access resources used by
user threads not very often, so collisions of user threads
to preempted utility threads should not happen very often.

The thread table contains
information of the current status of each thread existing in the system,
and also the event semaphores used in suspending the master thread
and utility and parallel communication threads when they have nothing to do.
The thread table can be seen as an analogue to the process table
in a traditional Unix implementation.

The thread table is also used in the global priority inheritance
scheme. This brings in one additional complication: threads accessing
the thread table must have at least normal fixed priority,
because the priority inheritance solution does not work if a background
thread is preempted while possessing the mutex protecting the thread table.
So, if a thread accesses the thread table, its priority has to be
boosted at least to normal. This priority requirement can be seen similar to
the privileged mode used when processing the kernel calls in traditional
Unix.*/

/* Thread slot in the thread table */
struct srv_slot_struct{
	os_thread_id_t	id;		/* thread id */
	os_thread_t	handle;		/* thread handle */
	ulint		type;		/* thread type: user, utility etc. */
	ibool		in_use;		/* TRUE if this slot is in use */
	ibool		suspended;	/* TRUE if the thread is waiting
					for the event of this slot */
	ib_time_t	suspend_time;	/* time when the thread was
					suspended */
	os_event_t	event;		/* event used in suspending the
					thread when it has nothing to do */
	que_thr_t*	thr;		/* suspended query thread (only
					used for MySQL threads) */
};

/* Table for MySQL threads where they will be suspended to wait for locks */
srv_slot_t*	srv_mysql_table = NULL;

os_event_t	srv_lock_timeout_thread_event;

srv_sys_t*	srv_sys	= NULL;

byte		srv_pad1[64];	/* padding to prevent other memory update
				hotspots from residing on the same memory
				cache line */
mutex_t*	kernel_mutex_temp;/* mutex protecting the server, trx structs,
				query threads, and lock table */
byte		srv_pad2[64];	/* padding to prevent other memory update
				hotspots from residing on the same memory
				cache line */

/* The following three values measure the urgency of the jobs of
buffer, version, and insert threads. They may vary from 0 - 1000.
The server mutex protects all these variables. The low-water values
tell that the server can acquiesce the utility when the value
drops below this low-water mark. */

ulint	srv_meter[SRV_MASTER + 1];
ulint	srv_meter_low_water[SRV_MASTER + 1];
ulint	srv_meter_high_water[SRV_MASTER + 1];
ulint	srv_meter_high_water2[SRV_MASTER + 1];
ulint	srv_meter_foreground[SRV_MASTER + 1];

/* The following values give info about the activity going on in
the database. They are protected by the server mutex. The arrays
are indexed by the type of the thread. */

ulint	srv_n_threads_active[SRV_MASTER + 1];
ulint	srv_n_threads[SRV_MASTER + 1];


/*************************************************************************
Accessor function to get pointer to n'th slot in the server thread
table. */
static
srv_slot_t*
srv_table_get_nth_slot(
/*===================*/
				/* out: pointer to the slot */
	ulint	index)		/* in: index of the slot */
{
	ut_a(index < OS_THREAD_MAX_N);

	return(srv_sys->threads + index);
}

/*************************************************************************
Gets the number of threads in the system. */

ulint
srv_get_n_threads(void)
/*===================*/
{
	ulint	i;
	ulint	n_threads	= 0;

	mutex_enter(&kernel_mutex);

	for (i = SRV_COM; i < SRV_MASTER + 1; i++) {
	
		n_threads += srv_n_threads[i];
	}

	mutex_exit(&kernel_mutex);

	return(n_threads);
}

/*************************************************************************
Reserves a slot in the thread table for the current thread. Also creates the
thread local storage struct for the current thread. NOTE! The server mutex
has to be reserved by the caller! */
static
ulint
srv_table_reserve_slot(
/*===================*/
			/* out: reserved slot index */
	ulint	type)	/* in: type of the thread: one of SRV_COM, ... */
{
	srv_slot_t*	slot;
	ulint		i;
	
	ut_a(type > 0);
	ut_a(type <= SRV_MASTER);

	i = 0;
	slot = srv_table_get_nth_slot(i);

	while (slot->in_use) {
		i++;
		slot = srv_table_get_nth_slot(i);
	}

	ut_a(slot->in_use == FALSE);
	
	slot->in_use = TRUE;
	slot->suspended = FALSE;
	slot->id = os_thread_get_curr_id();
	slot->handle = os_thread_get_curr();
	slot->type = type;

	thr_local_create();

	thr_local_set_slot_no(os_thread_get_curr_id(), i);

	return(i);
}

/*************************************************************************
Suspends the calling thread to wait for the event in its thread slot.
NOTE! The server mutex has to be reserved by the caller! */
static
os_event_t
srv_suspend_thread(void)
/*====================*/
			/* out: event for the calling thread to wait */
{
	srv_slot_t*	slot;
	os_event_t	event;
	ulint		slot_no;
	ulint		type;

	ut_ad(mutex_own(&kernel_mutex));
	
	slot_no = thr_local_get_slot_no(os_thread_get_curr_id());

	if (srv_print_thread_releases) {
	
		printf("Suspending thread %lu to slot %lu meter %lu\n",
		os_thread_get_curr_id(), slot_no, srv_meter[SRV_RECOVERY]);
	}

	slot = srv_table_get_nth_slot(slot_no);

	type = slot->type;

	ut_ad(type >= SRV_WORKER);
	ut_ad(type <= SRV_MASTER);

	event = slot->event;
	
	slot->suspended = TRUE;

	ut_ad(srv_n_threads_active[type] > 0);

	srv_n_threads_active[type]--;

	os_event_reset(event);

	return(event);
}

/*************************************************************************
Releases threads of the type given from suspension in the thread table.
NOTE! The server mutex has to be reserved by the caller! */

ulint
srv_release_threads(
/*================*/
			/* out: number of threads released: this may be
			< n if not enough threads were suspended at the
			moment */
	ulint	type,	/* in: thread type */
	ulint	n)	/* in: number of threads to release */
{
	srv_slot_t*	slot;
	ulint		i;
	ulint		count	= 0;

	ut_ad(type >= SRV_WORKER);
	ut_ad(type <= SRV_MASTER);
	ut_ad(n > 0);
	ut_ad(mutex_own(&kernel_mutex));
	
	for (i = 0; i < OS_THREAD_MAX_N; i++) {
	
		slot = srv_table_get_nth_slot(i);

		if ((slot->type == type) && slot->suspended) {
			
			slot->suspended = FALSE;

			srv_n_threads_active[type]++;

			os_event_set(slot->event);

			if (srv_print_thread_releases) {
				printf(
		"Releasing thread %lu type %lu from slot %lu meter %lu\n",
				slot->id, type, i, srv_meter[SRV_RECOVERY]);
			}

			count++;

			if (count == n) {
				break;
			}
		}
	}

	return(count);
}

/*************************************************************************
Returns the calling thread type. */

ulint
srv_get_thread_type(void)
/*=====================*/
			/* out: SRV_COM, ... */
{
	ulint		slot_no;
	srv_slot_t*	slot;
	ulint		type;

	mutex_enter(&kernel_mutex);
	
	slot_no = thr_local_get_slot_no(os_thread_get_curr_id());

	slot = srv_table_get_nth_slot(slot_no);

	type = slot->type;

	ut_ad(type >= SRV_WORKER);
	ut_ad(type <= SRV_MASTER);

	mutex_exit(&kernel_mutex);

	return(type);
}

/***********************************************************************
Increments by 1 the count of active threads of the type given
and releases master thread if necessary. */
static
void
srv_inc_thread_count(
/*=================*/
	ulint	type)	/* in: type of the thread */
{
	mutex_enter(&kernel_mutex);

	srv_activity_count++;
	
	srv_n_threads_active[type]++;
		
	if (srv_n_threads_active[SRV_MASTER] == 0) {

		srv_release_threads(SRV_MASTER, 1);
	}

	mutex_exit(&kernel_mutex);
}

/***********************************************************************
Decrements by 1 the count of active threads of the type given. */
static
void
srv_dec_thread_count(
/*=================*/
	ulint	type)	/* in: type of the thread */

{
	mutex_enter(&kernel_mutex);

	/* FIXME: the following assertion sometimes fails: */

	if (srv_n_threads_active[type] == 0) {
		printf("Error: thread type %lu\n", type);

		ut_ad(0);
	}	

	srv_n_threads_active[type]--;

	mutex_exit(&kernel_mutex);
}

/***********************************************************************
Calculates the number of allowed utility threads for a thread to decide if
it has to suspend itself in the thread table. */
static
ulint
srv_max_n_utilities(
/*================*/
			/* out: maximum number of allowed utilities
			of the type given */
	ulint	type)	/* in: utility type */
{
	ulint	ret;

	if (srv_n_threads_active[SRV_COM] == 0) {
		if (srv_meter[type] > srv_meter_low_water[type]) {
			return(srv_n_threads[type] / 2);
		} else {
			return(0);
		}
	} else {

		if (srv_meter[type] < srv_meter_foreground[type]) {
			return(0);
		}
		ret = 1 + ((srv_n_threads[type]
		     * (ulint)(srv_meter[type] - srv_meter_foreground[type]))
		     / (ulint)(1000 - srv_meter_foreground[type]));
		if (ret > srv_n_threads[type]) {
			return(srv_n_threads[type]);
		} else {
			return(ret);
		}
	}
}

/***********************************************************************
Increments the utility meter by the value given and releases utility
threads if necessary. */

void
srv_increment_meter(
/*================*/
	ulint	type,	/* in: utility type */
	ulint	n)	/* in: value to add to meter */
{
	ulint	m;

	mutex_enter(&kernel_mutex);

	srv_meter[type] += n;

	m = srv_max_n_utilities(type);

	if (m > srv_n_threads_active[type]) {
		
		srv_release_threads(type, m - srv_n_threads_active[type]);
	}

	mutex_exit(&kernel_mutex);
}

/***********************************************************************
Releases max number of utility threads if no queries are active and
the high-water mark for the utility is exceeded. */

void
srv_release_max_if_no_queries(void)
/*===============================*/
{
	ulint	m;
	ulint	type;

	mutex_enter(&kernel_mutex);

	if (srv_n_threads_active[SRV_COM] > 0) {
		mutex_exit(&kernel_mutex);

		return;
	}

	type = SRV_RECOVERY;
	
	m = srv_n_threads[type] / 2;

	if ((srv_meter[type] > srv_meter_high_water[type])
				&& (srv_n_threads_active[type] < m)) {

		srv_release_threads(type, m - srv_n_threads_active[type]);

		printf("Releasing max background\n");
	}

	mutex_exit(&kernel_mutex);
}

/***********************************************************************
Releases one utility thread if no queries are active and
the high-water mark 2 for the utility is exceeded. */
static
void
srv_release_one_if_no_queries(void)
/*===============================*/
{
	ulint	m;
	ulint	type;

	mutex_enter(&kernel_mutex);

	if (srv_n_threads_active[SRV_COM] > 0) {
		mutex_exit(&kernel_mutex);

		return;
	}

	type = SRV_RECOVERY;
	
	m = 1;

	if ((srv_meter[type] > srv_meter_high_water2[type])
	   				&& (srv_n_threads_active[type] < m)) {

		srv_release_threads(type, m - srv_n_threads_active[type]);

		printf("Releasing one background\n");
	}

	mutex_exit(&kernel_mutex);
}

#ifdef notdefined
/***********************************************************************
Decrements the utility meter by the value given and suspends the calling
thread, which must be an utility thread of the type given, if necessary. */
static
void
srv_decrement_meter(
/*================*/
	ulint	type,	/* in: utility type */
	ulint	n)	/* in: value to subtract from meter */
{
	ulint		opt;
	os_event_t	event;
	
	mutex_enter(&kernel_mutex);

	if (srv_meter[type] < n) {
		srv_meter[type] = 0;
	} else {
		srv_meter[type] -= n;
	}

	opt = srv_max_n_utilities(type);

	if (opt < srv_n_threads_active[type]) {
		
 		event = srv_suspend_thread();
		mutex_exit(&kernel_mutex);

		os_event_wait(event);
	} else {
		mutex_exit(&kernel_mutex);
	}
}
#endif

/*************************************************************************
Implements the server console. */

ulint
srv_console(
/*========*/
			/* out: return code, not used */
	void*	arg)	/* in: argument, not used */
{
	char	command[256];

	UT_NOT_USED(arg);

	mutex_enter(&kernel_mutex);
	srv_table_reserve_slot(SRV_CONSOLE);
	mutex_exit(&kernel_mutex);

	os_event_wait(srv_sys->operational);

	for (;;) {
		scanf("%s", command);
		
		srv_inc_thread_count(SRV_CONSOLE);

		if (command[0] == 'c') {
			printf("Making checkpoint\n");

			log_make_checkpoint_at(ut_dulint_max, TRUE);

			printf("Checkpoint completed\n");

		} else if (command[0] == 'd') {
			srv_sim_disk_wait_pct = atoi(command + 1);

			printf(
			"Starting disk access simulation with pct %lu\n",
							srv_sim_disk_wait_pct);
		} else {
			printf("\nNot supported!\n");
		}

		srv_dec_thread_count(SRV_CONSOLE);
	}
	
	return(0);
}

/*************************************************************************
Creates the first communication endpoint for the server. This
first call also initializes the com0com.* module. */

void
srv_communication_init(
/*===================*/
	char*	endpoint)	/* in: server address */
{
	ulint	ret;
	ulint	len;

	srv_sys->endpoint = com_endpoint_create(COM_SHM);

	ut_a(srv_sys->endpoint);

	len = ODBC_DATAGRAM_SIZE;
	
	ret = com_endpoint_set_option(srv_sys->endpoint,
					COM_OPT_MAX_DGRAM_SIZE,
					(byte*)&len, sizeof(ulint));
	ut_a(ret == 0);

	ret = com_bind(srv_sys->endpoint, endpoint, ut_strlen(endpoint));
	
	ut_a(ret == 0);
}
	
/*************************************************************************
Implements the recovery utility. */
static
ulint
srv_recovery_thread(
/*================*/
			/* out: return code, not used */
	void*	arg)	/* in: not used */
{
	ulint	slot_no;
	os_event_t event;

	UT_NOT_USED(arg);
	
	slot_no = srv_table_reserve_slot(SRV_RECOVERY);

	os_event_wait(srv_sys->operational);

	for (;;) {
		/* Finish a possible recovery */

		srv_inc_thread_count(SRV_RECOVERY);

/*		recv_recovery_from_checkpoint_finish(); */

		srv_dec_thread_count(SRV_RECOVERY);

		mutex_enter(&kernel_mutex);
 		event = srv_suspend_thread();
		mutex_exit(&kernel_mutex);

		/* Wait for somebody to release this thread; (currently, this
		should never be released) */

		os_event_wait(event);
	}

	return(0);
}

/*************************************************************************
Implements the purge utility. */

ulint
srv_purge_thread(
/*=============*/
			/* out: return code, not used */
	void*	arg)	/* in: not used */
{
	UT_NOT_USED(arg);

	os_event_wait(srv_sys->operational);

	for (;;) {
		trx_purge();
	}

	return(0);
}

/*************************************************************************
Creates the utility threads. */

void
srv_create_utility_threads(void)
/*============================*/
{
/*      os_thread_t	thread;
 	os_thread_id_t	thr_id; */
	ulint		i;

	mutex_enter(&kernel_mutex);

	srv_n_threads[SRV_RECOVERY] = 1;
	srv_n_threads_active[SRV_RECOVERY] = 1;

	mutex_exit(&kernel_mutex);

	for (i = 0; i < 1; i++) {
	  /* thread = os_thread_create(srv_recovery_thread, NULL, &thr_id); */

	  /* ut_a(thread); */
	}

/*	thread = os_thread_create(srv_purge_thread, NULL, &thr_id);

	ut_a(thread); */
}

/*************************************************************************
Implements the communication threads. */
static
ulint
srv_com_thread(
/*===========*/
			/* out: return code; not used */
	void*	arg)	/* in: not used */
{
	byte*	msg_buf;
	byte*	addr_buf;
	ulint	msg_len;
	ulint	addr_len;
	ulint	ret;

	UT_NOT_USED(arg);

	srv_table_reserve_slot(SRV_COM);

	os_event_wait(srv_sys->operational);

	msg_buf = mem_alloc(com_endpoint_get_max_size(srv_sys->endpoint));
	addr_buf = mem_alloc(COM_MAX_ADDR_LEN);
	
	for (;;) {
		ret = com_recvfrom(srv_sys->endpoint, msg_buf,
				com_endpoint_get_max_size(srv_sys->endpoint),
				&msg_len, (char*)addr_buf, COM_MAX_ADDR_LEN,
				&addr_len);
		ut_a(ret == 0);

		srv_inc_thread_count(SRV_COM);
		
		sess_process_cli_msg(msg_buf, msg_len, addr_buf, addr_len);

/*		srv_increment_meter(SRV_RECOVERY, 1); */

		srv_dec_thread_count(SRV_COM);

		/* Release one utility thread for each utility if
		high water mark 2 is exceeded and there are no
		active queries. This is done to utilize possible
		quiet time in the server. */

		srv_release_one_if_no_queries();
	}		

	return(0);
}

/*************************************************************************
Creates the communication threads. */

void
srv_create_com_threads(void)
/*========================*/
{
  /*	os_thread_t	thread;
	os_thread_id_t	thr_id; */
	ulint		i;

	srv_n_threads[SRV_COM] = srv_n_com_threads;

	for (i = 0; i < srv_n_com_threads; i++) {
	  /* thread = os_thread_create(srv_com_thread, NULL, &thr_id); */
	  /* ut_a(thread); */
	}
}

/*************************************************************************
Implements the worker threads. */
static
ulint
srv_worker_thread(
/*==============*/
			/* out: return code, not used */
	void*	arg)	/* in: not used */
{
	os_event_t	event;
	
	UT_NOT_USED(arg);

	srv_table_reserve_slot(SRV_WORKER);

	os_event_wait(srv_sys->operational);

	for (;;) {
		mutex_enter(&kernel_mutex);
 		event = srv_suspend_thread();
		mutex_exit(&kernel_mutex);

		/* Wait for somebody to release this thread */
		os_event_wait(event);

		srv_inc_thread_count(SRV_WORKER);

		/* Check in the server task queue if there is work for this
		thread, and do the work */

		srv_que_task_queue_check();				

		srv_dec_thread_count(SRV_WORKER);

		/* Release one utility thread for each utility if
		high water mark 2 is exceeded and there are no
		active queries. This is done to utilize possible
		quiet time in the server. */

		srv_release_one_if_no_queries();
	}		

	return(0);
}

/*************************************************************************
Creates the worker threads. */

void
srv_create_worker_threads(void)
/*===========================*/
{
/*	os_thread_t	thread;
	os_thread_id_t	thr_id; */
	ulint		i;

	srv_n_threads[SRV_WORKER] = srv_n_worker_threads;
	srv_n_threads_active[SRV_WORKER] = srv_n_worker_threads;

	for (i = 0; i < srv_n_worker_threads; i++) {
	  /* thread = os_thread_create(srv_worker_thread, NULL, &thr_id); */
	  /* ut_a(thread); */
	}
}

#ifdef notdefined
/*************************************************************************
Reads a keyword and a value from a file. */

ulint
srv_read_init_val(
/*==============*/
				/* out: DB_SUCCESS or error code */
	FILE*	initfile,	/* in: file pointer */
	char*	keyword,	/* in: keyword before value(s), or NULL if
				no keyword read */
	char*	str_buf,	/* in/out: buffer for a string value to read,
				buffer size must be 10000 bytes, if NULL
				then not read */
	ulint*	num_val,	/* out:	numerical value to read, if NULL
				then not read */
	ibool	print_not_err)	/* in: if TRUE, then we will not print
				error messages to console */
{		
	ulint	ret;
	char	scan_buf[10000];

	if (keyword == NULL) {

		goto skip_keyword;
	}
	
	ret = fscanf(initfile, "%9999s", scan_buf);
	
	if (ret == 0 || ret == EOF || 0 != ut_strcmp(scan_buf, keyword)) {
		if (print_not_err) {

			return(DB_ERROR);
		}
		
		printf("Error in InnoDB booting: keyword %s not found\n",
							keyword);
		printf("from the initfile!\n");

		return(DB_ERROR);
	}
skip_keyword:
	if (num_val == NULL && str_buf == NULL) {

		return(DB_SUCCESS);
	}		

	ret = fscanf(initfile, "%9999s", scan_buf);
	
	if (ret == EOF || ret == 0) {
		if (print_not_err) {

			return(DB_ERROR);
		}

		printf(
	"Error in InnoDB booting: could not read first value after %s\n",
								keyword);
		printf("from the initfile!\n");

		return(DB_ERROR);
	}

	if (str_buf) {
		ut_memcpy(str_buf, scan_buf, 10000);

		printf("init keyword %s value %s read\n", keyword, str_buf);

		if (!num_val) {
			return(DB_SUCCESS);
		}

		ret = fscanf(initfile, "%9999s", scan_buf);
	
		if (ret == EOF || ret == 0) {

			if (print_not_err) {

				return(DB_ERROR);
			}
			
			printf(
	"Error in InnoDB booting: could not read second value after %s\n",
							keyword);
			printf("from the initfile!\n");

			return(DB_ERROR);
		}
	}

	if (ut_strlen(scan_buf) > 9) {

		if (print_not_err) {

			return(DB_ERROR);
		}

		printf(
	"Error in InnoDB booting: numerical value too big after %s\n",
								keyword);
		printf("in the initfile!\n");

		return(DB_ERROR);
	}

	*num_val = (ulint)atoi(scan_buf);

	if (*num_val >= 1000000000) {

		if (print_not_err) {

			return(DB_ERROR);
		}

		printf(
	"Error in InnoDB booting: numerical value too big after %s\n",
							keyword);
		printf("in the initfile!\n");

		return(DB_ERROR);
	}

	printf("init keyword %s value %lu read\n", keyword, *num_val);

	return(DB_SUCCESS);
}

/*************************************************************************
Reads keywords and values from an initfile. */

ulint
srv_read_initfile(
/*==============*/
				/* out: DB_SUCCESS or error code */
	FILE*	initfile)	/* in: file pointer */
{
	char	str_buf[10000];
	ulint	n;
	ulint	i;
	ulint	ulint_val;
	ulint	val1;
	ulint	val2;
	ulint	err;

	err = srv_read_init_val(initfile, "INNOBASE_DATA_HOME_DIR",
						str_buf, NULL, FALSE);
	if (err != DB_SUCCESS) return(err);

	srv_data_home = ut_malloc(ut_strlen(str_buf) + 1);
	ut_memcpy(srv_data_home, str_buf, ut_strlen(str_buf) + 1);
		
	err = srv_read_init_val(initfile,"TABLESPACE_NUMBER_OF_DATA_FILES",
							NULL, &n, FALSE);
	if (err != DB_SUCCESS) return(err);

	srv_n_data_files = n;

	srv_data_file_names = ut_malloc(n * sizeof(char*));
	srv_data_file_sizes = ut_malloc(n * sizeof(ulint));
	
	for (i = 0; i < n; i++) {
		err = srv_read_init_val(initfile,
				"DATA_FILE_PATH_AND_SIZE_MB",
						str_buf, &ulint_val, FALSE);
		if (err != DB_SUCCESS) return(err);

		srv_data_file_names[i] = ut_malloc(ut_strlen(str_buf) + 1);
		ut_memcpy(srv_data_file_names[i], str_buf,
						ut_strlen(str_buf) + 1);
		srv_data_file_sizes[i] = ulint_val
					* ((1024 * 1024) / UNIV_PAGE_SIZE);
	}		

	err = srv_read_init_val(initfile,
				"NUMBER_OF_MIRRORED_LOG_GROUPS", NULL,
						&srv_n_log_groups, FALSE);	
	if (err != DB_SUCCESS) return(err);

	err = srv_read_init_val(initfile,
				"NUMBER_OF_LOG_FILES_IN_GROUP", NULL,
						&srv_n_log_files, FALSE);
	if (err != DB_SUCCESS) return(err);

	err = srv_read_init_val(initfile, "LOG_FILE_SIZE_KB", NULL,
						&srv_log_file_size, FALSE);
	if (err != DB_SUCCESS) return(err);

	srv_log_file_size = srv_log_file_size / (UNIV_PAGE_SIZE / 1024);

	srv_log_group_home_dirs = ut_malloc(srv_n_log_files * sizeof(char*));

	for (i = 0; i < srv_n_log_groups; i++) {
	
		err = srv_read_init_val(initfile,
					"INNOBASE_LOG_GROUP_HOME_DIR",
							str_buf, NULL, FALSE);
		if (err != DB_SUCCESS) return(err);

		srv_log_group_home_dirs[i] = ut_malloc(ut_strlen(str_buf) + 1);
		ut_memcpy(srv_log_group_home_dirs[i], str_buf,
							ut_strlen(str_buf) + 1);
	}

	err = srv_read_init_val(initfile, "INNOBASE_LOG_ARCH_DIR",
						str_buf, NULL, FALSE);
	if (err != DB_SUCCESS) return(err);

	srv_arch_dir = ut_malloc(ut_strlen(str_buf) + 1);
	ut_memcpy(srv_arch_dir, str_buf, ut_strlen(str_buf) + 1);
	
	err = srv_read_init_val(initfile, "LOG_ARCHIVE_ON(1/0)", NULL,
						&srv_log_archive_on, FALSE);
	if (err != DB_SUCCESS) return(err);
							
	err = srv_read_init_val(initfile, "LOG_BUFFER_SIZE_KB", NULL,
						&srv_log_buffer_size, FALSE);
	if (err != DB_SUCCESS) return(err);

	srv_log_buffer_size = srv_log_buffer_size / (UNIV_PAGE_SIZE / 1024);

	err = srv_read_init_val(initfile, "FLUSH_LOG_AT_TRX_COMMIT(1/0)", NULL,
				&srv_flush_log_at_trx_commit, FALSE);
	if (err != DB_SUCCESS) return(err);
	
	err = srv_read_init_val(initfile, "BUFFER_POOL_SIZE_MB", NULL,
						&srv_pool_size, FALSE);
	if (err != DB_SUCCESS) return(err);

	srv_pool_size = srv_pool_size * ((1024 * 1024) / UNIV_PAGE_SIZE);
	
	err = srv_read_init_val(initfile, "ADDITIONAL_MEM_POOL_SIZE_MB", NULL,
						&srv_mem_pool_size, FALSE);
	if (err != DB_SUCCESS) return(err);
	
	srv_mem_pool_size = srv_mem_pool_size * 1024 * 1024;

	srv_lock_table_size = 20 * srv_pool_size;

	err = srv_read_init_val(initfile, "NUMBER_OF_FILE_IO_THREADS", NULL,
						&srv_n_file_io_threads, FALSE);
	if (err != DB_SUCCESS) return(err);
	
	err = srv_read_init_val(initfile, "SRV_RECOVER_FROM_BACKUP",
							NULL, NULL, TRUE);
	if (err == DB_SUCCESS) {
		srv_archive_recovery = TRUE;
		srv_archive_recovery_limit_lsn = ut_dulint_max;
		
		err = srv_read_init_val(initfile, NULL, NULL, &val1, TRUE);
		err = srv_read_init_val(initfile, NULL, NULL, &val2, TRUE);

		if (err == DB_SUCCESS) {
			srv_archive_recovery_limit_lsn =
					ut_dulint_create(val1, val2);
		}
	}	

	/* err = srv_read_init_val(initfile,
				"SYNC_NUMBER_OF_SPIN_WAIT_ROUNDS", NULL,
						&srv_n_spin_wait_rounds);

	err = srv_read_init_val(initfile, "SYNC_SPIN_WAIT_DELAY", NULL,
						&srv_spin_wait_delay); */
	return(DB_SUCCESS);
}

/*************************************************************************
Reads keywords and a values from an initfile. In case of an error, exits
from the process. */

void
srv_read_initfile(
/*==============*/
	FILE*	initfile)	/* in: file pointer */
{
	char	str_buf[10000];
	ulint	ulint_val;

	srv_read_init_val(initfile, FALSE, "SRV_ENDPOINT_NAME", str_buf,
								&ulint_val);
	ut_a(ut_strlen(str_buf) < COM_MAX_ADDR_LEN);
	
	ut_memcpy(srv_endpoint_name, str_buf, COM_MAX_ADDR_LEN);

	srv_read_init_val(initfile, TRUE, "SRV_N_COM_THREADS", str_buf,
						&srv_n_com_threads);

	srv_read_init_val(initfile, TRUE, "SRV_N_WORKER_THREADS", str_buf,
						&srv_n_worker_threads);

	srv_read_init_val(initfile, TRUE, "SYNC_N_SPIN_WAIT_ROUNDS", str_buf,
						&srv_n_spin_wait_rounds);

	srv_read_init_val(initfile, TRUE, "SYNC_SPIN_WAIT_DELAY", str_buf,
						&srv_spin_wait_delay);

	srv_read_init_val(initfile, TRUE, "THREAD_PRIORITY_BOOST", str_buf,
						&srv_priority_boost);

	srv_read_init_val(initfile, TRUE, "N_SPACES", str_buf, &srv_n_spaces);
	srv_read_init_val(initfile, TRUE, "N_FILES", str_buf, &srv_n_files);
	srv_read_init_val(initfile, TRUE, "FILE_SIZE", str_buf,
							&srv_file_size);

	srv_read_init_val(initfile, TRUE, "N_LOG_GROUPS", str_buf,
							&srv_n_log_groups);
	srv_read_init_val(initfile, TRUE, "N_LOG_FILES", str_buf,
							&srv_n_log_files);
	srv_read_init_val(initfile, TRUE, "LOG_FILE_SIZE", str_buf,
							&srv_log_file_size);
	srv_read_init_val(initfile, TRUE, "LOG_ARCHIVE_ON", str_buf,
							&srv_log_archive_on);
	srv_read_init_val(initfile, TRUE, "LOG_BUFFER_SIZE", str_buf,
						&srv_log_buffer_size);
	srv_read_init_val(initfile, TRUE, "FLUSH_LOG_AT_TRX_COMMIT", str_buf,
						&srv_flush_log_at_trx_commit);
	
	
	srv_read_init_val(initfile, TRUE, "POOL_SIZE", str_buf,
						&srv_pool_size);
	srv_read_init_val(initfile, TRUE, "MEM_POOL_SIZE", str_buf,
						&srv_mem_pool_size);
	srv_read_init_val(initfile, TRUE, "LOCK_TABLE_SIZE", str_buf,
						&srv_lock_table_size);

	srv_read_init_val(initfile, TRUE, "SIM_DISK_WAIT_PCT", str_buf,
						&srv_sim_disk_wait_pct);

	srv_read_init_val(initfile, TRUE, "SIM_DISK_WAIT_LEN", str_buf,
						&srv_sim_disk_wait_len);

	srv_read_init_val(initfile, TRUE, "SIM_DISK_WAIT_BY_YIELD", str_buf,
						&srv_sim_disk_wait_by_yield);

	srv_read_init_val(initfile, TRUE, "SIM_DISK_WAIT_BY_WAIT", str_buf,
						&srv_sim_disk_wait_by_wait);

	srv_read_init_val(initfile, TRUE, "MEASURE_CONTENTION", str_buf,
						&srv_measure_contention);

	srv_read_init_val(initfile, TRUE, "MEASURE_BY_SPIN", str_buf,
						&srv_measure_by_spin);
	

	srv_read_init_val(initfile, TRUE, "PRINT_THREAD_RELEASES", str_buf,
						&srv_print_thread_releases);
	
	srv_read_init_val(initfile, TRUE, "PRINT_LOCK_WAITS", str_buf,
						&srv_print_lock_waits);
	if (srv_print_lock_waits) {
		lock_print_waits = TRUE;
	}
	
	srv_read_init_val(initfile, TRUE, "PRINT_BUF_IO", str_buf,
						&srv_print_buf_io);
	if (srv_print_buf_io) {
		buf_debug_prints = TRUE;
	}	
	
	srv_read_init_val(initfile, TRUE, "PRINT_LOG_IO", str_buf,
						&srv_print_log_io);
	if (srv_print_log_io) {
		log_debug_writes = TRUE;
	}	
	
	srv_read_init_val(initfile, TRUE, "PRINT_PARSED_SQL", str_buf,
						&srv_print_parsed_sql);
	if (srv_print_parsed_sql) {
		pars_print_lexed = TRUE;
	}

	srv_read_init_val(initfile, TRUE, "PRINT_LATCH_WAITS", str_buf,
						&srv_print_latch_waits);

	srv_read_init_val(initfile, TRUE, "TEST_EXTRA_MUTEXES", str_buf,
						&srv_test_extra_mutexes);
	srv_read_init_val(initfile, TRUE, "TEST_NOCACHE", str_buf,
						&srv_test_nocache);
	srv_read_init_val(initfile, TRUE, "TEST_CACHE_EVICT", str_buf,
						&srv_test_cache_evict);

	srv_read_init_val(initfile, TRUE, "TEST_SYNC", str_buf,
						&srv_test_sync);
	srv_read_init_val(initfile, TRUE, "TEST_N_THREADS", str_buf,
						&srv_test_n_threads);
	srv_read_init_val(initfile, TRUE, "TEST_N_LOOPS", str_buf,
						&srv_test_n_loops);
	srv_read_init_val(initfile, TRUE, "TEST_N_FREE_RNDS", str_buf,
						&srv_test_n_free_rnds);
	srv_read_init_val(initfile, TRUE, "TEST_N_RESERVED_RNDS", str_buf,
						&srv_test_n_reserved_rnds);
	srv_read_init_val(initfile, TRUE, "TEST_N_MUTEXES", str_buf,
						&srv_test_n_mutexes);
	srv_read_init_val(initfile, TRUE, "TEST_ARRAY_SIZE", str_buf,
						&srv_test_array_size);
}
#endif

/*************************************************************************
Initializes the server. */
static
void
srv_init(void)
/*==========*/
{
	srv_conc_slot_t* 	conc_slot;
	srv_slot_t*		slot;
	ulint			i;

	srv_sys = mem_alloc(sizeof(srv_sys_t));

	kernel_mutex_temp = mem_alloc(sizeof(mutex_t));
	mutex_create(&kernel_mutex);
	mutex_set_level(&kernel_mutex, SYNC_KERNEL);
	
	srv_sys->threads = mem_alloc(OS_THREAD_MAX_N * sizeof(srv_slot_t));

	for (i = 0; i < OS_THREAD_MAX_N; i++) {
		slot = srv_table_get_nth_slot(i);
		slot->in_use = FALSE;
		slot->event = os_event_create(NULL);
		ut_a(slot->event);
	}

	srv_mysql_table = mem_alloc(OS_THREAD_MAX_N * sizeof(srv_slot_t));

	for (i = 0; i < OS_THREAD_MAX_N; i++) {
		slot = srv_mysql_table + i;
		slot->in_use = FALSE;
		slot->event = os_event_create(NULL);
		ut_a(slot->event);
	}

	srv_lock_timeout_thread_event = os_event_create(NULL);
	
	for (i = 0; i < SRV_MASTER + 1; i++) {
		srv_n_threads_active[i] = 0;
		srv_n_threads[i] = 0;
		srv_meter[i] = 30;
		srv_meter_low_water[i] = 50;
		srv_meter_high_water[i] = 100;
		srv_meter_high_water2[i] = 200;
		srv_meter_foreground[i] = 250;
	}
	
	srv_sys->operational = os_event_create(NULL);

	ut_a(srv_sys->operational);

	UT_LIST_INIT(srv_sys->tasks);

	/* Init the server concurrency restriction data structures */

	os_fast_mutex_init(&srv_conc_mutex);
	
	UT_LIST_INIT(srv_conc_queue);
	
	for (i = 0; i < OS_THREAD_MAX_N; i++) {
		conc_slot = srv_conc_slots + i;
		conc_slot->reserved = FALSE;
		conc_slot->event = os_event_create(NULL);
		ut_a(conc_slot->event);
	}
}	
	
/*************************************************************************
Initializes the synchronization primitives, memory system, and the thread
local storage. */
static
void
srv_general_init(void)
/*==================*/
{
	sync_init();
	mem_init(srv_mem_pool_size);
	thr_local_init();
}

/*======================= InnoDB Server FIFO queue =======================*/

/*************************************************************************
Puts an OS thread to wait if there are too many concurrent threads
(>= srv_thread_concurrency) inside InnoDB. The threads wait in a FIFO queue. */

void
srv_conc_enter_innodb(
/*==================*/
	trx_t*	trx)	/* in: transaction object associated with the
			thread */
{
	srv_conc_slot_t*	slot;
	ulint			i;

	if (srv_thread_concurrency >= 500) {
		/* Disable the concurrency check */
	
		return;
	}

	/* If trx has 'free tickets' to enter the engine left, then use one
	such ticket */

	if (trx->n_tickets_to_enter_innodb > 0) {
		trx->n_tickets_to_enter_innodb--;

		return;
	}

	os_fast_mutex_lock(&srv_conc_mutex);

	if (srv_conc_n_threads < (lint)srv_thread_concurrency) {

		srv_conc_n_threads++;
		trx->declared_to_be_inside_innodb = TRUE;
		trx->n_tickets_to_enter_innodb = SRV_FREE_TICKETS_TO_ENTER;
		
		os_fast_mutex_unlock(&srv_conc_mutex);

		return;
	}
	
	/* Too many threads inside: put the current thread to a queue */

	for (i = 0; i < OS_THREAD_MAX_N; i++) {
		slot = srv_conc_slots + i;

		if (!slot->reserved) {
			break;
		}
	}

	if (i == OS_THREAD_MAX_N) {
		/* Could not find a free wait slot, we must let the
		thread enter */

		srv_conc_n_threads++;
		trx->declared_to_be_inside_innodb = TRUE;
		trx->n_tickets_to_enter_innodb = 0;

		os_fast_mutex_unlock(&srv_conc_mutex);

		return;
	}

	/* Release possible search system latch this thread has */
	if (trx->has_search_latch) {
		trx_search_latch_release_if_reserved(trx);
	}

	/* Add to the queue */
	slot->reserved = TRUE;
	slot->wait_ended = FALSE;
	
	UT_LIST_ADD_LAST(srv_conc_queue, srv_conc_queue, slot);

	os_event_reset(slot->event);

	srv_conc_n_waiting_threads++;

	os_fast_mutex_unlock(&srv_conc_mutex);

	/* Go to wait for the event; when a thread leaves InnoDB it will
	release this thread */

	os_event_wait(slot->event);

	os_fast_mutex_lock(&srv_conc_mutex);

	srv_conc_n_waiting_threads--;

	/* NOTE that the thread which released this thread already
	incremented the thread counter on behalf of this thread */

	slot->reserved = FALSE;

	UT_LIST_REMOVE(srv_conc_queue, srv_conc_queue, slot);

	trx->declared_to_be_inside_innodb = TRUE;
	trx->n_tickets_to_enter_innodb = SRV_FREE_TICKETS_TO_ENTER;

	os_fast_mutex_unlock(&srv_conc_mutex);
}

/*************************************************************************
This lets a thread enter InnoDB regardless of the number of threads inside
InnoDB. This must be called when a thread ends a lock wait. */

void
srv_conc_force_enter_innodb(
/*========================*/
	trx_t*	trx)	/* in: transaction object associated with the
			thread */
{
	if (srv_thread_concurrency >= 500) {
	
		return;
	}

	os_fast_mutex_lock(&srv_conc_mutex);

	srv_conc_n_threads++;
	trx->declared_to_be_inside_innodb = TRUE;
	trx->n_tickets_to_enter_innodb = 0;

	os_fast_mutex_unlock(&srv_conc_mutex);
}

/*************************************************************************
This must be called when a thread exits InnoDB in a lock wait or at the
end of an SQL statement. */

void
srv_conc_force_exit_innodb(
/*=======================*/
	trx_t*	trx)	/* in: transaction object associated with the
			thread */
{
	srv_conc_slot_t*	slot	= NULL;

	if (srv_thread_concurrency >= 500) {
	
		return;
	}

	if (trx->declared_to_be_inside_innodb == FALSE) {
		
		return;
	}

	os_fast_mutex_lock(&srv_conc_mutex);

	srv_conc_n_threads--;
	trx->declared_to_be_inside_innodb = FALSE;
	trx->n_tickets_to_enter_innodb = 0;

	if (srv_conc_n_threads < (lint)srv_thread_concurrency) {
		/* Look for a slot where a thread is waiting and no other
		thread has yet released the thread */
	
		slot = UT_LIST_GET_FIRST(srv_conc_queue);

		while (slot && slot->wait_ended == TRUE) {
			slot = UT_LIST_GET_NEXT(srv_conc_queue, slot);
		}

		if (slot != NULL) {
			slot->wait_ended = TRUE;

			/* We increment the count on behalf of the released
			thread */

			srv_conc_n_threads++;
		}
	}

	os_fast_mutex_unlock(&srv_conc_mutex);

	if (slot != NULL) {
		os_event_set(slot->event);
	}
}

/*************************************************************************
This must be called when a thread exits InnoDB. */

void
srv_conc_exit_innodb(
/*=================*/
	trx_t*	trx)	/* in: transaction object associated with the
			thread */
{
	srv_conc_slot_t*	slot	= NULL;

	if (srv_thread_concurrency >= 500) {
	
		return;
	}

	if (trx->n_tickets_to_enter_innodb > 0) {
		/* We will pretend the thread is still inside InnoDB though it
		now leaves the InnoDB engine. In this way we save
		a lot of semaphore operations. srv_conc_force_exit_innodb is
		used to declare the thread definitely outside InnoDB. It
		should be called when there is a lock wait or an SQL statement
		ends. */

		return;
	}

	srv_conc_force_exit_innodb(trx);
}

/*========================================================================*/

/*************************************************************************
Normalizes init parameter values to use units we use inside InnoDB. */
static
ulint
srv_normalize_init_values(void)
/*===========================*/
				/* out: DB_SUCCESS or error code */
{
	ulint	n;
	ulint	i;

	n = srv_n_data_files;
	
	for (i = 0; i < n; i++) {
		srv_data_file_sizes[i] = srv_data_file_sizes[i]
					* ((1024 * 1024) / UNIV_PAGE_SIZE);
	}		

	srv_log_file_size = srv_log_file_size / UNIV_PAGE_SIZE;

	srv_log_buffer_size = srv_log_buffer_size / UNIV_PAGE_SIZE;

	srv_pool_size = srv_pool_size / UNIV_PAGE_SIZE;
	
	srv_lock_table_size = 20 * srv_pool_size;

	return(DB_SUCCESS);
}

/*************************************************************************
Boots the InnoDB server. */

ulint
srv_boot(void)
/*==========*/
			/* out: DB_SUCCESS or error code */
{
	ulint	err;

	/* Transform the init parameter values given by MySQL to
	use units we use inside InnoDB: */
	
	err = srv_normalize_init_values();

	if (err != DB_SUCCESS) {
		return(err);
	}
	
	/* Initialize synchronization primitives, memory management, and thread
	local storage */
	
	srv_general_init();

	/* Initialize this module */

	srv_init();

	return(DB_SUCCESS);
}

/*************************************************************************
Reserves a slot in the thread table for the current MySQL OS thread.
NOTE! The kernel mutex has to be reserved by the caller! */
static
srv_slot_t*
srv_table_reserve_slot_for_mysql(void)
/*==================================*/
			/* out: reserved slot */
{
	srv_slot_t*	slot;
	ulint		i;

	ut_ad(mutex_own(&kernel_mutex));

	i = 0;
	slot = srv_mysql_table + i;

	while (slot->in_use) {
		i++;
		ut_a(i < OS_THREAD_MAX_N);
		
		slot = srv_mysql_table + i;
	}

	ut_a(slot->in_use == FALSE);
	
	slot->in_use = TRUE;
	slot->id = os_thread_get_curr_id();
	slot->handle = os_thread_get_curr();

	return(slot);
}

/*******************************************************************
Puts a MySQL OS thread to wait for a lock to be released. */

ibool
srv_suspend_mysql_thread(
/*=====================*/
				/* out: TRUE if the lock wait timeout was
				exceeded */
	que_thr_t*	thr)	/* in: query thread associated with
				the MySQL OS thread */
{
	srv_slot_t*	slot;
	os_event_t	event;
	double		wait_time;

	ut_ad(!mutex_own(&kernel_mutex));

	os_event_set(srv_lock_timeout_thread_event);

	mutex_enter(&kernel_mutex);

	if (thr->state == QUE_THR_RUNNING) {

		/* The lock has already been released: no need to suspend */

		mutex_exit(&kernel_mutex);

		return(FALSE);
	}
	
	slot = srv_table_reserve_slot_for_mysql();

	event = slot->event;
	
	slot->thr = thr;

	os_event_reset(event);	

	slot->suspend_time = ut_time();

	/* Wake the lock timeout monitor thread, if it is suspended */

	os_event_set(srv_lock_timeout_thread_event);
	
	mutex_exit(&kernel_mutex);

	/* We must declare this OS thread to exit InnoDB, since a possible
	other thread holding a lock which this thread waits for must be
	allowed to enter, sooner or later */
	
	srv_conc_force_exit_innodb(thr_get_trx(thr));

	/* Wait for the release */
	
	os_event_wait(event);

	/* Return back inside InnoDB */
	
	srv_conc_force_enter_innodb(thr_get_trx(thr));

	mutex_enter(&kernel_mutex);

	/* Release the slot for others to use */
	
	slot->in_use = FALSE;

	wait_time = ut_difftime(ut_time(), slot->suspend_time);
	
	mutex_exit(&kernel_mutex);

	if (srv_lock_wait_timeout < 100000000 && 
	    			wait_time > (double)srv_lock_wait_timeout) {
	   	return(TRUE);
	}

	return(FALSE);
}

/************************************************************************
Releases a MySQL OS thread waiting for a lock to be released, if the
thread is already suspended. */

void
srv_release_mysql_thread_if_suspended(
/*==================================*/
	que_thr_t*	thr)	/* in: query thread associated with the
				MySQL OS thread  */
{
	srv_slot_t*	slot;
	ulint		i;
	
	ut_ad(mutex_own(&kernel_mutex));

	for (i = 0; i < OS_THREAD_MAX_N; i++) {

		slot = srv_mysql_table + i;

		if (slot->in_use && slot->thr == thr) {
			/* Found */

			os_event_set(slot->event);

			return;
		}
	}

	/* not found */
}

/*************************************************************************
A thread which wakes up threads whose lock wait may have lasted too long.
This also prints the info output by various InnoDB monitors. */

#ifndef __WIN__
void*
#else
ulint
#endif
srv_lock_timeout_and_monitor_thread(
/*================================*/
			/* out: a dummy parameter */
	void*	arg)	/* in: a dummy parameter required by
			os_thread_create */
{
	srv_slot_t*	slot;
	double		time_elapsed;
	time_t          current_time;
	time_t          last_monitor_time;
	time_t          last_table_monitor_time;
	ibool		some_waits;
	double		wait_time;
	ulint		i;

	UT_NOT_USED(arg);
	last_monitor_time = time(NULL);
	last_table_monitor_time = time(NULL);
loop:
	srv_lock_timeout_and_monitor_active = TRUE;

	/* When someone is waiting for a lock, we wake up every second
	and check if a timeout has passed for a lock wait */

	os_thread_sleep(1000000);		

	/* In case mutex_exit is not a memory barrier, it is
	theoretically possible some threads are left waiting though
	the semaphore is already released. Wake up those threads: */
	
	sync_arr_wake_threads_if_sema_free();

	current_time = time(NULL);

	time_elapsed = difftime(current_time, last_monitor_time);
	
	if (time_elapsed > 15) {

	    if (srv_print_innodb_monitor) {

	    	last_monitor_time = time(NULL);
	
	        printf("=====================================\n");
		ut_print_timestamp(stdout);
	
		printf(" INNODB MONITOR OUTPUT\n"
	       	       "=====================================\n");
		printf("----------\n"
		       "SEMAPHORES\n"
		       "----------\n");
		sync_print();
		printf("------------\n"
		       "TRANSACTIONS\n"
		       "------------\n");
		lock_print_info();
		printf("--------\n"
		       "FILE I/O\n"
		       "--------\n");
		os_aio_print();
		printf("-------------\n"
		       "INSERT BUFFER\n"
		       "-------------\n");
		ibuf_print();
		printf("---\n"
		       "LOG\n"
		       "---\n");
		log_print();
		printf("----------------------\n"
		       "BUFFER POOL AND MEMORY\n"
		       "----------------------\n");
		printf(
	"Total memory allocated %lu; in additional pool allocated %lu\n",
				ut_total_allocated_memory,
				mem_pool_get_reserved(mem_comm_pool));
		buf_print_io();
		printf("--------------\n"
		       "ROW OPERATIONS\n"
		       "--------------\n");
		printf(
	"%ld queries inside InnoDB, %ld queries in queue; main thread: %s\n",
			srv_conc_n_threads, srv_conc_n_waiting_threads,
			srv_main_thread_op_info);
		printf(
	"Number of rows inserted %lu, updated %lu, deleted %lu, read %lu\n",
			srv_n_rows_inserted, 
			srv_n_rows_updated, 
			srv_n_rows_deleted, 
			srv_n_rows_read);
		printf(
	"%.2f inserts/s, %.2f updates/s, %.2f deletes/s, %.2f reads/s\n",
			(srv_n_rows_inserted - srv_n_rows_inserted_old)
						/ time_elapsed,
			(srv_n_rows_updated - srv_n_rows_updated_old)
						/ time_elapsed,
			(srv_n_rows_deleted - srv_n_rows_deleted_old)
						/ time_elapsed,
			(srv_n_rows_read - srv_n_rows_read_old)
						/ time_elapsed);

		srv_n_rows_inserted_old = srv_n_rows_inserted;
		srv_n_rows_updated_old = srv_n_rows_updated;
		srv_n_rows_deleted_old = srv_n_rows_deleted;
		srv_n_rows_read_old = srv_n_rows_read;

		printf("----------------------------\n"
		       "END OF INNODB MONITOR OUTPUT\n"
		       "============================\n");
            }

            if (srv_print_innodb_tablespace_monitor
                && difftime(current_time, last_table_monitor_time) > 60) {

		last_table_monitor_time = time(NULL);	

		printf("================================================\n");

		ut_print_timestamp(stdout);

		printf(" INNODB TABLESPACE MONITOR OUTPUT\n"
		       "================================================\n");
	       
		fsp_print(0);
		fprintf(stderr, "Validating tablespace\n");
		fsp_validate(0);
		fprintf(stderr, "Validation ok\n");
		printf("---------------------------------------\n"
	       		"END OF INNODB TABLESPACE MONITOR OUTPUT\n"
	       		"=======================================\n");
	    }

	    if (srv_print_innodb_table_monitor
                && difftime(current_time, last_table_monitor_time) > 60) {

		last_table_monitor_time = time(NULL);	

		printf("===========================================\n");

		ut_print_timestamp(stdout);

		printf(" INNODB TABLE MONITOR OUTPUT\n"
		       "===========================================\n");
	    	dict_print();

		printf("-----------------------------------\n"
	       		"END OF INNODB TABLE MONITOR OUTPUT\n"
	       		"==================================\n");
	    }
	}

	mutex_enter(&kernel_mutex);

	some_waits = FALSE;

	/* Check of all slots if a thread is waiting there, and if it
	has exceeded the time limit */
	
	for (i = 0; i < OS_THREAD_MAX_N; i++) {

		slot = srv_mysql_table + i;

		if (slot->in_use) {
			some_waits = TRUE;

			wait_time = ut_difftime(ut_time(), slot->suspend_time);
			
			if (srv_lock_wait_timeout < 100000000 && 
	    			(wait_time > (double) srv_lock_wait_timeout
						|| wait_time < 0)) {

				/* Timeout exceeded or a wrap-around in system
				time counter: cancel the lock request queued
				by the transaction and release possible
				other transactions waiting behind; it is
				possible that the lock has already been
				granted: in that case do nothing */

			        if (thr_get_trx(slot->thr)->wait_lock) {
				        lock_cancel_waiting_and_release(
				          thr_get_trx(slot->thr)->wait_lock);
			        }
			}
		}
	}

	os_event_reset(srv_lock_timeout_thread_event);

	mutex_exit(&kernel_mutex);

	if (srv_shutdown_state >= SRV_SHUTDOWN_CLEANUP) {
		goto exit_func;
	}

	if (some_waits || srv_print_innodb_monitor
			|| srv_print_innodb_lock_monitor
			|| srv_print_innodb_tablespace_monitor
			|| srv_print_innodb_table_monitor) {
		goto loop;
	}

	/* No one was waiting for a lock and no monitor was active:
	suspend this thread */

	srv_lock_timeout_and_monitor_active = FALSE;

	os_event_wait(srv_lock_timeout_thread_event);

	goto loop;

exit_func:
	srv_lock_timeout_and_monitor_active = FALSE;

#ifndef __WIN__
        return(NULL);
#else
	return(0);
#endif
}

/*************************************************************************
A thread which prints warnings about semaphore waits which have lasted
too long. These can be used to track bugs which cause hangs. */

#ifndef __WIN__
void*
#else
ulint
#endif
srv_error_monitor_thread(
/*=====================*/
			/* out: a dummy parameter */
	void*	arg)	/* in: a dummy parameter required by
			os_thread_create */
{
	UT_NOT_USED(arg);
loop:
	srv_error_monitor_active = TRUE;

	os_thread_sleep(10000000);

	sync_array_print_long_waits();

	/* Flush stdout and stderr so that a database user gets their output
	to possible MySQL error file */

	fflush(stderr);
	fflush(stdout);

	if (srv_shutdown_state < SRV_SHUTDOWN_LAST_PHASE) {

		goto loop;
	}

	srv_error_monitor_active = FALSE;

#ifndef __WIN__
        return(NULL);
#else
	return(0);
#endif
}

/***********************************************************************
Tells the InnoDB server that there has been activity in the database
and wakes up the master thread if it is suspended (not sleeping). Used
in the MySQL interface. Note that there is a small chance that the master
thread stays suspended (we do not protect our operation with the kernel
mutex, for performace reasons). */

void
srv_active_wake_master_thread(void)
/*===============================*/
{
	srv_activity_count++;
			
	if (srv_n_threads_active[SRV_MASTER] == 0) {

		mutex_enter(&kernel_mutex);

		srv_release_threads(SRV_MASTER, 1);

		mutex_exit(&kernel_mutex);
	}
}

/***********************************************************************
Wakes up the master thread if it is suspended or being suspended. */

void
srv_wake_master_thread(void)
/*========================*/
{
	srv_activity_count++;
			
	mutex_enter(&kernel_mutex);

	srv_release_threads(SRV_MASTER, 1);

	mutex_exit(&kernel_mutex);
}

/*************************************************************************
The master thread controlling the server. */

#ifndef __WIN__
void*
#else
ulint
#endif
srv_master_thread(
/*==============*/
			/* out: a dummy parameter */
	void*	arg)	/* in: a dummy parameter required by
			os_thread_create */
{
	os_event_t	event;
	time_t          last_flush_time;
	time_t          current_time;
	ulint		old_activity_count;
	ulint		n_pages_purged;
	ulint		n_bytes_merged;
	ulint		n_pages_flushed;
	ulint		n_bytes_archived;
	ulint		n_tables_to_drop;
	ulint		n_ios;
	ulint		n_ios_old;
	ulint		n_ios_very_old;
	ulint		n_pend_ios;
	ulint		i;
	
	UT_NOT_USED(arg);

	srv_table_reserve_slot(SRV_MASTER);	

	mutex_enter(&kernel_mutex);

	srv_n_threads_active[SRV_MASTER]++;

	mutex_exit(&kernel_mutex);

	os_event_set(srv_sys->operational);
loop:
	srv_main_thread_op_info = "reserving kernel mutex";

	n_ios_very_old = log_sys->n_log_ios + buf_pool->n_pages_read
						+ buf_pool->n_pages_written;
	mutex_enter(&kernel_mutex);

	old_activity_count = srv_activity_count;

	mutex_exit(&kernel_mutex);

	/* We run purge and a batch of ibuf_contract every 10 seconds, even
	if the server were active: */

	for (i = 0; i < 10; i++) {
		n_ios_old = log_sys->n_log_ios + buf_pool->n_pages_read
						+ buf_pool->n_pages_written;
		srv_main_thread_op_info = "sleeping";
		os_thread_sleep(1000000);

		/* ALTER TABLE in MySQL requires on Unix that the table handler
		can drop tables lazily after there no longer are SELECT
		queries to them. */

		srv_main_thread_op_info = "doing background drop tables";

		row_drop_tables_for_mysql_in_background();

		srv_main_thread_op_info = "";

		if (srv_force_recovery >= SRV_FORCE_NO_BACKGROUND) {

			goto suspend_thread;
		}

		/* We flush the log once in a second even if no commit
		is issued or the we have specified in my.cnf no flush
		at transaction commit */

		srv_main_thread_op_info = "flushing log";
		log_flush_up_to(ut_dulint_max, LOG_WAIT_ONE_GROUP);

		/* If there were less than 10 i/os during the
		one second sleep, we assume that there is free
		disk i/o capacity available, and it makes sense to
		do an insert buffer merge. */

		n_pend_ios = buf_get_n_pending_ios()
						+ log_sys->n_pending_writes;
		n_ios = log_sys->n_log_ios + buf_pool->n_pages_read
						+ buf_pool->n_pages_written;
		if (n_pend_ios < 3 && (n_ios - n_ios_old < 10)) {
			srv_main_thread_op_info = "doing insert buffer merge";
			ibuf_contract_for_n_pages(TRUE, 5);

			srv_main_thread_op_info = "flushing log";
			log_flush_up_to(ut_dulint_max, LOG_WAIT_ONE_GROUP);
		}
		
		if (srv_fast_shutdown && srv_shutdown_state > 0) {

			goto background_loop;
		}

		if (srv_activity_count == old_activity_count) {

			if (srv_print_thread_releases) {
				printf("Master thread wakes up!\n");
			}

			goto background_loop;
		}
	}

	if (srv_print_thread_releases) {
		printf("Master thread wakes up!\n");
	}

#ifdef MEM_PERIODIC_CHECK
	/* Check magic numbers of every allocated mem block once in 10
	seconds */
	mem_validate_all_blocks();
#endif	
	/* If there were less than 200 i/os during the 10 second period,
	we assume that there is free disk i/o capacity available, and it
	makes sense to do a buffer pool flush. */

	n_pend_ios = buf_get_n_pending_ios() + log_sys->n_pending_writes;
	n_ios = log_sys->n_log_ios + buf_pool->n_pages_read
						+ buf_pool->n_pages_written;
	if (n_pend_ios < 3 && (n_ios - n_ios_very_old < 200)) {

		srv_main_thread_op_info = "flushing buffer pool pages";
		buf_flush_batch(BUF_FLUSH_LIST, 50, ut_dulint_max);

		srv_main_thread_op_info = "flushing log";
		log_flush_up_to(ut_dulint_max, LOG_WAIT_ONE_GROUP);
	}

	/* We run a batch of insert buffer merge every 10 seconds,
	even if the server were active */

	srv_main_thread_op_info = "doing insert buffer merge";
	ibuf_contract_for_n_pages(TRUE, 5);

	srv_main_thread_op_info = "flushing log";
	log_flush_up_to(ut_dulint_max, LOG_WAIT_ONE_GROUP);

	/* We run a full purge every 10 seconds, even if the server
	were active */
	
	n_pages_purged = 1;

	last_flush_time = time(NULL);

	while (n_pages_purged) {

		if (srv_fast_shutdown && srv_shutdown_state > 0) {

			goto background_loop;
		}

		srv_main_thread_op_info = "purging";
		n_pages_purged = trx_purge();

		current_time = time(NULL);

		if (difftime(current_time, last_flush_time) > 1) {
			srv_main_thread_op_info = "flushing log";

		        log_flush_up_to(ut_dulint_max, LOG_WAIT_ONE_GROUP);
			last_flush_time = current_time;
		}
	}

background_loop:
	/* In this loop we run background operations when the server
	is quiet and we also come here about once in 10 seconds */

	srv_main_thread_op_info = "doing background drop tables";

	n_tables_to_drop = row_drop_tables_for_mysql_in_background();

	srv_main_thread_op_info = "";
	
	srv_main_thread_op_info = "flushing buffer pool pages";

	/* Flush a few oldest pages to make the checkpoint younger */

	n_pages_flushed = buf_flush_batch(BUF_FLUSH_LIST, 10, ut_dulint_max);

	srv_main_thread_op_info = "making checkpoint";

	/* Make a new checkpoint about once in 10 seconds */

	log_checkpoint(TRUE, FALSE);

	srv_main_thread_op_info = "reserving kernel mutex";

	mutex_enter(&kernel_mutex);
	if (srv_activity_count != old_activity_count) {
		mutex_exit(&kernel_mutex);
		goto loop;
	}
	old_activity_count = srv_activity_count;
	mutex_exit(&kernel_mutex);

	/* The server has been quiet for a while: start running background
	operations */
		
	srv_main_thread_op_info = "purging";

	n_pages_purged = trx_purge();

	srv_main_thread_op_info = "reserving kernel mutex";

	mutex_enter(&kernel_mutex);
	if (srv_activity_count != old_activity_count) {
		mutex_exit(&kernel_mutex);
		goto loop;
	}
	mutex_exit(&kernel_mutex);

	srv_main_thread_op_info = "doing insert buffer merge";
	n_bytes_merged = ibuf_contract_for_n_pages(TRUE, 20);

	srv_main_thread_op_info = "reserving kernel mutex";

	mutex_enter(&kernel_mutex);
	if (srv_activity_count != old_activity_count) {
		mutex_exit(&kernel_mutex);
		goto loop;
	}
	mutex_exit(&kernel_mutex);
	
	srv_main_thread_op_info = "flushing buffer pool pages";
	n_pages_flushed = buf_flush_batch(BUF_FLUSH_LIST, 100, ut_dulint_max);

	srv_main_thread_op_info = "reserving kernel mutex";

	mutex_enter(&kernel_mutex);
	if (srv_activity_count != old_activity_count) {
		mutex_exit(&kernel_mutex);
		goto loop;
	}
	mutex_exit(&kernel_mutex);
	
	srv_main_thread_op_info = "waiting for buffer pool flush to end";
	buf_flush_wait_batch_end(BUF_FLUSH_LIST);

	srv_main_thread_op_info = "making checkpoint";

	log_checkpoint(TRUE, FALSE);

	srv_main_thread_op_info = "reserving kernel mutex";

	mutex_enter(&kernel_mutex);
	if (srv_activity_count != old_activity_count) {
		mutex_exit(&kernel_mutex);
		goto loop;
	}
	mutex_exit(&kernel_mutex);

	srv_main_thread_op_info = "archiving log (if log archive is on)";
	
	log_archive_do(FALSE, &n_bytes_archived);

	if (srv_fast_shutdown && srv_shutdown_state > 0) {
		if (n_tables_to_drop + n_pages_flushed
				+ n_bytes_archived != 0) {

			goto background_loop;
		}
	} else if (n_tables_to_drop +
		n_pages_purged + n_bytes_merged + n_pages_flushed
						+ n_bytes_archived != 0) {
		goto background_loop;
	}
		
/*	mem_print_new_info();
 */

#ifdef UNIV_SEARCH_PERF_STAT
/*	btr_search_print_info(); */
#endif
	/* There is no work for background operations either: suspend
	master thread to wait for more server activity */
	
suspend_thread:
	srv_main_thread_op_info = "suspending";

	mutex_enter(&kernel_mutex);

	if (row_get_background_drop_list_len_low() > 0) {
		mutex_exit(&kernel_mutex);

		goto loop;
	}

	event = srv_suspend_thread();

	mutex_exit(&kernel_mutex);

	srv_main_thread_op_info = "waiting for server activity";

	os_event_wait(event);

	goto loop;

#ifndef __WIN__
        return(NULL);
#else
	return(0);
#endif
}
