/************************************************************************
Starts the Innobase database server

(c) 1996-2000 Innobase Oy

Created 2/16/1996 Heikki Tuuri
*************************************************************************/

#include "os0proc.h"
#include "sync0sync.h"
#include "ut0mem.h"
#include "mem0mem.h"
#include "mem0pool.h"
#include "data0data.h"
#include "data0type.h"
#include "dict0dict.h"
#include "buf0buf.h"
#include "buf0flu.h"
#include "buf0rea.h"
#include "os0file.h"
#include "os0thread.h"
#include "fil0fil.h"
#include "fsp0fsp.h"
#include "rem0rec.h"
#include "rem0cmp.h"
#include "mtr0mtr.h"
#include "log0log.h"
#include "log0recv.h"
#include "page0page.h"
#include "page0cur.h"
#include "trx0trx.h"
#include "dict0boot.h"
#include "trx0sys.h"
#include "dict0crea.h"
#include "btr0btr.h"
#include "btr0pcur.h"
#include "btr0cur.h"
#include "btr0sea.h"
#include "rem0rec.h"
#include "srv0srv.h"
#include "que0que.h"
#include "com0com.h"
#include "usr0sess.h"
#include "lock0lock.h"
#include "trx0roll.h"
#include "trx0purge.h"
#include "row0ins.h"
#include "row0sel.h"
#include "row0upd.h"
#include "row0row.h"
#include "row0mysql.h"
#include "lock0lock.h"
#include "ibuf0ibuf.h"
#include "pars0pars.h"
#include "btr0sea.h"
#include "srv0start.h"
#include "que0que.h"

ibool           srv_is_being_started = FALSE;
ibool           srv_was_started      = FALSE;

ibool		measure_cont	= FALSE;

os_file_t	files[1000];

mutex_t		ios_mutex;
ulint		ios;

#define SRV_MAX_N_IO_THREADS		1000

ulint		n[SRV_MAX_N_IO_THREADS + 5];
os_thread_id_t	thread_ids[SRV_MAX_N_IO_THREADS + 5];

#define SRV_N_PENDING_IOS_PER_THREAD 	OS_AIO_N_PENDING_IOS_PER_THREAD
#define SRV_MAX_N_PENDING_SYNC_IOS	100

#define SRV_MAX_N_OPEN_FILES		25

#define SRV_LOG_SPACE_FIRST_ID		1000000000

/************************************************************************
I/o-handler thread function. */
static

#ifndef __WIN__
void*
#else
ulint
#endif
io_handler_thread(
/*==============*/
	void*	arg)
{
	ulint	segment;
	ulint	i;
	
	segment = *((ulint*)arg);

/*	printf("Io handler thread %lu starts\n", segment); */

	for (i = 0;; i++) {
		fil_aio_wait(segment);

		mutex_enter(&ios_mutex);
		ios++;
		mutex_exit(&ios_mutex);
	}

#ifndef __WIN__
	return(NULL);
#else
	return(0);
#endif
}

#ifdef __WIN__
#define SRV_PATH_SEPARATOR	"\\"
#else
#define SRV_PATH_SEPARATOR	"/"
#endif

/*************************************************************************
Normalizes a directory path for Windows: converts slashes to backslashes. */
static
void
srv_normalize_path_for_win(
/*=======================*/
	char*	str)	/* in/out: null-terminated character string */
{
#ifdef __WIN__
	ulint	i;

	for (i = 0; i < ut_strlen(str); i++) {

		if (str[i] == '/') {
			str[i] = '\\';
		}
	}
#endif
}
	
