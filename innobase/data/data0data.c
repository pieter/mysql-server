/************************************************************************
SQL data field and tuple

(c) 1994-1996 Innobase Oy

Created 5/30/1994 Heikki Tuuri
*************************************************************************/

#include "data0data.h"

#ifdef UNIV_NONINL
#include "data0data.ic"
#endif

#include "ut0rnd.h"


byte	data_error;	/* data pointers of tuple fields are initialized
			to point here for error checking */

ulint	data_dummy;	/* this is used to fool the compiler in
			dtuple_validate */

byte	data_buf[8192];	/* used in generating test tuples */
ulint	data_rnd = 756511;


/* Some non-inlined functions used in the MySQL interface: */
void 
dfield_set_data_noninline(
	dfield_t* 	field,	/* in: field */
	void*		data,	/* in: data */
	ulint		len)	/* in: length or UNIV_SQL_NULL */
{
	dfield_set_data(field, data, len);
}
void* 
dfield_get_data_noninline(
	dfield_t* field)	/* in: field */
{
	return(dfield_get_data(field));
}
ulint
dfield_get_len_noninline(
	dfield_t* field)	/* in: field */
{
	return(dfield_get_len(field));
}
ulint 
dtuple_get_n_fields_noninline(
	dtuple_t* 	tuple)	/* in: tuple */
{
	return(dtuple_get_n_fields(tuple));
}
dfield_t* 
dtuple_get_nth_field_noninline(
	dtuple_t* 	tuple,	/* in: tuple */
	ulint		n)	/* in: index of field */
{
	return(dtuple_get_nth_field(tuple, n));
}

/*************************************************************************
Creates a dtuple for use in MySQL. */

dtuple_t*
dtuple_create_for_mysql(
/*====================*/
				/* out, own created dtuple */
	void** 	heap,    	/* out: created memory heap */
	ulint 	n_fields) 	/* in: number of fields */
{
  	*heap = (void*)mem_heap_create(500);
 
  	return(dtuple_create(*((mem_heap_t**)heap), n_fields));  
}

/*************************************************************************
Frees a dtuple used in MySQL. */

void
dtuple_free_for_mysql(
/*==================*/
	void*	heap) /* in: memory heap where tuple was created */
{
  	mem_heap_free((mem_heap_t*)heap);
}

/*************************************************************************
Sets number of fields used in a tuple. Normally this is set in
dtuple_create, but if you want later to set it smaller, you can use this. */ 

void
dtuple_set_n_fields(
/*================*/
	dtuple_t*	tuple,		/* in: tuple */
	ulint		n_fields)	/* in: number of fields */
{
	ut_ad(tuple);

	tuple->n_fields = n_fields;
	tuple->n_fields_cmp = n_fields;
}

/**************************************************************
Checks that a data field is typed. Asserts an error if not. */

ibool
dfield_check_typed(
/*===============*/
				/* out: TRUE if ok */
	dfield_t*	field)	/* in: data field */
{
	ut_a(dfield_get_type(field)->mtype <= DATA_SYS);
	ut_a(dfield_get_type(field)->mtype >= DATA_VARCHAR);

	return(TRUE);
}

/**************************************************************
Checks that a data tuple is typed. Asserts an error if not. */

ibool
dtuple_check_typed(
/*===============*/
				/* out: TRUE if ok */
	dtuple_t*	tuple)	/* in: tuple */
{
	dfield_t*	field;
	ulint	 	i;

	for (i = 0; i < dtuple_get_n_fields(tuple); i++) {

		field = dtuple_get_nth_field(tuple, i);

		ut_a(dfield_check_typed(field));
	}

	return(TRUE);
}

/**************************************************************
Validates the consistency of a tuple which must be complete, i.e,
all fields must have been set. */

ibool
dtuple_validate(
/*============*/
				/* out: TRUE if ok */
	dtuple_t*	tuple)	/* in: tuple */
{
	dfield_t*	field;
	byte*	 	data;
	ulint	 	n_fields;
	ulint	 	len;
	ulint	 	i;
	ulint	 	j;
	ulint	 	sum	= 0; /* A dummy variable used
					to prevent the compiler
					from erasing the loop below */
	ut_a(tuple->magic_n = DATA_TUPLE_MAGIC_N);

	n_fields = dtuple_get_n_fields(tuple);

	/* We dereference all the data of each field to test
	for memory traps */

	for (i = 0; i < n_fields; i++) {

		field = dtuple_get_nth_field(tuple, i);
		len = dfield_get_len(field);
	
		if (len != UNIV_SQL_NULL) {

			data = field->data;

			for (j = 0; j < len; j++) {

				data_dummy  += *data; /* fool the compiler not
							to optimize out this
							code */
				data++;
			}
		}
	}

	ut_a(dtuple_check_typed(tuple));

	return(TRUE);
}

