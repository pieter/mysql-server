/***********************************************************************
Memory primitives

(c) 1994, 1995 Innobase Oy

Created 5/30/1994 Heikki Tuuri
************************************************************************/

#ifndef ut0mem_h
#define ut0mem_h

#include "univ.i"
#include <string.h>
#include <stdlib.h>

/* The total amount of memory currently allocated from the OS with malloc */
extern ulint	ut_total_allocated_memory;

UNIV_INLINE
void*
ut_memcpy(void* dest, const void* sour, ulint n);

UNIV_INLINE
void*
ut_memmove(void* dest, const void* sour, ulint n);

UNIV_INLINE
int
ut_memcmp(const void* str1, const void* str2, ulint n);


/**************************************************************************
Allocates memory. Sets it also to zero if UNIV_SET_MEM_TO_ZERO is
defined and set_to_zero is TRUE. */

void*
ut_malloc_low(
/*==========*/
	                     /* out, own: allocated memory */
        ulint   n,           /* in: number of bytes to allocate */
	ibool   set_to_zero); /* in: TRUE if allocated memory should be set
			     to zero if UNIV_SET_MEM_TO_ZERO is defined */
/**************************************************************************
Allocates memory. Sets it also to zero if UNIV_SET_MEM_TO_ZERO is
defined. */

void*
ut_malloc(
/*======*/
	                /* out, own: allocated memory */
        ulint   n);     /* in: number of bytes to allocate */
/**************************************************************************
Tests if malloc of n bytes would succeed. ut_malloc() asserts if memory runs
out. It cannot be used if we want to return an error message. Prints to
stderr a message if fails. */

ibool
ut_test_malloc(
/*===========*/
			/* out: TRUE if succeeded */
	ulint	n);	/* in: try to allocate this many bytes */
/**************************************************************************
Frees a memory bloock allocated with ut_malloc. */

void
ut_free(
/*====*/
	void* ptr);  /* in, own: memory block */
/**************************************************************************
Frees in shutdown all allocated memory not freed yet. */

void
ut_free_all_mem(void);
/*=================*/

UNIV_INLINE
char*
ut_strcpy(char* dest, const char* sour);

UNIV_INLINE
ulint
ut_strlen(const char* str);

UNIV_INLINE
int
ut_strcmp(const void* str1, const void* str2);

/**************************************************************************
Determine the length of a string when it is quoted with ut_strcpyq(). */
UNIV_INLINE
ulint
ut_strlenq(
/*=======*/
				/* out: length of the string when quoted */
	const char*	str,	/* in: null-terminated string */
	char		q);	/* in: the quote character */

/**************************************************************************
Make a quoted copy of a string. */

char*
ut_strcpyq(
/*=======*/
				/* out: pointer to end of dest */
	char*		dest,	/* in: output buffer */
	char		q,	/* in: the quote character */
	const char*	src);	/* in: null-terminated string */

/**************************************************************************
Make a quoted copy of a fixed-length string. */

char*
ut_memcpyq(
/*=======*/
				/* out: pointer to end of dest */
	char*		dest,	/* in: output buffer */
	char		q,	/* in: the quote character */
	const char*	src,	/* in: string to be quoted */
	ulint		len);	/* in: length of src */

#ifndef UNIV_NONINL
#include "ut0mem.ic"
#endif

#endif
