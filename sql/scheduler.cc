/* Copyright (C) 2007 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  Implementation for the thread scheduler
*/

#ifdef USE_PRAGMA_INTERFACE
#pragma implementation
#endif

#include <mysql_priv.h>


/*
  'Dummy' functions to be used when we don't need any handling for a scheduler
  event
 */

static bool init_dummy(void) {return 0;}
static void post_kill_dummy(THD *thd) {}  
static void end_dummy(void) {}
static bool end_thread_dummy(THD *thd, bool cache_thread) { return 0; }

/*
  Initialize default scheduler with dummy functions so that setup functions
  only need to declare those that are relvant for their usage
*/

scheduler_functions::scheduler_functions()
  :init(init_dummy),
   init_new_connection_thread(init_new_connection_handler_thread),
   add_connection(0),                           // Must be defined
   post_kill_notification(post_kill_dummy),
   end_thread(end_thread_dummy), end(end_dummy)
{}


/*
  End connection, in case when we are using 'no-threads'
*/

static bool no_threads_end(THD *thd, bool put_in_cache)
{
  unlink_thd(thd);
  pthread_mutex_unlock(&LOCK_thread_count);
  return 1;                                     // Abort handle_one_connection
}


/*
  Initailize scheduler for --thread-handling=no-threads
*/

void one_thread_scheduler(scheduler_functions *func)
{
  func->max_threads= 1;
#ifndef EMBEDDED_LIBRARY
  func->add_connection= handle_connection_in_main_thread;
#endif
  func->init_new_connection_thread= init_dummy;
  func->end_thread= no_threads_end;
}


/*
  Initialize scheduler for --thread-handling=one-thread-per-connection
*/

#ifndef EMBEDDED_LIBRARY
void one_thread_per_connection_scheduler(scheduler_functions *func)
{
  func->max_threads= max_connections;
  func->add_connection= create_thread_to_handle_connection;
  func->end_thread= one_thread_per_connection_end;
}
#endif /* EMBEDDED_LIBRARY */


#if defined(HAVE_LIBEVENT) && HAVE_POOL_OF_THREADS == 1

#include "event.h"

static uint created_threads, killed_threads;
static int thd_add_pipe[2]; /*pipe to use for adding connections to libevent*/
static pthread_mutex_t LOCK_event_loop;
bool kill_pool_threads= FALSE;

static LIST *thds_need_processing;

pthread_handler_t libevent_thread_proc(void *arg);
static void libevent_abort_threads();
static bool libevent_needs_immediate_processing(THD *thd);
static void libevent_connection_close(THD *thd);
static bool libevent_should_close_connection(THD* thd);

void libevent_io_callback(int Fd, short Operation, void *ctx);
void libevent_add_thd_callback(int Fd, short Operation, void *ctx);
void libevent_kill_callback(int Fd, short Operation, void *ctx);


/*
  Create a pipe and set to non-blocking. Returns TRUE if there is an error.
*/
static bool init_pipe(int pipe_fds[])
{
  int flags;
  return pipe(pipe_fds) < 0 ||
            (flags= fcntl(pipe_fds[0], F_GETFL)) == -1 ||
            fcntl(pipe_fds[0], F_SETFL, flags | O_NONBLOCK) == -1 ||
            (flags= fcntl(pipe_fds[1], F_GETFL)) == -1 ||
            fcntl(pipe_fds[1], F_SETFL, flags | O_NONBLOCK) == -1;
}


/*
  thd_scheduler keeps the link between THD and events.
  It's embedded in the THD class.
*/

thd_scheduler::thd_scheduler()
  : logged_in(FALSE), io_event(NULL), kill_event(NULL)
{
  kill_pipe[0]= 0;
  kill_pipe[1]= 0;
}


thd_scheduler::~thd_scheduler()
{  
  if (kill_pipe[0]) close(kill_pipe[0]);
  if (kill_pipe[1]) close(kill_pipe[1]);
  my_free(io_event, MYF(MY_ALLOW_ZERO_PTR));
  my_free(kill_event, MYF(MY_ALLOW_ZERO_PTR));
}