/*****************************************************************
Pretty prints a dfield value according to its data type. */

void
dfield_print(
/*=========*/
	dfield_t*	dfield)	 /* in: dfield */
{
	byte*	data;
	ulint	len;
	ulint	mtype;
	ulint	i;

	len = dfield_get_len(dfield);
	data = dfield_get_data(dfield);

	if (len == UNIV_SQL_NULL) {
		printf("NULL");

		return;
	}

	mtype = dtype_get_mtype(dfield_get_type(dfield));

	if ((mtype == DATA_CHAR) || (mtype == DATA_VARCHAR)) {
	
		for (i = 0; i < len; i++) {

			if (isprint((char)(*data))) {
				printf("%c", (char)*data);
			} else {
				printf(" ");
			}

			data++;
		}
	} else if (mtype == DATA_INT) {
		ut_a(len == 4); /* only works for 32-bit integers */
		printf("%li", (int)mach_read_from_4(data));
	} else {
		ut_error;
	}
}

/*****************************************************************
Pretty prints a dfield value according to its data type. Also the hex string
is printed if a string contains non-printable characters. */ 

void
dfield_print_also_hex(
/*==================*/
	dfield_t*	dfield)	 /* in: dfield */
{
	byte*	data;
	ulint	len;
	ulint	mtype;
	ulint	i;
	ibool	print_also_hex;

	len = dfield_get_len(dfield);
	data = dfield_get_data(dfield);

	if (len == UNIV_SQL_NULL) {
		printf("NULL");

		return;
	}

	mtype = dtype_get_mtype(dfield_get_type(dfield));

	if ((mtype == DATA_CHAR) || (mtype == DATA_VARCHAR)) {

		print_also_hex = FALSE;
	
		for (i = 0; i < len; i++) {

			if (isprint((char)(*data))) {
				printf("%c", (char)*data);
			} else {
				print_also_hex = TRUE;
				printf(" ");
			}

			data++;
		}

		if (!print_also_hex) {

			return;
		}

		printf(" Hex: ");
		
		data = dfield_get_data(dfield);
		
		for (i = 0; i < len; i++) {
			printf("%02x", (ulint)*data);

			data++;
		}
	} else if (mtype == DATA_INT) {
		ut_a(len == 4); /* inly works for 32-bit integers */
		printf("%li", (int)mach_read_from_4(data));
	} else {
		ut_error;
	}
}

/**************************************************************
The following function prints the contents of a tuple. */

void
dtuple_print(
/*=========*/
	dtuple_t*	tuple)	/* in: tuple */
{
	dfield_t*	field;
	ulint		n_fields;
	ulint		i;

	n_fields = dtuple_get_n_fields(tuple);

	printf("DATA TUPLE: %lu fields;\n", n_fields);

	for (i = 0; i < n_fields; i++) {
		printf(" %lu:", i);	

		field = dtuple_get_nth_field(tuple, i);
		
		if (field->len != UNIV_SQL_NULL) {
			ut_print_buf(field->data, field->len);
		} else {
			printf(" SQL NULL");
		}

		printf(";");
	}

	printf("\n");

	dtuple_validate(tuple);
}

/**************************************************************
The following function prints the contents of a tuple to a buffer. */

ulint
dtuple_sprintf(
/*===========*/
				/* out: printed length in bytes */
	char*		buf,	/* in: print buffer */
	ulint		buf_len,/* in: buf length in bytes */
	dtuple_t*	tuple)	/* in: tuple */
{
	dfield_t*	field;
	ulint		n_fields;
	ulint		len;
	ulint		i;

	len = 0;

	n_fields = dtuple_get_n_fields(tuple);

	for (i = 0; i < n_fields; i++) {
		if (len + 30 > buf_len) {

			return(len);
		}

		len += sprintf(buf + len, " %lu:", i);	

		field = dtuple_get_nth_field(tuple, i);
		
		if (field->len != UNIV_SQL_NULL) {
			if (5 * field->len + len + 30 > buf_len) {

				return(len);
			}
		
			len += ut_sprintf_buf(buf + len, field->data,
								field->len);
		} else {
			len += sprintf(buf + len, " SQL NULL");
		}

		len += sprintf(buf + len, ";");
	}

	return(len);
}

