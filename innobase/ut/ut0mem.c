/************************************************************************
Memory primitives

(c) 1994, 1995 Innobase Oy

Created 5/11/1994 Heikki Tuuri
*************************************************************************/

#include "ut0mem.h"

#ifdef UNIV_NONINL
#include "ut0mem.ic"
#endif

#include "mem0mem.h"
#include "os0sync.h"

/* This struct is placed first in every allocated memory block */
typedef struct ut_mem_block_struct ut_mem_block_t;

/* The total amount of memory currently allocated from the OS with malloc */
ulint	ut_total_allocated_memory	= 0;

struct ut_mem_block_struct{
        UT_LIST_NODE_T(ut_mem_block_t) mem_block_list;
			/* mem block list node */
	ulint	size;	/* size of allocated memory */
	ulint	magic_n;
};

#define UT_MEM_MAGIC_N	1601650166

/* List of all memory blocks allocated from the operating system
with malloc */
UT_LIST_BASE_NODE_T(ut_mem_block_t)   ut_mem_block_list;

os_fast_mutex_t ut_list_mutex;  /* this protects the list */

ibool  ut_mem_block_list_inited = FALSE;

ulint*	ut_mem_null_ptr	= NULL;

/**************************************************************************
Initializes the mem block list at database startup. */
static
void
ut_mem_block_list_init(void)
/*========================*/
{
        os_fast_mutex_init(&ut_list_mutex);
        UT_LIST_INIT(ut_mem_block_list);
	ut_mem_block_list_inited = TRUE;
}

/**************************************************************************
Allocates memory. Sets it also to zero if UNIV_SET_MEM_TO_ZERO is
defined and set_to_zero is TRUE. */

void*
ut_malloc_low(
/*==========*/
	                     /* out, own: allocated memory */
        ulint   n,           /* in: number of bytes to allocate */
	ibool   set_to_zero) /* in: TRUE if allocated memory should be set
			     to zero if UNIV_SET_MEM_TO_ZERO is defined */
{
	void*	ret;

	ut_ad((sizeof(ut_mem_block_t) % 8) == 0); /* check alignment ok */

	if (!ut_mem_block_list_inited) {
	        ut_mem_block_list_init();
	}

	os_fast_mutex_lock(&ut_list_mutex);

	ret = malloc(n + sizeof(ut_mem_block_t));

	if (ret == NULL) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
		"  InnoDB: Fatal error: cannot allocate %lu bytes of\n"
		"InnoDB: memory with malloc! Total allocated memory\n"
		"InnoDB: by InnoDB %lu bytes. Operating system errno: %lu\n"
		"InnoDB: Cannot continue operation!\n"
		"InnoDB: Check if you should increase the swap file or\n"
		"InnoDB: ulimits of your operating system.\n"
		"InnoDB: On FreeBSD check you have compiled the OS with\n"
		"InnoDB: a big enough maximum process size.\n"
		"InnoDB: We now intentionally generate a seg fault so that\n"
		"InnoDB: on Linux we get a stack trace.\n",
		                  (ulong) n, (ulong) ut_total_allocated_memory,
#ifdef __WIN__
			(ulong) GetLastError()
#else
			(ulong) errno
#endif
			);

		/* Flush stderr to make more probable that the error
		message gets in the error file before we generate a seg
		fault */

		fflush(stderr);

	        os_fast_mutex_unlock(&ut_list_mutex);

		/* Make an intentional seg fault so that we get a stack
		trace */
		if (*ut_mem_null_ptr) ut_mem_null_ptr = 0;
	}		

	if (set_to_zero) {
#ifdef UNIV_SET_MEM_TO_ZERO
	        memset(ret, '\0', n + sizeof(ut_mem_block_t));
#endif
	}

	((ut_mem_block_t*)ret)->size = n + sizeof(ut_mem_block_t);
	((ut_mem_block_t*)ret)->magic_n = UT_MEM_MAGIC_N;

	ut_total_allocated_memory += n + sizeof(ut_mem_block_t);
	
	UT_LIST_ADD_FIRST(mem_block_list, ut_mem_block_list,
			                         ((ut_mem_block_t*)ret));
	os_fast_mutex_unlock(&ut_list_mutex);

	return((void*)((byte*)ret + sizeof(ut_mem_block_t)));
}

/**************************************************************************
Allocates memory. Sets it also to zero if UNIV_SET_MEM_TO_ZERO is
defined. */

void*
ut_malloc(
/*======*/
	                /* out, own: allocated memory */
        ulint   n)      /* in: number of bytes to allocate */
{
        return(ut_malloc_low(n, TRUE));
}