bool thd_scheduler::init(THD *parent_thd)
{
  io_event=
    (struct event*)my_malloc(sizeof(*io_event),MYF(MY_ZEROFILL|MY_WME));
  kill_event=
    (struct event*)my_malloc(sizeof(*kill_event),MYF(MY_ZEROFILL|MY_WME));
    
  if (!io_event || !kill_event)
  {
    sql_print_error("Memory allocation error in thd_scheduler::init\n");
    return TRUE;
  }

  if (init_pipe(kill_pipe))
  {
    sql_print_error("init_pipe error in thd_scheduler::init\n");
    return TRUE;
  }
  
  event_set(io_event, parent_thd->net.vio->sd, EV_READ, 
    libevent_io_callback, (void*)parent_thd);
  event_set(kill_event, kill_pipe[0], EV_READ, 
    libevent_kill_callback, (void*)parent_thd);
  list_need_processing.data= parent_thd;
  
  return FALSE;
}

/*
  Create all threads for the thread pool

  NOTES
    After threads are created we wait until all threads has signaled that
    they have started before we return

  RETURN
    0  ok
    1  We got an error creating the thread pool
       In this case we will abort all created threads
*/
static bool libevent_init(void)
{
  uint i;
  static struct event thd_add_event;
  DBUG_ENTER("libevent_init");

  event_init();
  
  created_threads= 0;
  killed_threads= 0;
  
  pthread_mutex_init(&LOCK_event_loop, NULL);
  
    /* set up the pipe used to add new thds to the event pool */
  if (init_pipe(thd_add_pipe))
  {
    sql_print_error("init_pipe error in libevent_init\n");
    pthread_mutex_unlock(&LOCK_thread_count);
    DBUG_RETURN(1);
  }
  event_set(&thd_add_event, thd_add_pipe[0], EV_READ|EV_PERSIST,
            libevent_add_thd_callback, NULL);          
 
 if (event_add(&thd_add_event, NULL))
 {
   sql_print_error("thd_add_event event_add error in libevent_init\n");
 }
  
  /* Set up the thread pool */
  created_threads= killed_threads= 0;
  pthread_mutex_lock(&LOCK_thread_count);

  for (i= 0; i < thread_pool_size; i++)
  {
    pthread_t thread;
    int error;
    if ((error= pthread_create(&thread, &connection_attrib,
                              libevent_thread_proc, 0)))
    {
      sql_print_error("Can't create completion port thread (error %d)",
                      error);
      pthread_mutex_unlock(&LOCK_thread_count);
      libevent_abort_threads();                       // Cleanup
      DBUG_RETURN(TRUE);
    }
  }

  /* Wait until all threads are created */
  while (created_threads != thread_pool_size)
    pthread_cond_wait(&COND_thread_count,&LOCK_thread_count);
  pthread_mutex_unlock(&LOCK_thread_count);
  
  DBUG_PRINT("info", ("%u threads created", (uint) thread_pool_size));
  DBUG_RETURN(FALSE);
}


/*
  This is called when data is ready on the socket.
  
  NOTES
    This is only called by the thread that owns LOCK_event_loop.
  
    We add the thd that got the data to thds_need_processing, and 
    cause the libevent event_loop() to terminate. Then this same thread will
    return from event_loop and pick the thd value back up for processing.
*/
void libevent_io_callback(int, short, void *ctx)
{    
  safe_mutex_assert_owner(&LOCK_event_loop);
  THD *thd= (THD*)ctx;
  event_del(thd->scheduler.kill_event);
  thds_need_processing=
      list_add(thds_need_processing, &thd->scheduler.list_need_processing);
}

/*
  This is called when we want the thread to be be killed.
  
  NOTES
    This is only called by the thread that owns LOCK_event_loop.
*/
void libevent_kill_callback(int Fd, short, void *ctx)
{    
  safe_mutex_assert_owner(&LOCK_event_loop);
  THD *thd= (THD*)ctx;
  char c;
  read(Fd, &c, sizeof(c));
  event_del(thd->scheduler.io_event);
  thds_need_processing=
      list_add(thds_need_processing, &thd->scheduler.list_need_processing);
}


