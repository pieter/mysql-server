/* Copyright (C) 2000-2003 MySQL AB

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"

#ifdef WITH_NDBCLUSTER_STORAGE_ENGINE
#include <ndbapi/NdbApi.hpp>
#include "ha_ndbcluster_connection.h"

/* options from from mysqld.cc */
extern my_bool opt_ndb_optimized_node_selection;
extern const char *opt_ndbcluster_connectstring;
extern ulong opt_ndb_wait_connected;
extern ulong opt_ndb_cluster_connection_pool;

Ndb* g_ndb= NULL;
Ndb_cluster_connection* g_ndb_cluster_connection= NULL;
static Ndb_cluster_connection **g_ndb_cluster_connection_pool= NULL;
static ulong g_ndb_cluster_connection_pool_alloc= 0;
static ulong g_ndb_cluster_connection_pool_pos= 0;
static pthread_mutex_t g_ndb_cluster_connection_pool_mutex;

int ndbcluster_connect(int (*connect_callback)(void))
{
  int res;
  DBUG_ENTER("ndbcluster_connect");
  // Set connectstring if specified
  if (opt_ndbcluster_connectstring != 0)
    DBUG_PRINT("connectstring", ("%s", opt_ndbcluster_connectstring));
  if ((g_ndb_cluster_connection=
       new Ndb_cluster_connection(opt_ndbcluster_connectstring)) == 0)
  {
    sql_print_error("NDB: failed to allocate global "
                    "ndb cluster connection object");
    DBUG_PRINT("error",("Ndb_cluster_connection(%s)",
                        opt_ndbcluster_connectstring));
    my_errno= HA_ERR_OUT_OF_MEM;
    goto ndbcluster_connect_error;
  }
  {
    char buf[128];
    my_snprintf(buf, sizeof(buf), "mysqld --server-id=%lu", server_id);
    g_ndb_cluster_connection->set_name(buf);
  }
  g_ndb_cluster_connection->set_optimized_node_selection
    (opt_ndb_optimized_node_selection);

  // Create a Ndb object to open the connection  to NDB
  if ( (g_ndb= new Ndb(g_ndb_cluster_connection, "sys")) == 0 )
  {
    sql_print_error("NDB: failed to allocate global ndb object");
    DBUG_PRINT("error", ("failed to create global ndb object"));
    my_errno= HA_ERR_OUT_OF_MEM;
    goto ndbcluster_connect_error;
  }
  if (g_ndb->init() != 0)
  {
    DBUG_PRINT("error", ("%d  message: %s",
                         g_ndb->getNdbError().code,
                         g_ndb->getNdbError().message));
    goto ndbcluster_connect_error;
  }

  /* Connect to management server */

  struct timeval end_time;
  gettimeofday(&end_time, 0);
  end_time.tv_sec+= opt_ndb_wait_connected;

  while ((res= g_ndb_cluster_connection->connect(0,0,0)) == 1)
  {
    struct timeval now_time;
    gettimeofday(&now_time, 0);
    if (now_time.tv_sec > end_time.tv_sec ||
        (now_time.tv_sec == end_time.tv_sec &&
         now_time.tv_usec >= end_time.tv_usec))
      break;
    sleep(1);
  }

  {
    g_ndb_cluster_connection_pool_alloc= opt_ndb_cluster_connection_pool;
    g_ndb_cluster_connection_pool= (Ndb_cluster_connection**)
      my_malloc(g_ndb_cluster_connection_pool_alloc *
                sizeof(Ndb_cluster_connection*),
                MYF(MY_WME | MY_ZEROFILL));
    pthread_mutex_init(&g_ndb_cluster_connection_pool_mutex,
                       MY_MUTEX_INIT_FAST);
    g_ndb_cluster_connection_pool[0]= g_ndb_cluster_connection;
    for (unsigned i= 1; i < g_ndb_cluster_connection_pool_alloc; i++)
    {
      if ((g_ndb_cluster_connection_pool[i]=
           new Ndb_cluster_connection(opt_ndbcluster_connectstring,
                                      g_ndb_cluster_connection)) == 0)
      {
        sql_print_error("NDB[%u]: failed to allocate cluster connect object",
                        i);
        DBUG_PRINT("error",("Ndb_cluster_connection[%u](%s)",
                            i, opt_ndbcluster_connectstring));
        goto ndbcluster_connect_error;
      }
      {
        char buf[128];
        my_snprintf(buf, sizeof(buf), "mysqld --server-id=%lu (connection %u)",
                    server_id, i+1);
        g_ndb_cluster_connection_pool[i]->set_name(buf);
      }
      g_ndb_cluster_connection_pool[i]->set_optimized_node_selection
        (opt_ndb_optimized_node_selection);
    }
  }

  if (res == 0)
  {
    connect_callback();
    for (unsigned i= 0; i < g_ndb_cluster_connection_pool_alloc; i++)
    {
      if (g_ndb_cluster_connection_pool[i]->node_id() == 0)
      {
        // not connected to mgmd yet, try again
        g_ndb_cluster_connection_pool[i]->connect(0,0,0);
        if (g_ndb_cluster_connection_pool[i]->node_id() == 0)
        {
          sql_print_warning("NDB[%u]: starting connect thread", i);
          g_ndb_cluster_connection_pool[i]->start_connect_thread();
          continue;
        }
      }
      DBUG_PRINT("info",
                 ("NDBCLUSTER storage engine (%u) at %s on port %d", i,
                  g_ndb_cluster_connection_pool[i]->get_connected_host(),
                  g_ndb_cluster_connection_pool[i]->get_connected_port()));

      struct timeval now_time;
      gettimeofday(&now_time, 0);
      ulong wait_until_ready_time = (end_time.tv_sec > now_time.tv_sec) ?
        end_time.tv_sec - now_time.tv_sec : 1;
      res= g_ndb_cluster_connection_pool[i]->
        wait_until_ready(wait_until_ready_time,3);
      if (res == 0)
      {
        sql_print_information("NDB[%u]: all storage nodes connected", i);
      }
      else if (res > 0)
      {
        sql_print_information("NDB[%u]: some storage nodes connected", i);
      }
      else if (res < 0)
      {
        sql_print_information("NDB[%u]: no storage nodes connected (timed out)", i);
      }
    }
  }
  else if (res == 1)
  {
    for (unsigned i= 0; i < g_ndb_cluster_connection_pool_alloc; i++)
    {
      if (g_ndb_cluster_connection_pool[i]->
          start_connect_thread(i == 0 ? connect_callback :  NULL))
      {
        sql_print_error("NDB[%u]: failed to start connect thread", i);
        DBUG_PRINT("error", ("g_ndb_cluster_connection->start_connect_thread()"));
        goto ndbcluster_connect_error;
      }
    }
#ifndef DBUG_OFF
    {
      char buf[1024];
      DBUG_PRINT("info",
                 ("NDBCLUSTER storage engine not started, "
                  "will connect using %s",
                  g_ndb_cluster_connection->
                  get_connectstring(buf,sizeof(buf))));
    }
#endif
  }
  else
  {
    DBUG_ASSERT(res == -1);
    DBUG_PRINT("error", ("permanent error"));
    goto ndbcluster_connect_error;
  }
  DBUG_RETURN(0);
ndbcluster_connect_error:
  DBUG_RETURN(-1);
}