/**************************************************************************
Tests if malloc of n bytes would succeed. ut_malloc() asserts if memory runs
out. It cannot be used if we want to return an error message. Prints to
stderr a message if fails. */

ibool
ut_test_malloc(
/*===========*/
			/* out: TRUE if succeeded */
	ulint	n)	/* in: try to allocate this many bytes */
{
	void*	ret;

	ret = malloc(n);

	if (ret == NULL) {
		ut_print_timestamp(stderr);
		fprintf(stderr,
		"  InnoDB: Error: cannot allocate %lu bytes of memory for\n"
		"InnoDB: a BLOB with malloc! Total allocated memory\n"
		"InnoDB: by InnoDB %lu bytes. Operating system errno: %d\n"
		"InnoDB: Check if you should increase the swap file or\n"
		"InnoDB: ulimits of your operating system.\n"
		"InnoDB: On FreeBSD check you have compiled the OS with\n"
		"InnoDB: a big enough maximum process size.\n",
		                  (ulong) n,
			          (ulong) ut_total_allocated_memory,
				  (int) errno);
		return(FALSE);
	}

	free(ret);

	return(TRUE);
}	

/**************************************************************************
Frees a memory block allocated with ut_malloc. */

void
ut_free(
/*====*/
	void* ptr)  /* in, own: memory block */
{
        ut_mem_block_t* block;

	block = (ut_mem_block_t*)((byte*)ptr - sizeof(ut_mem_block_t));

	os_fast_mutex_lock(&ut_list_mutex);

	ut_a(block->magic_n == UT_MEM_MAGIC_N);
	ut_a(ut_total_allocated_memory >= block->size);

	ut_total_allocated_memory -= block->size;
	
	UT_LIST_REMOVE(mem_block_list, ut_mem_block_list, block);
	free(block);
	
	os_fast_mutex_unlock(&ut_list_mutex);
}

/**************************************************************************
Frees in shutdown all allocated memory not freed yet. */

void
ut_free_all_mem(void)
/*=================*/
{
        ut_mem_block_t* block;

        os_fast_mutex_free(&ut_list_mutex);

	while ((block = UT_LIST_GET_FIRST(ut_mem_block_list))) {

		ut_a(block->magic_n == UT_MEM_MAGIC_N);
		ut_a(ut_total_allocated_memory >= block->size);

		ut_total_allocated_memory -= block->size;
	
	        UT_LIST_REMOVE(mem_block_list, ut_mem_block_list, block);
	        free(block);
	}
		                      
	if (ut_total_allocated_memory != 0) {
		fprintf(stderr,
"InnoDB: Warning: after shutdown total allocated memory is %lu\n",
		  (ulong) ut_total_allocated_memory);
	}
}

/**************************************************************************
Make a quoted copy of a string. */

char*
ut_strcpyq(
/*=======*/
				/* out: pointer to end of dest */
	char*		dest,	/* in: output buffer */
	char		q,	/* in: the quote character */
	const char*	src)	/* in: null-terminated string */
{
	while (*src) {
		if ((*dest++ = *src++) == q) {
			*dest++ = q;
		}
	}

	return(dest);
}

/**************************************************************************
Make a quoted copy of a fixed-length string. */

char*
ut_memcpyq(
/*=======*/
				/* out: pointer to end of dest */
	char*		dest,	/* in: output buffer */
	char		q,	/* in: the quote character */
	const char*	src,	/* in: string to be quoted */
	ulint		len)	/* in: length of src */
{
	const char*	srcend = src + len;

	while (src < srcend) {
		if ((*dest++ = *src++) == q) {
			*dest++ = q;
		}
	}

	return(dest);
}

/**************************************************************************
Catenates two strings into newly allocated memory. The memory must be freed
using mem_free. */

char*
ut_str_catenate(
/*============*/
			/* out, own: catenated null-terminated string */
	char*	str1,	/* in: null-terminated string */
	char*	str2)	/* in: null-terminated string */
{
	ulint	len1;
	ulint	len2;
	char*	str;

	len1 = ut_strlen(str1);
	len2 = ut_strlen(str2);

	str = mem_alloc(len1 + len2 + 1);

	ut_memcpy(str, str1, len1);
	ut_memcpy(str + len1, str2, len2 + 1);

	return(str);
}

/**************************************************************************
Return a copy of the given string. The returned string must be freed
using mem_free. */

char*
ut_strdup(
/*======*/
			/* out, own: cnull-terminated string */
	char*	str)	/* in: null-terminated string */
{
	ulint	len;
	char*	copy;

	len = ut_strlen(str);

	copy = mem_alloc(len + 1);

	ut_memcpy(copy, str, len);

	copy[len] = 0;

	return(copy);
}