/*
  This is used to add connections to the pool. This callback is invoked from the
  libevent event_loop() call whenever the thd_add_pipe[1] pipe is written too.
  
  NOTES
    This is only called by the thread that owns LOCK_event_loop.
*/
void libevent_add_thd_callback(int Fd, short, void *)
{ 
  safe_mutex_assert_owner(&LOCK_event_loop);
  
  THD *thd;
  if (read(Fd, &thd, sizeof(THD*)) != sizeof(THD*))
  {
    sql_print_error("Error reading from pipe in libevent_add_thd_callback\n");
    return;
  }
  
  /* thd can be NULL during shutdown, to "wake-up" event_loop() */
  if (thd == NULL)
    return;
  
  if (!thd->scheduler.logged_in || libevent_should_close_connection(thd))
  {
    /*
      Add thd to thds_need_processing list. If it needs closing we'll close it
      outside of event_loop().
    */
    thds_need_processing=
        list_add(thds_need_processing, &thd->scheduler.list_need_processing);
  }
  else
  {
    /* add the events to libevent */
    if (event_add(thd->scheduler.io_event, NULL))
    {
      sql_print_error("io_event event_add error in libevent_add_thd_callback\n");
      libevent_connection_close(thd);
      return;
    }
    if (event_add(thd->scheduler.kill_event, NULL))
    {
      sql_print_error("kill_event event_add error in libevent_add_thd_callback\n");
      event_del(thd->scheduler.io_event);
      libevent_connection_close(thd);
      return;
    }
  }
}


/*
  Notify the thread pool about a new connection

  NOTES
    LOCK_thread_count is locked on entry. This function MUST unlock it!
*/
static void libevent_add_new_connection(THD *thd)
{
  DBUG_ENTER("libevent_add_new_connection");
  DBUG_PRINT("enter", ("thd: 0x%lx  thread_id: %lu",
                       (long) thd, thd->thread_id));
  
  if (thd->scheduler.init(thd))
  {
      sql_print_error("Scheduler init error in libevent_add_new_connection\n");
      goto err;
  }
  
  threads.append(thd);
  
  /* causes the event_loop to invoke libevent_add_thd_callback */
  if (write(thd_add_pipe[1], &thd, sizeof(THD*)) != sizeof(THD*))
  {  
    sql_print_error("Pipe error in libevent_add_new_connection\n");
    goto err;
  }
  
  pthread_mutex_unlock(&LOCK_thread_count);
  DBUG_VOID_RETURN;
  
err:
  pthread_mutex_unlock(&LOCK_thread_count);
  libevent_connection_close(thd);
  DBUG_VOID_RETURN;
}


/*
  Signal a connection it's time to die

  NOTES
    On entry we have a lock on LOCK_thread_cont
*/

static void libevent_post_kill_notification(THD *thd)
{
  char c= 0;
  if(write(thd->scheduler.kill_pipe[1], &c, sizeof(c)) != sizeof(c))
  {  
    sql_print_error("Pipe error in libevent_post_kill_notification\n");
  }
}


/*
  Close and delete a connection.
*/
static void libevent_connection_close(THD *thd)
{
  DBUG_ENTER("libevent_connection_close");
  DBUG_PRINT("enter", ("thd: 0x%lx", (long) thd));

  thd->killed= THD::KILL_CONNECTION;          // Avoid error messages

  if (thd->net.vio->sd >= 0)                   // not already closed
  {
    end_connection(thd);
    close_connection(thd, 0, 1);
  }
  
  no_threads_end(thd, 0); /* deletes thd */

  DBUG_VOID_RETURN;
}

/*
  Returns true if we should close and delete a THD connection.
*/
static bool libevent_should_close_connection(THD* thd)
{
  return thd->net.error ||
      thd->net.vio == 0 ||
      thd->killed == THD::KILL_CONNECTION;
}