/*************************************************************************
Adds a slash or a backslash to the end of a string if it is missing. */
static
char*
srv_add_path_separator_if_needed(
/*=============================*/
			/* out, own: string which has the separator */
	char*	str)	/* in: null-terminated character string */
{
	char*	out_str;

	if (ut_strlen(str) == 0) {
		out_str = ut_malloc(2);
		sprintf(out_str, "%s", SRV_PATH_SEPARATOR);

		return(out_str);
	}

	if (str[ut_strlen(str) - 1] == SRV_PATH_SEPARATOR[0]) {
		out_str = ut_malloc(ut_strlen(str) + 1);
		
		sprintf(out_str, "%s", str);

		return(out_str);
	}
		
	out_str = ut_malloc(ut_strlen(str) + 2);
		
	sprintf(out_str, "%s%s", str, SRV_PATH_SEPARATOR);

	return(out_str);
}

/*************************************************************************
Creates or opens the log files. */
static
ulint
open_or_create_log_file(
/*====================*/
					/* out: DB_SUCCESS or error code */
	ibool	create_new_db,		/* in: TRUE if we should create a
					new database */
	ibool*	log_file_created,	/* out: TRUE if new log file
					created */
	ulint	k,			/* in: log group number */
	ulint	i)			/* in: log file number in group */
{
	ibool	ret;
	ulint	arch_space_id;
	ulint	size;
	ulint	size_high;
	char	name[10000];

	UT_NOT_USED(create_new_db);

	*log_file_created = FALSE;

	srv_normalize_path_for_win(srv_log_group_home_dirs[k]);
	srv_log_group_home_dirs[k] = srv_add_path_separator_if_needed(
						srv_log_group_home_dirs[k]);

	sprintf(name, "%s%s%lu", srv_log_group_home_dirs[k], "ib_logfile", i);

	files[i] = os_file_create(name, OS_FILE_CREATE, OS_FILE_NORMAL, &ret);

	if (ret == FALSE) {
		if (os_file_get_last_error() != OS_FILE_ALREADY_EXISTS) {
			fprintf(stderr,
			"Innobase: Error in creating or opening %s\n", name);
				
			return(DB_ERROR);
		}

		files[i] = os_file_create(
					name, OS_FILE_OPEN, OS_FILE_AIO, &ret);
		if (!ret) {
			fprintf(stderr,
			"Innobase: Error in opening %s\n", name);
				
			return(DB_ERROR);
		}

		ret = os_file_get_size(files[i], &size, &size_high);
		ut_a(ret);
		
		if (size != UNIV_PAGE_SIZE * srv_log_file_size
							|| size_high != 0) {
			fprintf(stderr,
			"Innobase: Error: log file %s is of different size\n"
			"Innobase: than specified in the .cnf file!\n", name);
				
			return(DB_ERROR);
		}					
	} else {
		*log_file_created = TRUE;
					
		fprintf(stderr,
		"Innobase: Log file %s did not exist: new to be created\n",
									name);
		printf("Innobase: Setting log file %s size to %lu\n",
			             name, UNIV_PAGE_SIZE * srv_log_file_size);

		ret = os_file_set_size(name, files[i],
					UNIV_PAGE_SIZE * srv_log_file_size, 0);
		if (!ret) {
			fprintf(stderr,
		"Innobase: Error in creating %s: probably out of disk space\n",
			name);

			return(DB_ERROR);
		}
	}

	ret = os_file_close(files[i]);
	ut_a(ret);

	if (i == 0) {
		/* Create in memory the file space object
		which is for this log group */
				
		fil_space_create(name,
		2 * k + SRV_LOG_SPACE_FIRST_ID, FIL_LOG);
	}

	ut_a(fil_validate());

	fil_node_create(name, srv_log_file_size,
					2 * k + SRV_LOG_SPACE_FIRST_ID);

	/* If this is the first log group, create the file space object
	for archived logs */

	if (k == 0 && i == 0) {
		arch_space_id = 2 * k + 1 + SRV_LOG_SPACE_FIRST_ID;

	    	fil_space_create("arch_log_space", arch_space_id,
								FIL_LOG);
	} else {
		arch_space_id = ULINT_UNDEFINED;
	}

	if (i == 0) {
		log_group_init(k, srv_n_log_files,
				srv_log_file_size * UNIV_PAGE_SIZE,
				2 * k + SRV_LOG_SPACE_FIRST_ID,
				arch_space_id);
	}

	return(DB_SUCCESS);
}