/******************************************************************
Generates random numbers, where 10/16 is uniformly
distributed between 0 and n1, 5/16 between 0 and n2,
and 1/16 between 0 and n3. */
static
ulint
dtuple_gen_rnd_ulint(
/*=================*/
			/* out: random ulint */
	ulint	n1,	
	ulint	n2,
	ulint	n3)
{
	ulint 	m;
	ulint	n;

	m = ut_rnd_gen_ulint() % 16;
	
	if (m < 10) {
		n = n1;
	} else if (m < 15) {
		n = n2;
	} else {
		n = n3;
	}
	
	m = ut_rnd_gen_ulint();

	return(m % n);
}

/***************************************************************
Generates a random tuple. */

dtuple_t*
dtuple_gen_rnd_tuple(
/*=================*/
				/* out: pointer to the tuple */
	mem_heap_t*	heap)	/* in: memory heap where generated */
{
	ulint		n_fields;
	dfield_t*	field;
	ulint		len;
	dtuple_t*	tuple;	
	ulint		i;
	ulint		j;
	byte*		ptr;

	n_fields = dtuple_gen_rnd_ulint(5, 30, 300) + 1;

	tuple = dtuple_create(heap, n_fields);

	for (i = 0; i < n_fields; i++) {

		if (n_fields < 7) {
			len = dtuple_gen_rnd_ulint(5, 30, 400);
		} else {
			len = dtuple_gen_rnd_ulint(7, 5, 17);
		}

		field = dtuple_get_nth_field(tuple, i);
		
		if (len == 0) {
			dfield_set_data(field, NULL, UNIV_SQL_NULL);
		} else {
			ptr = mem_heap_alloc(heap, len);
			dfield_set_data(field, ptr, len - 1);

			for (j = 0; j < len; j++) {
				*ptr = (byte)(65 + 
					dtuple_gen_rnd_ulint(22, 22, 22));
				ptr++;
			}
		}

		dtype_set(dfield_get_type(field), DATA_VARCHAR,
							DATA_ENGLISH, 500, 0);
	}

	ut_a(dtuple_validate(tuple));

	return(tuple);
}

/*******************************************************************
Generates a test tuple for sort and comparison tests. */

void
dtuple_gen_test_tuple(
/*==================*/
	dtuple_t*	tuple,	/* in/out: a tuple with 3 fields */
	ulint		i)	/* in: a number < 512 */
{
	ulint		j;
	dfield_t*	field;
	void*		data	= NULL;
	ulint		len	= 0;

	for (j = 0; j < 3; j++) {
		switch (i % 8) {
			case 0:
				data = ""; len = 0; break;
			case 1:
				data = "A"; len = 1; break;
			case 2:
				data = "AA"; len = 2; break;
			case 3:
				data = "AB"; len = 2; break;
			case 4:
				data = "B"; len = 1; break;
			case 5:
				data = "BA"; len = 2; break;
			case 6:
				data = "BB"; len = 2; break;
			case 7:
				len = UNIV_SQL_NULL; break;
		}

		field = dtuple_get_nth_field(tuple, 2 - j);
		
		dfield_set_data(field, data, len);
		dtype_set(dfield_get_type(field), DATA_VARCHAR,
				DATA_ENGLISH, 100, 0);
		
		i = i / 8;
	}
	
	ut_ad(dtuple_validate(tuple));
}

/*******************************************************************
Generates a test tuple for B-tree speed tests. */