pthread_handler_t libevent_thread_proc(void *arg)
{
  if (init_new_connection_handler_thread())
  {
    my_thread_global_end();
    sql_print_error("libevent_thread_proc: my_thread_init() failed\n");
    exit(1);
  }
  DBUG_ENTER("libevent_thread_proc");

  /*
    Signal libevent_init() when all threads has been created and are ready to
    receive events.
  */
  (void) pthread_mutex_lock(&LOCK_thread_count);
  created_threads++;
  if (created_threads == thread_pool_size)
    (void) pthread_cond_signal(&COND_thread_count);
  (void) pthread_mutex_unlock(&LOCK_thread_count);
  
  for (;;)
  {
    THD *thd= NULL;
    (void) pthread_mutex_lock(&LOCK_event_loop);
    
    /* get thd(s) to process */
    while (!thds_need_processing)
    {
      if (kill_pool_threads)
      {
        /* the flag that we should die has been set */
        (void) pthread_mutex_unlock(&LOCK_event_loop);
        goto thread_exit;
      }
      event_loop(EVLOOP_ONCE);
    }
    
    /* pop the first thd off the list */
    thd= (THD*)thds_need_processing->data;
    thds_need_processing= 
        list_delete(thds_need_processing, thds_need_processing);
    
    (void) pthread_mutex_unlock(&LOCK_event_loop);
    
    /* now we process the connection (thd) */
    
    /* set up the thd<->thread links. */
    thd->thread_stack= (char*) &thd;
    
    if (libevent_should_close_connection(thd) ||
        setup_connection_thread_globals(thd))
    {
      libevent_connection_close(thd);
      continue;
    }
    
    my_errno= 0;
    thd->mysys_var->abort= 0;

    /* is the connection logged in yet? */
    if (!thd->scheduler.logged_in)
    {
      DBUG_PRINT("info", ("init new connection.  sd: %d",
                          thd->net.vio->sd));
      if (login_connection(thd))
      {
        /* Failed to log in */
        libevent_connection_close(thd);
        continue;
      }
      else
      {
        /* login successful */
        thd->scheduler.logged_in= TRUE;
        prepare_new_connection_state(thd);
        if (!libevent_needs_immediate_processing(thd))
          continue; /* New connection is now waiting for data in libevent*/
      }
    }

    do
    {
      /* Process a query */
      thd->net.no_send_error= 0;
      if (do_command(thd))
      {
        libevent_connection_close(thd);
        break;
      }
    } while (libevent_needs_immediate_processing(thd));
  }
  
thread_exit:
  DBUG_PRINT("exit", ("ending thread"));
  (void) pthread_mutex_lock(&LOCK_thread_count);
  killed_threads++;
  pthread_cond_broadcast(&COND_thread_count);
  (void) pthread_mutex_unlock(&LOCK_thread_count);
  my_thread_end();
  pthread_exit(0);
  DBUG_RETURN(0);                               /* purify: deadcode */
}

/*
  Returns TRUE if the connection needs immediate processing and FALSE if 
  instead it's queued for libevent processing or closed,
*/
static bool libevent_needs_immediate_processing(THD *thd)
{
  if (libevent_should_close_connection(thd))
  {
    libevent_connection_close(thd);
    return FALSE;
  }
  /*
    If more data in the socket buffer, return TRUE to process another command.

    Note: we cannot add for event processing because the whole request might
    already be buffered and we wouldn't receive an event.
  */
  if (thd->net.vio == 0 || thd->net.vio->read_pos < thd->net.vio->read_end)
    return TRUE;
    
  /*
    Send to be queued for libevent.
    The mutex is to protect thd_add_pipe,
    which can otherwise be simutaneously written-to by multiple threads and
    from add_connection (which also holds LOCK_thread_count when writing).
  */  
  pthread_mutex_lock(&LOCK_thread_count);
  bool added_to_pipe= write(thd_add_pipe[1], &thd, sizeof(THD*)) == sizeof(THD*);
  pthread_mutex_unlock(&LOCK_thread_count);
  if (!added_to_pipe)
  {
    /* Things must be really backed up. Close the connection. */
    sql_print_error("Pipe error in libevent_needs_immediate_processing\n");
    libevent_connection_close(thd);
  }
  return FALSE;
}

/*
  Wait until all pool threads have been deleted for clean shutdown
*/
static void libevent_abort_threads()
{
  DBUG_ENTER("libevent_abort_threads");
  DBUG_PRINT("enter", ("created_threads: %d  killed_threads: %u",
                       created_threads, killed_threads));
  
  
  (void) pthread_mutex_lock(&LOCK_thread_count);
  
  kill_pool_threads= TRUE;
  while (killed_threads != created_threads)
  {
    /* write a null to wake up the event loop */
    void *p=NULL;
    write(thd_add_pipe[1], &p, sizeof(p));
      
    pthread_cond_wait(&COND_thread_count, &LOCK_thread_count);
  }
  (void) pthread_mutex_unlock(&LOCK_thread_count);
  
  close(thd_add_pipe[0]);
  close(thd_add_pipe[1]);
  (void) pthread_mutex_destroy(&LOCK_event_loop);
  DBUG_VOID_RETURN;
}


void pool_of_threads_scheduler(scheduler_functions* func)
{
  func->max_threads= thread_pool_size;
  func->init= libevent_init;
  func->end=  libevent_abort_threads;
  func->post_kill_notification= libevent_post_kill_notification;
  func->add_connection= libevent_add_new_connection;
}

#endif