/*************************************************************************
Creates or opens database data files. */
static
ulint
open_or_create_data_files(
/*======================*/
				/* out: DB_SUCCESS or error code */
	ibool*	create_new_db,	/* out: TRUE if new database should be
								created */
	dulint*	min_flushed_lsn,/* out: min of flushed lsn values in data
				files */
	ulint*	min_arch_log_no,/* out: min of archived log numbers in data
				files */
	dulint*	max_flushed_lsn,/* out: */
	ulint*	max_arch_log_no,/* out: */
	ulint*	sum_of_new_sizes)/* out: sum of sizes of the new files added */
{
	ibool	ret;
	ulint	i;
	ibool	one_opened	= FALSE;
	ibool	one_created	= FALSE;
	ulint	size;
	ulint	size_high;
	char	name[10000];

	ut_a(srv_n_data_files < 1000);

	*sum_of_new_sizes = 0;
	
	*create_new_db = FALSE;

	srv_normalize_path_for_win(srv_data_home);
	srv_data_home = srv_add_path_separator_if_needed(srv_data_home);

	for (i = 0; i < srv_n_data_files; i++) {
		srv_normalize_path_for_win(srv_data_file_names[i]);

		sprintf(name, "%s%s", srv_data_home, srv_data_file_names[i]);
	
		files[i] = os_file_create(name, OS_FILE_CREATE,
						OS_FILE_NORMAL, &ret);
		if (ret == FALSE) {
			if (os_file_get_last_error() !=
						OS_FILE_ALREADY_EXISTS) {
				fprintf(stderr,
				"Innobase: Error in creating or opening %s\n",
				name);

				return(DB_ERROR);
			}

			if (one_created) {
				fprintf(stderr,
	"Innobase: Error: data files can only be added at the end\n");
				fprintf(stderr,
	"Innobase: of a tablespace, but data file %s existed beforehand.\n",
				name);
				return(DB_ERROR);
			}
				
			files[i] = os_file_create(
				name, OS_FILE_OPEN, OS_FILE_NORMAL, &ret);

			if (!ret) {
				fprintf(stderr,
				"Innobase: Error in opening %s\n", name);

				return(DB_ERROR);
			}

			ret = os_file_get_size(files[i], &size, &size_high);
			ut_a(ret);
		
			if (size != UNIV_PAGE_SIZE * srv_data_file_sizes[i]
		    					|| size_high != 0) {
				fprintf(stderr,
			"Innobase: Error: data file %s is of different size\n"
			"Innobase: than specified in the .cnf file!\n", name);
				
				return(DB_ERROR);
			}

			fil_read_flushed_lsn_and_arch_log_no(files[i],
					one_opened,
					min_flushed_lsn, min_arch_log_no,
					max_flushed_lsn, max_arch_log_no);
			one_opened = TRUE;
		} else {
			one_created = TRUE;

			if (i > 0) {
				fprintf(stderr, 
	"Innobase: Data file %s did not exist: new to be created\n", name);
			} else {
				fprintf(stderr, 
 		"Innobase: The first specified data file %s did not exist:\n"
		"Innobase: a new database to be created!\n", name);
				*create_new_db = TRUE;
			}
			
			printf("Innobase: Setting file %s size to %lu\n",
			       name, UNIV_PAGE_SIZE * srv_data_file_sizes[i]);

			printf(
	    "Innobase: Database physically writes the file full: wait...\n");

			ret = os_file_set_size(name, files[i],
				UNIV_PAGE_SIZE * srv_data_file_sizes[i], 0);

			if (!ret) {
				fprintf(stderr, 
	"Innobase: Error in creating %s: probably out of disk space\n", name);

				return(DB_ERROR);
			}

			*sum_of_new_sizes = *sum_of_new_sizes
						+ srv_data_file_sizes[i];
		}

		ret = os_file_close(files[i]);
		ut_a(ret);

		if (i == 0) {
			fil_space_create(name, 0, FIL_TABLESPACE);
		}

		ut_a(fil_validate());

		fil_node_create(name, srv_data_file_sizes[i], 0);
	}

	ios = 0;

	mutex_create(&ios_mutex);
	mutex_set_level(&ios_mutex, SYNC_NO_ORDER_CHECK);

	return(DB_SUCCESS);
}