void
dtuple_gen_test_tuple3(
/*===================*/
	dtuple_t*	tuple,	/* in/out: a tuple with >= 3 fields */
	ulint		i,	/* in: a number < 1000000 */
	ulint		type,	/* in: DTUPLE_TEST_FIXED30, ... */
	byte*		buf)	/* in: a buffer of size >= 16 bytes */
{
	dfield_t*	field;
	ulint		third_size;

	ut_ad(tuple && buf);
	ut_ad(i < 1000000);
	
	field = dtuple_get_nth_field(tuple, 0);

	ut_strcpy((char*)buf, "0000000");

	buf[1] = (byte)('0' + (i / 100000) % 10);
	buf[2] = (byte)('0' + (i / 10000) % 10);
	buf[3] = (byte)('0' + (i / 1000) % 10);
	buf[4] = (byte)('0' + (i / 100) % 10);
	buf[5] = (byte)('0' + (i / 10) % 10);
	buf[6] = (byte)('0' + (i % 10));
	
	dfield_set_data(field, buf, 8);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 100, 0);

	field = dtuple_get_nth_field(tuple, 1);

	i = i % 1000; /* ut_rnd_gen_ulint() % 1000000; */

	ut_strcpy((char*)buf + 8, "0000000");

	buf[9] = (byte)('0' + (i / 100000) % 10);
	buf[10] = (byte)('0' + (i / 10000) % 10);
	buf[11] = (byte)('0' + (i / 1000) % 10);
	buf[12] = (byte)('0' + (i / 100) % 10);
	buf[13] = (byte)('0' + (i / 10) % 10);
	buf[14] = (byte)('0' + (i % 10));
	
	dfield_set_data(field, buf + 8, 8);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 100, 0);

	field = dtuple_get_nth_field(tuple, 2);

	data_rnd += 8757651;

	if (type == DTUPLE_TEST_FIXED30) {
		third_size = 30;
	} else if (type == DTUPLE_TEST_RND30) {
		third_size = data_rnd % 30;
	} else if (type == DTUPLE_TEST_RND3500) {
		third_size = data_rnd % 3500;
	} else if (type == DTUPLE_TEST_FIXED2000) {
		third_size = 2000;
	} else if (type == DTUPLE_TEST_FIXED3) {
		third_size = 3;
	} else {
		ut_error;
	}
	
	if (type == DTUPLE_TEST_FIXED30) {
		dfield_set_data(field,
			"12345678901234567890123456789", third_size);
	} else {
		dfield_set_data(field, data_buf, third_size);
	}
	
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 100, 0);

	ut_ad(dtuple_validate(tuple));
}

/*******************************************************************
Generates a test tuple for B-tree speed tests. */

void
dtuple_gen_search_tuple3(
/*=====================*/
	dtuple_t*	tuple,	/* in/out: a tuple with 1 or 2 fields */
	ulint		i,	/* in: a number < 1000000 */
	byte*		buf)	/* in: a buffer of size >= 16 bytes */
{
	dfield_t*	field;

	ut_ad(tuple && buf);
	ut_ad(i < 1000000);
	
	field = dtuple_get_nth_field(tuple, 0);

	ut_strcpy((char*)buf, "0000000");

	buf[1] = (byte)('0' + (i / 100000) % 10);
	buf[2] = (byte)('0' + (i / 10000) % 10);
	buf[3] = (byte)('0' + (i / 1000) % 10);
	buf[4] = (byte)('0' + (i / 100) % 10);
	buf[5] = (byte)('0' + (i / 10) % 10);
	buf[6] = (byte)('0' + (i % 10));
	
	dfield_set_data(field, buf, 8);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 100, 0);

	if (dtuple_get_n_fields(tuple) == 1) {

		return;
	}

	field = dtuple_get_nth_field(tuple, 1);

	i = (i * 1000) % 1000000;

	ut_strcpy((char*)buf + 8, "0000000");

	buf[9] = (byte)('0' + (i / 100000) % 10);
	buf[10] = (byte)('0' + (i / 10000) % 10);
	buf[11] = (byte)('0' + (i / 1000) % 10);
	buf[12] = (byte)('0' + (i / 100) % 10);
	buf[13] = (byte)('0' + (i / 10) % 10);
	buf[14] = (byte)('0' + (i % 10));
	
	dfield_set_data(field, buf + 8, 8);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 100, 0);

	ut_ad(dtuple_validate(tuple));
}

/*******************************************************************
Generates a test tuple for TPC-A speed test. */

