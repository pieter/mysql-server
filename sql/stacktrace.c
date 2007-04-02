/* Copyright (C) 2000 MySQL AB

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

#include <my_global.h>
#include "stacktrace.h"
#include <signal.h>
#include <my_pthread.h>

#ifdef HAVE_STACKTRACE
#include <unistd.h>
#include <strings.h>

#define PTR_SANE(p) ((p) && (char*)(p) >= heap_start && (char*)(p) <= heap_end)

char *heap_start;

void safe_print_str(const char* name, const char* val, int max_len)
{
  char *heap_end= (char*) sbrk(0);
  fprintf(stderr, "%s at %p ", name, val);

  if (!PTR_SANE(val))
  {
    fprintf(stderr, " is invalid pointer\n");
    return;
  }

  fprintf(stderr, "= ");
  for (; max_len && PTR_SANE(val) && *val; --max_len)
    fputc(*val++, stderr);
  fputc('\n', stderr);
}

#ifdef TARGET_OS_LINUX

#ifdef __i386__
#define SIGRETURN_FRAME_OFFSET 17
#endif

#ifdef __x86_64__
#define SIGRETURN_FRAME_OFFSET 23
#endif

#if defined(__alpha__) && defined(__GNUC__)
/*
  The only way to backtrace without a symbol table on alpha
  is to find stq fp,N(sp), and the first byte
  of the instruction opcode will give us the value of N. From this
  we can find where the old value of fp is stored
*/

#define MAX_INSTR_IN_FUNC  10000

inline uchar** find_prev_fp(uint32* pc, uchar** fp)
{
  int i;
  for (i = 0; i < MAX_INSTR_IN_FUNC; ++i,--pc)
  {
    uchar* p = (uchar*)pc;
    if (p[2] == 222 &&  p[3] == 35)
    {
      return (uchar**)((uchar*)fp - *(short int*)p);
    }
  }
  return 0;
}

inline uint32* find_prev_pc(uint32* pc, uchar** fp)
{
  int i;
  for (i = 0; i < MAX_INSTR_IN_FUNC; ++i,--pc)
  {
    char* p = (char*)pc;
    if (p[1] == 0 && p[2] == 94 &&  p[3] == -73)
    {
      uint32* prev_pc = (uint32*)*((fp+p[0]/sizeof(fp)));
      return prev_pc;
    }
  }
  return 0;
}
#endif /* defined(__alpha__) && defined(__GNUC__) */


void  print_stacktrace(gptr stack_bottom, ulong thread_stack)
{
  uchar** fp;
  uint frame_count = 0, sigreturn_frame_count;
#if defined(__alpha__) && defined(__GNUC__)
  uint32* pc;
#endif
  LINT_INIT(fp);

  fprintf(stderr,"\
Attempting backtrace. You can use the following information to find out\n\
where mysqld died. If you see no messages after this, something went\n\
terribly wrong...\n");
#ifdef __i386__
  __asm __volatile__ ("movl %%ebp,%0"
		      :"=r"(fp)
		      :"r"(fp));
#endif
#ifdef __x86_64__
  __asm __volatile__ ("movq %%rbp,%0"
		      :"=r"(fp)
		      :"r"(fp));
#endif
#if defined(__alpha__) && defined(__GNUC__) 
  __asm __volatile__ ("mov $30,%0"
		      :"=r"(fp)
		      :"r"(fp));
#endif
  if (!fp)
  {
    fprintf(stderr, "frame pointer is NULL, did you compile with\n\
-fomit-frame-pointer? Aborting backtrace!\n");
    return;
  }

  if (!stack_bottom || (gptr) stack_bottom > (gptr) &fp)
  {
    ulong tmp= min(0x10000,thread_stack);
    /* Assume that the stack starts at the previous even 65K */
    stack_bottom= (gptr) (((ulong) &fp + tmp) &
			  ~(ulong) 0xFFFF);
    fprintf(stderr, "Cannot determine thread, fp=%p, backtrace may not be correct.\n", fp);
  }
  if (fp > (uchar**) stack_bottom ||
      fp < (uchar**) stack_bottom - thread_stack)
  {
    fprintf(stderr, "Bogus stack limit or frame pointer,\
 fp=%p, stack_bottom=%p, thread_stack=%ld, aborting backtrace.\n",
	    fp, stack_bottom, thread_stack);
    return;
  }

  fprintf(stderr, "Stack range sanity check OK, backtrace follows:\n");
#if defined(__alpha__) && defined(__GNUC__)
  fprintf(stderr, "Warning: Alpha stacks are difficult -\
 will be taking some wild guesses, stack trace may be incorrect or \
 terminate abruptly\n");
  /* On Alpha, we need to get pc */
  __asm __volatile__ ("bsr %0, do_next; do_next: "
		      :"=r"(pc)
		      :"r"(pc));
#endif  /* __alpha__ */

  /* We are 1 frame above signal frame with NPTL and 2 frames above with LT */
  sigreturn_frame_count = thd_lib_detected == THD_LIB_LT ? 2 : 1;

  while (fp < (uchar**) stack_bottom)
  {
#if defined(__i386__) || defined(__x86_64__)
    uchar** new_fp = (uchar**)*fp;
    fprintf(stderr, "%p\n", frame_count == sigreturn_frame_count ?
	    *(fp + SIGRETURN_FRAME_OFFSET) : *(fp + 1));
#endif /* defined(__386__)  || defined(__x86_64__) */

#if defined(__alpha__) && defined(__GNUC__)
    uchar** new_fp = find_prev_fp(pc, fp);
    if (frame_count == sigreturn_frame_count - 1)
    {
      new_fp += 90;
    }

    if (fp && pc)
    {
      pc = find_prev_pc(pc, fp);
      if (pc)
	fprintf(stderr, "%p\n", pc);
      else
      {
	fprintf(stderr, "Not smart enough to deal with the rest\
 of this stack\n");
	goto end;
      }
    }
    else
    {
      fprintf(stderr, "Not smart enough to deal with the rest of this stack\n");
      goto end;
    }
#endif /* defined(__alpha__) && defined(__GNUC__) */
    if (new_fp <= fp )
    {
      fprintf(stderr, "New value of fp=%p failed sanity check,\
 terminating stack trace!\n", new_fp);
      goto end;
    }
    fp = new_fp;
    ++frame_count;
  }

  fprintf(stderr, "Stack trace seems successful - bottom reached\n");

end:
  fprintf(stderr, "Please read http://dev.mysql.com/doc/mysql/en/using-stack-trace.html and follow instructions on how to resolve the stack trace. Resolved\n\
stack trace is much more helpful in diagnosing the problem, so please do \n\
resolve it\n");
}
#endif /* TARGET_OS_LINUX */
#endif /* HAVE_STACKTRACE */

/* Produce a core for the thread */

#ifdef NOT_USED /* HAVE_LINUXTHREADS */
void write_core(int sig)
{
  signal(sig, SIG_DFL);
  if (fork() != 0) exit(1);			/* Abort main program */
  /* Core will be written at exit */
}
#else
void write_core(int sig)
{
  signal(sig, SIG_DFL);
  pthread_kill(pthread_self(), sig);
#if defined(P_MYID) && !defined(SCO)
  /* On Solaris, the above kill is not enough */
  sigsend(P_PID,P_MYID,sig);
#endif
}
#endif
