/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "my_my_global.h"

static void spawn_stern_thread(pthread_t *t);
static int act_goofy(void);
static void *be_stern(void *);

static struct {
  pthread_mutex_t lock;
  pthread_cond_t  cond;
  int msg;
} comm = { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0 };


int
main(void)
{
  pthread_t t;
  spawn_stern_thread(&t);

  while (act_goofy() != 0)
    /* do nothing */;

  pthread_exit(NULL);

  /* notreached */
  return EXIT_SUCCESS;
}

static void spawn_stern_thread(pthread_t *t)
{
  pthread_attr_t attr;

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

  if (pthread_create(t, &attr, be_stern, NULL) != 0)
    exit(EXIT_FAILURE);

  pthread_attr_destroy(&attr);
}

static int act_goofy(void)
{
  int  ret;
  char buf[30];

  fputs("Are you ready to act goofy (Y/n)? ", stdout); fflush(stdout);
  fgets(buf, sizeof(buf), stdin);
  pthread_mutex_lock(&comm.lock);
  if (buf[0] == 'y' || buf[0] == '\n') {
    fputs("** Waawlwalkwwwaa!!\n", stdout); fflush(stdout);
    ++comm.msg;
    ret = 1;
  }
  else {
    fputs("OK, I hate you.  Let me go.\n", stdout); fflush(stdout);
    comm.msg = -1;
    ret = 0;
  }
  pthread_mutex_unlock(&comm.lock);
  pthread_cond_signal(&comm.cond);

  return ret;
}

static void *be_stern(void *v __attribute__((unused)))
{
  int msg;
  for (;;) {
    pthread_mutex_lock(&comm.lock);
    while (comm.msg == 0)
      pthread_cond_wait(&comm.cond, &comm.lock);
    msg = comm.msg;
    comm.msg = 0;
    pthread_mutex_unlock(&comm.lock);

    if (msg < 0)
      break;  /* the goofy one learned a lesson! */

    fputs("I HAVE TO BE STERN WITH YOU!\n", stderr);
    fprintf(stderr, "I should give you %d lashes.\n", msg);
    sleep(msg);
  }

  fputs("You are NOTHING!\n", stderr);
  return NULL;
}