void
dtuple_gen_test_tuple_TPC_A(
/*========================*/
	dtuple_t*	tuple,	/* in/out: a tuple with >= 3 fields */
	ulint		i,	/* in: a number < 10000 */
	byte*		buf)	/* in: a buffer of size >= 16 bytes */
{
	dfield_t*	field;
	ulint		third_size;

	ut_ad(tuple && buf);
	ut_ad(i < 10000);
	
	field = dtuple_get_nth_field(tuple, 0);

	ut_strcpy((char*)buf, "0000");

	buf[0] = (byte)('0' + (i / 1000) % 10);
	buf[1] = (byte)('0' + (i / 100) % 10);
	buf[2] = (byte)('0' + (i / 10) % 10);
	buf[3] = (byte)('0' + (i % 10));
	
	dfield_set_data(field, buf, 5);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 100, 0);

	field = dtuple_get_nth_field(tuple, 1);
	
	dfield_set_data(field, buf + 8, 5);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 100, 0);

	field = dtuple_get_nth_field(tuple, 2);

	third_size = 90;
	
	dfield_set_data(field, data_buf, third_size);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 100, 0);

	ut_ad(dtuple_validate(tuple));
}

/*******************************************************************
Generates a test tuple for B-tree speed tests. */

void
dtuple_gen_search_tuple_TPC_A(
/*==========================*/
	dtuple_t*	tuple,	/* in/out: a tuple with 1 field */
	ulint		i,	/* in: a number < 10000 */
	byte*		buf)	/* in: a buffer of size >= 16 bytes */
{
	dfield_t*	field;

	ut_ad(tuple && buf);
	ut_ad(i < 10000);
	
	field = dtuple_get_nth_field(tuple, 0);

	ut_strcpy((char*)buf, "0000");

	buf[0] = (byte)('0' + (i / 1000) % 10);
	buf[1] = (byte)('0' + (i / 100) % 10);
	buf[2] = (byte)('0' + (i / 10) % 10);
	buf[3] = (byte)('0' + (i % 10));
	
	dfield_set_data(field, buf, 5);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 100, 0);

	ut_ad(dtuple_validate(tuple));
}

/*******************************************************************
Generates a test tuple for TPC-C speed test. */

void
dtuple_gen_test_tuple_TPC_C(
/*========================*/
	dtuple_t*	tuple,	/* in/out: a tuple with >= 12 fields */
	ulint		i,	/* in: a number < 100000 */
	byte*		buf)	/* in: a buffer of size >= 16 bytes */
{
	dfield_t*	field;
	ulint		size;
	ulint		j;

	ut_ad(tuple && buf);
	ut_ad(i < 100000);
	
	field = dtuple_get_nth_field(tuple, 0);

	buf[0] = (byte)('0' + (i / 10000) % 10);
	buf[1] = (byte)('0' + (i / 1000) % 10);
	buf[2] = (byte)('0' + (i / 100) % 10);
	buf[3] = (byte)('0' + (i / 10) % 10);
	buf[4] = (byte)('0' + (i % 10));
	
	dfield_set_data(field, buf, 5);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 100, 0);

	field = dtuple_get_nth_field(tuple, 1);
	
	dfield_set_data(field, buf, 5);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 100, 0);

	for (j = 0; j < 10; j++) {

		field = dtuple_get_nth_field(tuple, 2 + j);

		size = 24;
	
		dfield_set_data(field, data_buf, size);
		dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH,
								100, 0);
	}

	ut_ad(dtuple_validate(tuple));
}

/*******************************************************************
Generates a test tuple for B-tree speed tests. */

void
dtuple_gen_search_tuple_TPC_C(
/*==========================*/
	dtuple_t*	tuple,	/* in/out: a tuple with 1 field */
	ulint		i,	/* in: a number < 100000 */
	byte*		buf)	/* in: a buffer of size >= 16 bytes */
{
	dfield_t*	field;

	ut_ad(tuple && buf);
	ut_ad(i < 100000);
	
	field = dtuple_get_nth_field(tuple, 0);

	buf[0] = (byte)('0' + (i / 10000) % 10);
	buf[1] = (byte)('0' + (i / 1000) % 10);
	buf[2] = (byte)('0' + (i / 100) % 10);
	buf[3] = (byte)('0' + (i / 10) % 10);
	buf[4] = (byte)('0' + (i % 10));
	
	dfield_set_data(field, buf, 5);
	dtype_set(dfield_get_type(field), DATA_VARCHAR, DATA_ENGLISH, 100, 0);

	ut_ad(dtuple_validate(tuple));
}