/*********************************************************************
This thread is used to measure contention of latches. */
static
ulint
test_measure_cont(
/*==============*/
	void*	arg)
{
	ulint	i, j;
	ulint	pcount, kcount, s_scount, s_xcount, s_mcount, lcount;

	UT_NOT_USED(arg);

	fprintf(stderr, "Starting contention measurement\n");
	
	for (i = 0; i < 1000; i++) {

		pcount = 0;
		kcount = 0;
		s_scount = 0;
		s_xcount = 0;
		s_mcount = 0;
		lcount = 0;

		for (j = 0; j < 100; j++) {

		    if (srv_measure_by_spin) {
		    	ut_delay(ut_rnd_interval(0, 20000));
		    } else {
		    	os_thread_sleep(20000);
		    }

		    if (kernel_mutex.lock_word) {
			kcount++;
		    }

		    if (buf_pool->mutex.lock_word) {
		    	pcount++;
		    }

		    if (log_sys->mutex.lock_word) {
		    	lcount++;
		    }

		    if (btr_search_latch.reader_count) {
		    	s_scount++;
		    }

		    if (btr_search_latch.writer != RW_LOCK_NOT_LOCKED) {
		    	s_xcount++;
		    }

		    if (btr_search_latch.mutex.lock_word) {
		    	s_mcount++;
		    }
		}

		fprintf(stderr, 
	"Mutex res. l %lu, p %lu, k %lu s x %lu s s %lu s mut %lu of %lu\n",
		lcount, pcount, kcount, s_xcount, s_scount, s_mcount, j);

		sync_print_wait_info();

		fprintf(stderr, 
    "log i/o %lu n non sea %lu n succ %lu n h fail %lu\n",
			log_sys->n_log_ios, btr_cur_n_non_sea,
			btr_search_n_succ, btr_search_n_hash_fail);
	}

	return(0);
}

/********************************************************************
Starts Innobase and creates a new database if database files
are not found and the user wants. Server parameters are
read from a file of name "srv_init" in the ib_home directory. */

