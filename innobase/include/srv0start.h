/******************************************************
Starts the Innobase database server

(c) 1995-2000 Innobase Oy

Created 10/10/1995 Heikki Tuuri
*******************************************************/


#ifndef srv0start_h
#define srv0start_h

#include "univ.i"

/********************************************************************
Starts Innobase and creates a new database if database files
are not found and the user wants. Server parameters are
read from a file of name "srv_init" in the ib_home directory. */

int
innobase_start_or_create_for_mysql(void);
/*====================================*/
				/* out: DB_SUCCESS or error code */
/********************************************************************
Shuts down the Innobase database. */

int
innobase_shutdown_for_mysql(void);
/*=============================*/
				/* out: DB_SUCCESS or error code */

extern	ibool	srv_startup_is_before_trx_rollback_phase;
extern	ibool	srv_is_being_shut_down;

/* At a shutdown the value first climbs from 0 to SRV_SHUTDOWN_CLEANUP
and then to SRV_SHUTDOWN_LAST_PHASE */

extern 	ulint	srv_shutdown_state;

#define SRV_SHUTDOWN_CLEANUP	1
#define SRV_SHUTDOWN_LAST_PHASE	2

#endif