int ndbcluster_disconnect()
{
  DBUG_ENTER("ndbcluster_disconnect");
  if (g_ndb)
    delete g_ndb;
  g_ndb= NULL;
  {
    if (g_ndb_cluster_connection_pool)
    {
      /* first in pool is the main one, wait with release */
      for (unsigned i= 1; i < g_ndb_cluster_connection_pool_alloc; i++)
      {
        if (g_ndb_cluster_connection_pool[i])
          delete g_ndb_cluster_connection_pool[i];
      }
      my_free((uchar*) g_ndb_cluster_connection_pool, MYF(MY_ALLOW_ZERO_PTR));
      pthread_mutex_destroy(&g_ndb_cluster_connection_pool_mutex);
      g_ndb_cluster_connection_pool= 0;
    }
    g_ndb_cluster_connection_pool_alloc= 0;
    g_ndb_cluster_connection_pool_pos= 0;
  }
  if (g_ndb_cluster_connection)
    delete g_ndb_cluster_connection;
  g_ndb_cluster_connection= NULL;
  DBUG_RETURN(0);
}

Ndb_cluster_connection *ndb_get_cluster_connection()
{
  pthread_mutex_lock(&g_ndb_cluster_connection_pool_mutex);
  Ndb_cluster_connection *connection=
    g_ndb_cluster_connection_pool[g_ndb_cluster_connection_pool_pos];
  g_ndb_cluster_connection_pool_pos++;
  if (g_ndb_cluster_connection_pool_pos ==
      g_ndb_cluster_connection_pool_alloc)
    g_ndb_cluster_connection_pool_pos= 0;
  pthread_mutex_unlock(&g_ndb_cluster_connection_pool_mutex);
  return connection;
}

ulonglong ndb_get_latest_trans_gci()
{
  unsigned i;
  ulonglong val= *g_ndb_cluster_connection->get_latest_trans_gci();
  for (i= 1; i < g_ndb_cluster_connection_pool_alloc; i++)
  {
    ulonglong tmp= *g_ndb_cluster_connection_pool[i]->get_latest_trans_gci();
    if (tmp > val)
      val= tmp;
  }
  return val;
}

void ndb_set_latest_trans_gci(ulonglong val)
{
  unsigned i;
  for (i= 0; i < g_ndb_cluster_connection_pool_alloc; i++)
  {
    *g_ndb_cluster_connection_pool[i]->get_latest_trans_gci()= val;
  }
}

int ndb_has_node_id(uint id)
{
  unsigned i;
  for (i= 0; i < g_ndb_cluster_connection_pool_alloc; i++)
  {
    if (id == g_ndb_cluster_connection_pool[i]->node_id())
      return 1;
  }
  return 0;
}

#endif /* WITH_NDBCLUSTER_STORAGE_ENGINE */