int
innobase_start_or_create_for_mysql(void)
/*====================================*/
				/* out: DB_SUCCESS or error code */
{
	ulint	i;
	ulint	k;
	ulint	err;
	ibool	create_new_db;
	ibool	log_file_created;
	ibool	log_created	= FALSE;
	ibool	log_opened	= FALSE;
	dulint	min_flushed_lsn;
	dulint	max_flushed_lsn;
	ulint	min_arch_log_no;
	ulint	max_arch_log_no;
	ibool	start_archive;
	ulint   sum_of_new_sizes;
	mtr_t   mtr;

	log_do_write = TRUE;
/*	yydebug = TRUE; */

	srv_is_being_started = TRUE;

	os_aio_use_native_aio = srv_use_native_aio;

	err = srv_boot();

	if (err != DB_SUCCESS) {

		return((int) err);
	}

#if !(defined(WIN_ASYNC_IO) || defined(POSIX_ASYNC_IO))
	/* In simulated aio we currently have use only for 4 threads */

	os_aio_use_native_aio = FALSE;

	srv_n_file_io_threads = 4;
#endif

#ifdef WIN_ASYNC_IO
	/* On NT always use aio */
	os_aio_use_native_aio = TRUE;
#endif

	if (!os_aio_use_native_aio) {
		os_aio_init(4 * SRV_N_PENDING_IOS_PER_THREAD
						* srv_n_file_io_threads,
					srv_n_file_io_threads,
					SRV_MAX_N_PENDING_SYNC_IOS);
	} else {
		os_aio_init(SRV_N_PENDING_IOS_PER_THREAD
						* srv_n_file_io_threads,
					srv_n_file_io_threads,
					SRV_MAX_N_PENDING_SYNC_IOS);
	}
	
	fil_init(SRV_MAX_N_OPEN_FILES);

	buf_pool_init(srv_pool_size, srv_pool_size);

	fsp_init();
	log_init();
	
	lock_sys_create(srv_lock_table_size);

#ifdef POSIX_ASYNC_IO
	if (os_aio_use_native_aio) {
		/* There is only one thread per async io array:
		one for ibuf i/o, one for log i/o, one for ordinary reads,
		one for ordinary writes; we need only 4 i/o threads */

		srv_n_file_io_threads = 4;
	}
#endif
	/* Create i/o-handler threads: */

	for (i = 0; i < srv_n_file_io_threads; i++) {
		n[i] = i;

		os_thread_create(io_handler_thread, n + i, thread_ids + i);
    	}

	if (0 != ut_strcmp(srv_log_group_home_dirs[0], srv_arch_dir)) {
		fprintf(stderr,
	"InnoDB: Error: you must set the log group home dir in my.cnf the\n"
	"InnoDB: same as log arch dir.\n");

		return(DB_ERROR);
	}

	err = open_or_create_data_files(&create_new_db,
					&min_flushed_lsn, &min_arch_log_no,
					&max_flushed_lsn, &max_arch_log_no,
					&sum_of_new_sizes);
	if (err != DB_SUCCESS) {

	        fprintf(stderr, "Innobase: Could not open data files\n");

		return((int) err);
	}

	srv_normalize_path_for_win(srv_arch_dir);
	srv_arch_dir = srv_add_path_separator_if_needed(srv_arch_dir);

	for (k = 0; k < srv_n_log_groups; k++) {

		for (i = 0; i < srv_n_log_files; i++) {

			err = open_or_create_log_file(create_new_db,
						&log_file_created, k, i);
			if (err != DB_SUCCESS) {

				return((int) err);
			}

			if (log_file_created) {
				log_created = TRUE;
			} else {
				log_opened = TRUE;
			}

			if ((log_opened && create_new_db)
			    		|| (log_opened && log_created)) {
				fprintf(stderr, 
	"Innobase: Error: all log files must be created at the same time.\n"
	"Innobase: If you want bigger or smaller log files,\n"
	"Innobase: shut down the database and make sure there\n"
	"Innobase: were no errors in shutdown.\n"
	"Innobase: Then delete the existing log files. Edit the .cnf file\n"
	"Innobase: and start the database again.\n");

				return(DB_ERROR);
			}
			
		}
	}

	if (log_created && !create_new_db && !srv_archive_recovery) {

		if (ut_dulint_cmp(max_flushed_lsn, min_flushed_lsn) != 0
				|| max_arch_log_no != min_arch_log_no) {
			fprintf(stderr, 
		"Innobase: Cannot initialize created log files because\n"
		"Innobase: data files were not in sync with each other\n"
		"Innobase: or the data files are corrupt./n");

			return(DB_ERROR);
		}

		if (ut_dulint_cmp(max_flushed_lsn, ut_dulint_create(0, 1000))
		    < 0) {
		    	fprintf(stderr,
		"Innobase: Cannot initialize created log files because\n"
		"Innobase: data files are corrupt, or new data files were\n"
		"Innobase: created when the database was started previous\n"
		"Innobase: time but the database was not shut down\n"
		"Innobase: normally after that.\n");

			return(DB_ERROR);
		}

		mutex_enter(&(log_sys->mutex));

		recv_reset_logs(ut_dulint_align_down(max_flushed_lsn,
					OS_FILE_LOG_BLOCK_SIZE),
					max_arch_log_no + 1, TRUE);
		
		mutex_exit(&(log_sys->mutex));
	}

	sess_sys_init_at_db_start();

	if (create_new_db) {
		mtr_start(&mtr);

		fsp_header_init(0, sum_of_new_sizes, &mtr);		

		mtr_commit(&mtr);

		trx_sys_create();
		dict_create();

	} else if (srv_archive_recovery) {
		fprintf(stderr,
	"Innobase: Starting archive recovery from a backup...\n");
	
		err = recv_recovery_from_archive_start(
					min_flushed_lsn,
					srv_archive_recovery_limit_lsn,
					min_arch_log_no);
		if (err != DB_SUCCESS) {

			return(DB_ERROR);
		}

		trx_sys_init_at_db_start();
		dict_boot();
		
		recv_recovery_from_archive_finish();
	} else {
		/* We always try to do a recovery, even if the database had
		been shut down normally */
		
		err = recv_recovery_from_checkpoint_start(LOG_CHECKPOINT,
							ut_dulint_max,
							min_flushed_lsn,
							max_flushed_lsn);
		if (err != DB_SUCCESS) {

			return(DB_ERROR);
		}

		trx_sys_init_at_db_start();
		dict_boot();

		/* The following needs trx lists which are initialized in
		trx_sys_init_at_db_start */
		
		recv_recovery_from_checkpoint_finish();
	}
	
	if (!create_new_db && sum_of_new_sizes > 0) {
		/* New data file(s) were added */
		mtr_start(&mtr);

		fsp_header_inc_size(0, sum_of_new_sizes, &mtr);		

		mtr_commit(&mtr);
	}

	log_make_checkpoint_at(ut_dulint_max, TRUE);

	if (!srv_log_archive_on) {
		ut_a(DB_SUCCESS == log_archive_noarchivelog());
	} else {
		mutex_enter(&(log_sys->mutex));

		start_archive = FALSE;

		if (log_sys->archiving_state == LOG_ARCH_OFF) {
			start_archive = TRUE;
		}

		mutex_exit(&(log_sys->mutex));

		if (start_archive) {
			ut_a(DB_SUCCESS == log_archive_archivelog());
		}
	}

	if (srv_measure_contention) {
	  /* os_thread_create(&test_measure_cont, NULL, thread_ids +
                             	     SRV_MAX_N_IO_THREADS); */
	}

	/* Create the master thread which monitors the database
	server, and does purge and other utility operations */

	os_thread_create(&srv_master_thread, NULL, thread_ids + 1 +
							SRV_MAX_N_IO_THREADS);
	/* fprintf(stderr, "Max allowed record size %lu\n",
				page_get_free_space_of_empty() / 2); */

	/* Create the thread which watches the timeouts for lock waits */
	os_thread_create(&srv_lock_timeout_monitor_thread, NULL,
					thread_ids + 2 + SRV_MAX_N_IO_THREADS);	
	fprintf(stderr, "Innobase: Started\n");

	srv_was_started = TRUE;
	srv_is_being_started = FALSE;

	sync_order_checks_on = TRUE;

	/* buf_debug_prints = TRUE; */
	
	return((int) DB_SUCCESS);
}

/********************************************************************
Shuts down the Innobase database. */

int
innobase_shutdown_for_mysql(void) 
/*=============================*/
				/* out: DB_SUCCESS or error code */
{
        if (!srv_was_started) {
	  if (srv_is_being_started) {
            fprintf(stderr, 
	"Innobase: Warning: shutting down not properly started database\n");
	  }
	  return(DB_SUCCESS);
	}

	/* Flush buffer pool to disk, write the current lsn to
	the tablespace header(s), and copy all log data to archive */

	logs_empty_and_mark_files_at_shutdown();

	return((int) DB_SUCCESS);
}
