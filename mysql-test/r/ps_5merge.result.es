use test;
drop table if exists t1, t1_1, t1_2,
t9, t9_1, t9_2;
drop table if exists t1, t9 ;
create table t1
(
a int, b varchar(30),
primary key(a)
) engine = 'MYISAM'  ;
create table t9 
(
c1  tinyint, c2  smallint, c3  mediumint, c4  int,
c5  integer, c6  bigint, c7  float, c8  double,
c9  double precision, c10 real, c11 decimal(7, 4), c12 numeric(8, 4),
c13 date, c14 datetime, c15 timestamp(14), c16 time,
c17 year, c18 bit, c19 bool, c20 char,
c21 char(10), c22 varchar(30), c23 tinyblob, c24 tinytext,
c25 blob, c26 text, c27 mediumblob, c28 mediumtext,
c29 longblob, c30 longtext, c31 enum('one', 'two', 'three'),
c32 set('monday', 'tuesday', 'wednesday'),
primary key(c1)
) engine = 'MYISAM'  ;
rename table t1 to t1_1, t9 to t9_1 ;
drop table if exists t1, t9 ;
create table t1
(
a int, b varchar(30),
primary key(a)
) engine = 'MYISAM'  ;
create table t9 
(
c1  tinyint, c2  smallint, c3  mediumint, c4  int,
c5  integer, c6  bigint, c7  float, c8  double,
c9  double precision, c10 real, c11 decimal(7, 4), c12 numeric(8, 4),
c13 date, c14 datetime, c15 timestamp(14), c16 time,
c17 year, c18 bit, c19 bool, c20 char,
c21 char(10), c22 varchar(30), c23 tinyblob, c24 tinytext,
c25 blob, c26 text, c27 mediumblob, c28 mediumtext,
c29 longblob, c30 longtext, c31 enum('one', 'two', 'three'),
c32 set('monday', 'tuesday', 'wednesday'),
primary key(c1)
) engine = 'MYISAM'  ;
rename table t1 to t1_2, t9 to t9_2 ;
create table t1
(
a int, b varchar(30),
primary key(a)
) ENGINE = MERGE UNION=(t1_1,t1_2)
INSERT_METHOD=FIRST;
create table t9
(
c1  tinyint, c2  smallint, c3  mediumint, c4  int,
c5  integer, c6  bigint, c7  float, c8  double,
c9  double precision, c10 real, c11 decimal(7, 4), c12 numeric(8, 4),
c13 date, c14 datetime, c15 timestamp(14), c16 time,
c17 year, c18 bit, c19 bool, c20 char,
c21 char(10), c22 varchar(30), c23 tinyblob, c24 tinytext,
c25 blob, c26 text, c27 mediumblob, c28 mediumtext,
c29 longblob, c30 longtext, c31 enum('one', 'two', 'three'),
c32 set('monday', 'tuesday', 'wednesday'),
primary key(c1)
)  ENGINE = MERGE UNION=(t9_1,t9_2)
INSERT_METHOD=FIRST;
delete from t1 ;
insert into t1 values (1,'one');
insert into t1 values (2,'two');
insert into t1 values (3,'three');
insert into t1 values (4,'four');
commit ;
delete from t9 ;
insert into t9
set c1= 1, c2= 1, c3= 1, c4= 1, c5= 1, c6= 1, c7= 1, c8= 1, c9= 1,
c10= 1, c11= 1, c12 = 1,
c13= '2004-02-29', c14= '2004-02-29 11:11:11', c15= '2004-02-29 11:11:11',
c16= '11:11:11', c17= '2004',
c18= 1, c19=true, c20= 'a', c21= '123456789a', 
c22= '123456789a123456789b123456789c', c23= 'tinyblob', c24= 'tinytext',
c25= 'blob', c26= 'text', c27= 'mediumblob', c28= 'mediumtext',
c29= 'longblob', c30= 'longtext', c31='one', c32= 'monday';
insert into t9
set c1= 9, c2= 9, c3= 9, c4= 9, c5= 9, c6= 9, c7= 9, c8= 9, c9= 9,
c10= 9, c11= 9, c12 = 9,
c13= '2004-02-29', c14= '2004-02-29 11:11:11', c15= '2004-02-29 11:11:11',
c16= '11:11:11', c17= '2004',
c18= 1, c19=false, c20= 'a', c21= '123456789a', 
c22= '123456789a123456789b123456789c', c23= 'tinyblob', c24= 'tinytext',
c25= 'blob', c26= 'text', c27= 'mediumblob', c28= 'mediumtext',
c29= 'longblob', c30= 'longtext', c31='two', c32= 'tuesday';
commit ;
test_sequence
------ simple select tests ------
prepare stmt1 from ' select * from t9 order by c1 ' ;
execute stmt1;
Catalog	Database	Table	Table_alias	Column	Column_alias	Name	Type	Length	Max length	Is_null	Flags	Decimals	Charsetnr
def	test	t9	t9	c1	c1	1	4	1	N	49155	0	63
def	test	t9	t9	c2	c2	2	6	1	Y	32768	0	63
def	test	t9	t9	c3	c3	9	9	1	Y	32768	0	63
def	test	t9	t9	c4	c4	3	11	1	Y	32768	0	63
def	test	t9	t9	c5	c5	3	11	1	Y	32768	0	63
def	test	t9	t9	c6	c6	8	20	1	Y	32768	0	63
def	test	t9	t9	c7	c7	4	12	1	Y	32768	31	63
def	test	t9	t9	c8	c8	5	22	1	Y	32768	31	63
def	test	t9	t9	c9	c9	5	22	1	Y	32768	31	63
def	test	t9	t9	c10	c10	5	22	1	Y	32768	31	63
def	test	t9	t9	c11	c11	0	9	6	Y	32768	4	63
def	test	t9	t9	c12	c12	0	10	6	Y	32768	4	63
def	test	t9	t9	c13	c13	10	10	10	Y	128	0	63
def	test	t9	t9	c14	c14	12	19	19	Y	128	0	63
def	test	t9	t9	c15	c15	7	19	19	N	1249	0	63
def	test	t9	t9	c16	c16	11	8	8	Y	128	0	63
def	test	t9	t9	c17	c17	13	4	4	Y	32864	0	63
def	test	t9	t9	c18	c18	1	1	1	Y	32768	0	63
def	test	t9	t9	c19	c19	1	1	1	Y	32768	0	63
def	test	t9	t9	c20	c20	254	1	1	Y	0	0	8
def	test	t9	t9	c21	c21	253	10	10	Y	0	0	8
def	test	t9	t9	c22	c22	253	30	30	Y	0	0	8
def	test	t9	t9	c23	c23	252	255	8	Y	144	0	63
def	test	t9	t9	c24	c24	252	255	8	Y	16	0	8
def	test	t9	t9	c25	c25	252	65535	4	Y	144	0	63
def	test	t9	t9	c26	c26	252	65535	4	Y	16	0	8
def	test	t9	t9	c27	c27	252	16777215	10	Y	144	0	63
def	test	t9	t9	c28	c28	252	16777215	10	Y	16	0	8
def	test	t9	t9	c29	c29	252	16777215	8	Y	144	0	63
def	test	t9	t9	c30	c30	252	16777215	8	Y	16	0	8
def	test	t9	t9	c31	c31	254	5	3	Y	256	0	8
def	test	t9	t9	c32	c32	254	24	7	Y	2048	0	8
c1	c2	c3	c4	c5	c6	c7	c8	c9	c10	c11	c12	c13	c14	c15	c16	c17	c18	c19	c20	c21	c22	c23	c24	c25	c26	c27	c28	c29	c30	c31	c32
1	1	1	1	1	1	1	1	1	1	1.0000	1.0000	2004-02-29	2004-02-29 11:11:11	2004-02-29 11:11:11	11:11:11	2004	1	1	a	123456789a	123456789a123456789b123456789c	tinyblob	tinytext	blob	text	mediumblob	mediumtext	longblob	longtext	one	monday
9	9	9	9	9	9	9	9	9	9	9.0000	9.0000	2004-02-29	2004-02-29 11:11:11	2004-02-29 11:11:11	11:11:11	2004	1	0	a	123456789a	123456789a123456789b123456789c	tinyblob	tinytext	blob	text	mediumblob	mediumtext	longblob	longtext	two	tuesday
set @arg00='SELECT' ;
prepare stmt1 from ' ? a from t1 where a=1 ';
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '? a from t1 where a=1' at line 1
set @arg00=1 ;
select @arg00, b from t1 where a=1 ;
@arg00	b
1	one
prepare stmt1 from ' select ?, b from t1 where a=1 ' ;
execute stmt1 using @arg00 ;
?	b
1	one
set @arg00='lion' ;
select @arg00, b from t1 where a=1 ;
@arg00	b
lion	one
prepare stmt1 from ' select ?, b from t1 where a=1 ' ;
execute stmt1 using @arg00 ;
?	b
lion	one
set @arg00=NULL ;
select @arg00, b from t1 where a=1 ;
@arg00	b
NULL	one
prepare stmt1 from ' select ?, b from t1 where a=1 ' ;
execute stmt1 using @arg00 ;
?	b
NULL	one
set @arg00=1 ;
select b, a - @arg00 from t1 where a=1 ;
b	a - @arg00
one	0
prepare stmt1 from ' select b, a - ? from t1 where a=1 ' ;
execute stmt1 using @arg00 ;
b	a - ?
one	0
set @arg00=null ;
select @arg00 as my_col ;
my_col
NULL
prepare stmt1 from ' select ? as my_col';
execute stmt1 using @arg00 ;
my_col
NULL
select @arg00 + 1 as my_col ;
my_col
NULL
prepare stmt1 from ' select ? + 1 as my_col';
execute stmt1 using @arg00 ;
my_col
NULL
select 1 + @arg00 as my_col ;
my_col
NULL
prepare stmt1 from ' select 1 + ? as my_col';
execute stmt1 using @arg00 ;
my_col
NULL
set @arg00='MySQL' ;
select substr(@arg00,1,2) from t1 where a=1 ;
substr(@arg00,1,2)
My
prepare stmt1 from ' select substr(?,1,2) from t1 where a=1 ' ;
execute stmt1 using @arg00 ;
substr(?,1,2)
My
set @arg00=3 ;
select substr('MySQL',@arg00,5) from t1 where a=1 ;
substr('MySQL',@arg00,5)
SQL
prepare stmt1 from ' select substr(''MySQL'',?,5) from t1 where a=1 ' ;
execute stmt1 using @arg00 ;
substr('MySQL',?,5)
SQL
select substr('MySQL',1,@arg00) from t1 where a=1 ;
substr('MySQL',1,@arg00)
MyS
prepare stmt1 from ' select substr(''MySQL'',1,?) from t1 where a=1 ' ;
execute stmt1 using @arg00 ;
substr('MySQL',1,?)
MyS
set @arg00='MySQL' ;
select a , concat(@arg00,b) from t1 order by a;
a	concat(@arg00,b)
1	MySQLone
2	MySQLtwo
3	MySQLthree
4	MySQLfour
prepare stmt1 from ' select a , concat(?,b) from t1 order by a ' ;
execute stmt1 using @arg00;
a	concat(?,b)
1	MySQLone
2	MySQLtwo
3	MySQLthree
4	MySQLfour
select a , concat(b,@arg00) from t1 order by a ;
a	concat(b,@arg00)
1	oneMySQL
2	twoMySQL
3	threeMySQL
4	fourMySQL
prepare stmt1 from ' select a , concat(b,?) from t1 order by a ' ;
execute stmt1 using @arg00;
a	concat(b,?)
1	oneMySQL
2	twoMySQL
3	threeMySQL
4	fourMySQL
set @arg00='MySQL' ;
select group_concat(@arg00,b order by a) from t1 
group by 'a' ;
group_concat(@arg00,b order by a)
MySQLone,MySQLtwo,MySQLthree,MySQLfour
prepare stmt1 from ' select group_concat(?,b order by a) from t1
group by ''a'' ' ;
execute stmt1 using @arg00;
group_concat(?,b order by a)
MySQLone,MySQLtwo,MySQLthree,MySQLfour
select group_concat(b,@arg00 order by a) from t1 
group by 'a' ;
group_concat(b,@arg00 order by a)
oneMySQL,twoMySQL,threeMySQL,fourMySQL
prepare stmt1 from ' select group_concat(b,? order by a) from t1
group by ''a'' ' ;
execute stmt1 using @arg00;
group_concat(b,? order by a)
oneMySQL,twoMySQL,threeMySQL,fourMySQL
set @arg00='first' ;
set @arg01='second' ;
set @arg02=NULL;
select @arg00, @arg01 from t1 where a=1 ;
@arg00	@arg01
first	second
prepare stmt1 from ' select ?, ? from t1 where a=1 ' ;
execute stmt1 using @arg00, @arg01 ;
?	?
first	second
execute stmt1 using @arg02, @arg01 ;
?	?
NULL	second
execute stmt1 using @arg00, @arg02 ;
?	?
first	NULL
execute stmt1 using @arg02, @arg02 ;
?	?
NULL	NULL
drop table if exists t5 ;
create table t5 (id1 int(11) not null default '0',
value2 varchar(100), value1 varchar(100)) ;
insert into t5 values (1,'hh','hh'),(2,'hh','hh'),
(1,'ii','ii'),(2,'ii','ii') ;
prepare stmt1 from ' select id1,value1 from t5 where id1=? or value1=? order by id1,value1 ' ;
set @arg00=1 ;
set @arg01='hh' ;
execute stmt1 using @arg00, @arg01 ;
id1	value1
1	hh
1	ii
2	hh
drop table t5 ;
drop table if exists t5 ;
create table t5(session_id  char(9) not null) ;
insert into t5 values ('abc') ;
prepare stmt1 from ' select * from t5
where ?=''1111'' and session_id = ''abc'' ' ;
set @arg00='abc' ;
execute stmt1 using @arg00 ;
session_id
set @arg00='1111' ;
execute stmt1 using @arg00 ;
session_id
abc
set @arg00='abc' ;
execute stmt1 using @arg00 ;
session_id
drop table t5 ;
set @arg00='FROM' ;
select a @arg00 t1 where a=1 ;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '@arg00 t1 where a=1' at line 1
prepare stmt1 from ' select a ? t1 where a=1 ' ;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '? t1 where a=1' at line 1
set @arg00='t1' ;
select a from @arg00 where a=1 ;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '@arg00 where a=1' at line 1
prepare stmt1 from ' select a from ? where a=1 ' ;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '? where a=1' at line 1
set @arg00='WHERE' ;
select a from t1 @arg00 a=1 ;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '@arg00 a=1' at line 1
prepare stmt1 from ' select a from t1 ? a=1 ' ;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '? a=1' at line 1
set @arg00=1 ;
select a FROM t1 where a=@arg00 ;
a
1
prepare stmt1 from ' select a FROM t1 where a=? ' ;
execute stmt1 using @arg00 ;
a
1
set @arg00=1000 ;
execute stmt1 using @arg00 ;
a
set @arg00=NULL ;
select a FROM t1 where a=@arg00 ;
a
prepare stmt1 from ' select a FROM t1 where a=? ' ;
execute stmt1 using @arg00 ;
a
set @arg00=4 ;
select a FROM t1 where a=sqrt(@arg00) ;
a
2
prepare stmt1 from ' select a FROM t1 where a=sqrt(?) ' ;
execute stmt1 using @arg00 ;
a
2
set @arg00=NULL ;
select a FROM t1 where a=sqrt(@arg00) ;
a
prepare stmt1 from ' select a FROM t1 where a=sqrt(?) ' ;
execute stmt1 using @arg00 ;
a
set @arg00=2 ;
set @arg01=3 ;
select a FROM t1 where a in (@arg00,@arg01) order by a;
a
2
3
prepare stmt1 from ' select a FROM t1 where a in (?,?) order by a ';
execute stmt1 using @arg00, @arg01;
a
2
3
set @arg00= 'one' ;
set @arg01= 'two' ;
set @arg02= 'five' ;
prepare stmt1 from ' select b FROM t1 where b in (?,?,?) order by b ' ;
execute stmt1 using @arg00, @arg01, @arg02 ;
b
one
two
prepare stmt1 from ' select b FROM t1 where b like ? ';
set @arg00='two' ;
execute stmt1 using @arg00 ;
b
two
set @arg00='tw%' ;
execute stmt1 using @arg00 ;
b
two
set @arg00='%wo' ;
execute stmt1 using @arg00 ;
b
two
set @arg00=null ;
insert into t9 set c1= 0, c5 = NULL ;
select c5 from t9 where c5 > NULL ;
c5
prepare stmt1 from ' select c5 from t9 where c5 > ? ';
execute stmt1 using @arg00 ;
c5
select c5 from t9 where c5 < NULL ;
c5
prepare stmt1 from ' select c5 from t9 where c5 < ? ';
execute stmt1 using @arg00 ;
c5
select c5 from t9 where c5 = NULL ;
c5
prepare stmt1 from ' select c5 from t9 where c5 = ? ';
execute stmt1 using @arg00 ;
c5
select c5 from t9 where c5 <=> NULL ;
c5
NULL
prepare stmt1 from ' select c5 from t9 where c5 <=> ? ';
execute stmt1 using @arg00 ;
c5
NULL
delete from t9 where c1= 0 ;
set @arg00='>' ;
select a FROM t1 where a @arg00 1 ;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '@arg00 1' at line 1
prepare stmt1 from ' select a FROM t1 where a ? 1 ' ;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '? 1' at line 1
set @arg00=1 ;
select a,b FROM t1 where a is not NULL
AND b is not NULL group by a - @arg00 ;
a	b
1	one
2	two
3	three
4	four
prepare stmt1 from ' select a,b FROM t1 where a is not NULL
AND b is not NULL group by a - ? ' ;
execute stmt1 using @arg00 ;
a	b
1	one
2	two
3	three
4	four
set @arg00='two' ;
select a,b FROM t1 where a is not NULL
AND b is not NULL having b <> @arg00 order by a ;
a	b
1	one
3	three
4	four
prepare stmt1 from ' select a,b FROM t1 where a is not NULL
AND b is not NULL having b <> ? order by a ' ;
execute stmt1 using @arg00 ;
a	b
1	one
3	three
4	four
set @arg00=1 ;
select a,b FROM t1 where a is not NULL
AND b is not NULL order by a - @arg00 ;
a	b
1	one
2	two
3	three
4	four
prepare stmt1 from ' select a,b FROM t1 where a is not NULL
AND b is not NULL order by a - ? ' ;
execute stmt1 using @arg00 ;
a	b
1	one
2	two
3	three
4	four
set @arg00=2 ;
select a,b from t1 order by 2 ;
a	b
4	four
1	one
3	three
2	two
prepare stmt1 from ' select a,b from t1
order by ? ';
execute stmt1 using @arg00;
a	b
4	four
1	one
3	three
2	two
set @arg00=1 ;
execute stmt1 using @arg00;
a	b
1	one
2	two
3	three
4	four
set @arg00=0 ;
execute stmt1 using @arg00;
ERROR 42S22: Unknown column '?' in 'order clause'
set @arg00=1;
prepare stmt1 from ' select a,b from t1 order by a
limit 1 ';
execute stmt1 ;
a	b
1	one
prepare stmt1 from ' select a,b from t1
limit ? ';
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '?' at line 2
set @arg00='b' ;
set @arg01=0 ;
set @arg02=2 ;
set @arg03=2 ;
select sum(a), @arg00 from t1 where a > @arg01
and b is not null group by substr(b,@arg02)
having sum(a) <> @arg03 ;
sum(a)	@arg00
3	b
1	b
4	b
prepare stmt1 from ' select sum(a), ? from t1 where a > ?
and b is not null group by substr(b,?)
having sum(a) <> ? ';
execute stmt1 using @arg00, @arg01, @arg02, @arg03;
sum(a)	?
3	b
1	b
4	b
test_sequence
------ join tests ------
select first.a as a1, second.a as a2 
from t1 first, t1 second
where first.a = second.a order by a1 ;
a1	a2
1	1
2	2
3	3
4	4
prepare stmt1 from ' select first.a as a1, second.a as a2 
        from t1 first, t1 second
        where first.a = second.a order by a1 ';
execute stmt1 ;
a1	a2
1	1
2	2
3	3
4	4
set @arg00='ABC';
set @arg01='two';
set @arg02='one';
select first.a, @arg00, second.a FROM t1 first, t1 second
where @arg01 = first.b or first.a = second.a or second.b = @arg02
order by second.a, first.a;
a	@arg00	a
1	ABC	1
2	ABC	1
3	ABC	1
4	ABC	1
2	ABC	2
2	ABC	3
3	ABC	3
2	ABC	4
4	ABC	4
prepare stmt1 from ' select first.a, ?, second.a FROM t1 first, t1 second
                    where ? = first.b or first.a = second.a or second.b = ?
                    order by second.a, first.a';
execute stmt1 using @arg00, @arg01, @arg02;
a	?	a
1	ABC	1
2	ABC	1
3	ABC	1
4	ABC	1
2	ABC	2
2	ABC	3
3	ABC	3
2	ABC	4
4	ABC	4
drop table if exists t2 ;
create table t2 as select * from t1 ;
set @query1= 'SELECT * FROM t2 join t1 on (t1.a=t2.a) order by t2.a ' ;
set @query2= 'SELECT * FROM t2 natural join t1 order by t2.a ' ;
set @query3= 'SELECT * FROM t2 join t1 using(a) order by t2.a ' ;
set @query4= 'SELECT * FROM t2 left join t1 on(t1.a=t2.a) order by t2.a ' ;
set @query5= 'SELECT * FROM t2 natural left join t1 order by t2.a ' ;
set @query6= 'SELECT * FROM t2 left join t1 using(a) order by t2.a ' ;
set @query7= 'SELECT * FROM t2 right join t1 on(t1.a=t2.a) order by t2.a ' ;
set @query8= 'SELECT * FROM t2 natural right join t1 order by t2.a ' ;
set @query9= 'SELECT * FROM t2 right join t1 using(a) order by t2.a ' ;
the join statement is:
SELECT * FROM t2 right join t1 using(a) order by t2.a 
prepare stmt1 from @query9  ;
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
the join statement is:
SELECT * FROM t2 natural right join t1 order by t2.a 
prepare stmt1 from @query8 ;
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
the join statement is:
SELECT * FROM t2 right join t1 on(t1.a=t2.a) order by t2.a 
prepare stmt1 from @query7 ;
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
the join statement is:
SELECT * FROM t2 left join t1 using(a) order by t2.a 
prepare stmt1 from @query6 ;
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
the join statement is:
SELECT * FROM t2 natural left join t1 order by t2.a 
prepare stmt1 from @query5 ;
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
the join statement is:
SELECT * FROM t2 left join t1 on(t1.a=t2.a) order by t2.a 
prepare stmt1 from @query4 ;
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
the join statement is:
SELECT * FROM t2 join t1 using(a) order by t2.a 
prepare stmt1 from @query3 ;
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
the join statement is:
SELECT * FROM t2 natural join t1 order by t2.a 
prepare stmt1 from @query2 ;
execute stmt1 ;
a	b
1	one
2	two
3	three
4	four
execute stmt1 ;
a	b
1	one
2	two
3	three
4	four
execute stmt1 ;
a	b
1	one
2	two
3	three
4	four
the join statement is:
SELECT * FROM t2 join t1 on (t1.a=t2.a) order by t2.a 
prepare stmt1 from @query1 ;
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
drop table t2 ;
test_sequence
------ subquery tests ------
prepare stmt1 from ' select a, b FROM t1 outer_table where
   a = (select a from t1 where b = ''two'') ';
execute stmt1 ;
a	b
2	two
set @arg00='two' ;
select a, b FROM t1 outer_table where
a = (select a from t1 where b = 'two' ) and b=@arg00 ;
a	b
2	two
prepare stmt1 from ' select a, b FROM t1 outer_table where
   a = (select a from t1 where b = ''two'') and b=? ';
execute stmt1 using @arg00;
a	b
2	two
set @arg00='two' ;
select a, b FROM t1 outer_table where
a = (select a from t1 where b = @arg00 ) and b='two' ;
a	b
2	two
prepare stmt1 from ' select a, b FROM t1 outer_table where
   a = (select a from t1 where b = ? ) and b=''two'' ' ;
execute stmt1 using @arg00;
a	b
2	two
set @arg00=3 ;
set @arg01='three' ;
select a,b FROM t1 where (a,b) in (select 3, 'three');
a	b
3	three
select a FROM t1 where (a,b) in (select @arg00,@arg01);
a
3
prepare stmt1 from ' select a FROM t1 where (a,b) in (select ?, ?) ';
execute stmt1 using @arg00, @arg01;
a
3
set @arg00=1 ;
set @arg01='two' ;
set @arg02=2 ;
set @arg03='two' ;
select a, @arg00, b FROM t1 outer_table where
b=@arg01 and a = (select @arg02 from t1 where b = @arg03 ) ;
a	@arg00	b
2	1	two
prepare stmt1 from ' select a, ?, b FROM t1 outer_table where
   b=? and a = (select ? from t1 where b = ? ) ' ;
execute stmt1 using @arg00, @arg01, @arg02, @arg03 ;
a	?	b
2	1	two
prepare stmt1 from ' select a, b FROM t1 outer_table where
   a = (select a from t1 where b = outer_table.b ) order by a ';
execute stmt1 ;
a	b
1	one
2	two
3	three
4	four
prepare stmt1 from ' SELECT a as ccc from t1 where a+1=
                           (SELECT 1+ccc from t1 where ccc+1=a+1 and a=1) ';
execute stmt1 ;
ccc
1
deallocate prepare stmt1 ;
prepare stmt1 from ' SELECT a as ccc from t1 where a+1=
                           (SELECT 1+ccc from t1 where ccc+1=a+1 and a=1) ';
execute stmt1 ;
ccc
1
deallocate prepare stmt1 ;
prepare stmt1 from ' SELECT a as ccc from t1 where a+1=
                           (SELECT 1+ccc from t1 where ccc+1=a+1 and a=1) ';
execute stmt1 ;
ccc
1
deallocate prepare stmt1 ;
set @arg00='two' ;
select a, b FROM t1 outer_table where
a = (select a from t1 where b = outer_table.b ) and b=@arg00 ;
a	b
2	two
prepare stmt1 from ' select a, b FROM t1 outer_table where
   a = (select a from t1 where b = outer_table.b) and b=? ';
execute stmt1 using @arg00;
a	b
2	two
set @arg00=2 ;
select a, b FROM t1 outer_table where
a = (select a from t1 where a = @arg00 and b = outer_table.b) and b='two' ;
a	b
2	two
prepare stmt1 from ' select a, b FROM t1 outer_table where
   a = (select a from t1 where a = ? and b = outer_table.b) and b=''two'' ' ;
execute stmt1 using @arg00;
a	b
2	two
set @arg00=2 ;
select a, b FROM t1 outer_table where
a = (select a from t1 where outer_table.a = @arg00 and a=2) and b='two' ;
a	b
2	two
prepare stmt1 from ' select a, b FROM t1 outer_table where
   a = (select a from t1 where outer_table.a = ? and a=2) and b=''two'' ' ;
execute stmt1 using @arg00;
a	b
2	two
set @arg00=1 ;
set @arg01='two' ;
set @arg02=2 ;
set @arg03='two' ;
select a, @arg00, b FROM t1 outer_table where
b=@arg01 and a = (select @arg02 from t1 where outer_table.b = @arg03
and outer_table.a=a ) ;
a	@arg00	b
2	1	two
prepare stmt1 from ' select a, ?, b FROM t1 outer_table where
   b=? and a = (select ? from t1 where outer_table.b = ? 
                   and outer_table.a=a ) ' ;
execute stmt1 using @arg00, @arg01, @arg02, @arg03 ;
a	?	b
2	1	two
set @arg00=1 ;
set @arg01=0 ;
select a, @arg00 
from ( select a - @arg00 as a from t1 where a=@arg00 ) as t2
where a=@arg01;
a	@arg00
0	1
prepare stmt1 from ' select a, ? 
                    from ( select a - ? as a from t1 where a=? ) as t2
                    where a=? ';
execute stmt1 using @arg00, @arg00, @arg00, @arg01 ;
a	?
0	1
drop table if exists t2 ;
create table t2 as select * from t1;
prepare stmt1 from ' select a in (select a from t2) from t1 ' ;
execute stmt1 ;
a in (select a from t2)
1
1
1
1
drop table if exists t5, t6, t7 ;
create table t5 (a int , b int) ;
create table t6 like t5 ;
create table t7 like t5 ;
insert into t5 values (0, 100), (1, 2), (1, 3), (2, 2), (2, 7),
(2, -1), (3, 10) ;
insert into t6 values (0, 0), (1, 1), (2, 1), (3, 1), (4, 1) ;
insert into t7 values (3, 3), (2, 2), (1, 1) ;
prepare stmt1 from ' select a, (select count(distinct t5.b) as sum from t5, t6
                     where t5.a=t6.a and t6.b > 0 and t5.a <= t7.b
                     group by t5.a order by sum limit 1) from t7 ' ;
execute stmt1 ;
a	(select count(distinct t5.b) as sum from t5, t6
                     where t5.a=t6.a and t6.b > 0 and t5.a <= t7.b
                     group by t5.a order by sum limit 1)
3	1
2	2
1	2
execute stmt1 ;
a	(select count(distinct t5.b) as sum from t5, t6
                     where t5.a=t6.a and t6.b > 0 and t5.a <= t7.b
                     group by t5.a order by sum limit 1)
3	1
2	2
1	2
execute stmt1 ;
a	(select count(distinct t5.b) as sum from t5, t6
                     where t5.a=t6.a and t6.b > 0 and t5.a <= t7.b
                     group by t5.a order by sum limit 1)
3	1
2	2
1	2
drop table t5, t6, t7 ;
drop table if exists t2 ;
create table t2 as select * from t9;
set @stmt= ' SELECT
   (SELECT SUM(c1 + c12 + 0.0) FROM t2 
    where (t9.c2 - 0e-3) = t2.c2
    GROUP BY t9.c15 LIMIT 1) as scalar_s,
   exists (select 1.0e+0 from t2 
           where t2.c3 * 9.0000000000 = t9.c4) as exists_s,
   c5 * 4 in (select c6 + 0.3e+1 from t2) as in_s,
   (c7 - 4, c8 - 4) in (select c9 + 4.0, c10 + 40e-1 from t2) as in_row_s
FROM t9,
(select c25 x, c32 y from t2) tt WHERE x = c25 ' ;
prepare stmt1 from @stmt ;
execute stmt1 ;
execute stmt1 ;
set @stmt= concat('explain ',@stmt);
prepare stmt1 from @stmt ;
execute stmt1 ;
execute stmt1 ;
set @stmt= ' SELECT
   (SELECT SUM(c1+c12+?) FROM t2 where (t9.c2-?)=t2.c2
    GROUP BY t9.c15 LIMIT 1) as scalar_s,
   exists (select ? from t2 
           where t2.c3*?=t9.c4) as exists_s,
   c5*? in (select c6+? from t2) as in_s,
   (c7-?, c8-?) in (select c9+?, c10+? from t2) as in_row_s
FROM t9,
(select c25 x, c32 y from t2) tt WHERE x =c25 ' ;
set @arg00= 0.0 ;
set @arg01= 0e-3 ;
set @arg02= 1.0e+0 ;
set @arg03= 9.0000000000 ;
set @arg04= 4 ;
set @arg05= 0.3e+1 ;
set @arg06= 4 ;
set @arg07= 4 ;
set @arg08= 4.0 ;
set @arg09= 40e-1 ;
prepare stmt1 from @stmt ;
execute stmt1 using @arg00, @arg01, @arg02, @arg03, @arg04, @arg05, @arg06,
@arg07, @arg08, @arg09 ;
execute stmt1 using @arg00, @arg01, @arg02, @arg03, @arg04, @arg05, @arg06,
@arg07, @arg08, @arg09 ;
set @stmt= concat('explain ',@stmt);
prepare stmt1 from @stmt ;
execute stmt1 using @arg00, @arg01, @arg02, @arg03, @arg04, @arg05, @arg06,
@arg07, @arg08, @arg09 ;
execute stmt1 using @arg00, @arg01, @arg02, @arg03, @arg04, @arg05, @arg06,
@arg07, @arg08, @arg09 ;
drop table t2 ;
select 1 < (select a from t1) ;
ERROR 21000: Subquery returns more than 1 row
prepare stmt1 from ' select 1 < (select a from t1) ' ;
execute stmt1 ;
ERROR 21000: Subquery returns more than 1 row
select 1 as my_col ;
my_col
1
test_sequence
------ union tests ------
prepare stmt1 from ' select a FROM t1 where a=1
                     union distinct
                     select a FROM t1 where a=1 ';
execute stmt1 ;
a
1
execute stmt1 ;
a
1
prepare stmt1 from ' select a FROM t1 where a=1
                     union all
                     select a FROM t1 where a=1 ';
execute stmt1 ;
a
1
1
prepare stmt1 from ' SELECT 1, 2 union SELECT 1 ' ;
ERROR 21000: The used SELECT statements have a different number of columns
prepare stmt1 from ' SELECT 1 union SELECT 1, 2 ' ;
ERROR 21000: The used SELECT statements have a different number of columns
prepare stmt1 from ' SELECT * from t1 union SELECT 1 ' ;
ERROR 21000: The used SELECT statements have a different number of columns
prepare stmt1 from ' SELECT 1 union SELECT * from t1 ' ;
ERROR 21000: The used SELECT statements have a different number of columns
set @arg00=1 ;
select @arg00 FROM t1 where a=1
union distinct
select 1 FROM t1 where a=1;
@arg00
1
prepare stmt1 from ' select ? FROM t1 where a=1
                     union distinct
                     select 1 FROM t1 where a=1 ' ;
execute stmt1 using @arg00;
?
1
set @arg00=1 ;
select 1 FROM t1 where a=1
union distinct
select @arg00 FROM t1 where a=1;
1
1
prepare stmt1 from ' select 1 FROM t1 where a=1
                     union distinct
                     select ? FROM t1 where a=1 ' ;
execute stmt1 using @arg00;
1
1
set @arg00='a' ;
select @arg00 FROM t1 where a=1
union distinct
select @arg00 FROM t1 where a=1;
@arg00
a
prepare stmt1 from ' select ? FROM t1 where a=1
                     union distinct
                     select ? FROM t1 where a=1 ';
execute stmt1 using @arg00, @arg00;
?
a
prepare stmt1 from ' select ? 
                     union distinct
                     select ? ';
execute stmt1 using @arg00, @arg00;
?
a
set @arg00='a' ;
set @arg01=1 ;
set @arg02='a' ;
set @arg03=2 ;
select @arg00 FROM t1 where a=@arg01
union distinct
select @arg02 FROM t1 where a=@arg03;
@arg00
a
prepare stmt1 from ' select ? FROM t1 where a=?
                     union distinct
                     select ? FROM t1 where a=? ' ;
execute stmt1 using @arg00, @arg01, @arg02, @arg03;
?
a
set @arg00=1 ;
prepare stmt1 from ' select sum(a) + 200, ? from t1
union distinct
select sum(a) + 200, 1 from t1
group by b ' ;
execute stmt1 using @arg00;
sum(a) + 200	?
210	1
204	1
201	1
203	1
202	1
set @Oporto='Oporto' ;
set @Lisboa='Lisboa' ;
set @0=0 ;
set @1=1 ;
set @2=2 ;
set @3=3 ;
set @4=4 ;
select @Oporto,@Lisboa,@0,@1,@2,@3,@4 ;
@Oporto	@Lisboa	@0	@1	@2	@3	@4
Oporto	Lisboa	0	1	2	3	4
select sum(a) + 200 as the_sum, @Oporto as the_town from t1
group by b
union distinct
select sum(a) + 200, @Lisboa from t1
group by b ;
the_sum	the_town
204	Oporto
201	Oporto
203	Oporto
202	Oporto
204	Lisboa
201	Lisboa
203	Lisboa
202	Lisboa
prepare stmt1 from ' select sum(a) + 200 as the_sum, ? as the_town from t1
                     group by b
                     union distinct
                     select sum(a) + 200, ? from t1
                     group by b ' ;
execute stmt1 using @Oporto, @Lisboa;
the_sum	the_town
204	Oporto
201	Oporto
203	Oporto
202	Oporto
204	Lisboa
201	Lisboa
203	Lisboa
202	Lisboa
select sum(a) + 200 as the_sum, @Oporto as the_town from t1
where a > @1
group by b
union distinct
select sum(a) + 200, @Lisboa from t1
where a > @2
group by b ;
the_sum	the_town
204	Oporto
203	Oporto
202	Oporto
204	Lisboa
203	Lisboa
prepare stmt1 from ' select sum(a) + 200 as the_sum, ? as the_town from t1
                     where a > ?
                     group by b
                     union distinct
                     select sum(a) + 200, ? from t1
                     where a > ?
                     group by b ' ;
execute stmt1 using @Oporto, @1, @Lisboa, @2;
the_sum	the_town
204	Oporto
203	Oporto
202	Oporto
204	Lisboa
203	Lisboa
select sum(a) + 200 as the_sum, @Oporto as the_town from t1
where a > @1
group by b
having avg(a) > @2
union distinct
select sum(a) + 200, @Lisboa from t1
where a > @2
group by b 
having avg(a) > @3;
the_sum	the_town
204	Oporto
203	Oporto
204	Lisboa
prepare stmt1 from ' select sum(a) + 200 as the_sum, ? as the_town from t1
                     where a > ?
                     group by b
                     having avg(a) > ?
                     union distinct
                     select sum(a) + 200, ? from t1
                     where a > ?
                     group by b
                     having avg(a) > ? ';
execute stmt1 using @Oporto, @1, @2, @Lisboa, @2, @3;
the_sum	the_town
204	Oporto
203	Oporto
204	Lisboa
test_sequence
------ explain select tests ------
prepare stmt1 from ' explain select * from t9 ' ;
execute stmt1;
Catalog	Database	Table	Table_alias	Column	Column_alias	Name	Type	Length	Max length	Is_null	Flags	Decimals	Charsetnr
def					id	8	3	1	N	32801	0	8
def					select_type	253	19	6	N	1	31	33
def					table	253	64	2	N	1	31	33
def					type	253	10	3	N	1	31	33
def					possible_keys	253	4096	0	Y	0	31	33
def					key	253	64	0	Y	0	31	33
def					key_len	8	3	0	Y	32800	0	8
def					ref	253	1024	0	Y	0	31	33
def					rows	8	10	1	N	32801	0	8
def					Extra	253	255	0	N	1	31	33
id	select_type	table	type	possible_keys	key	key_len	ref	rows	Extra
1	SIMPLE	t9	ALL	NULL	NULL	NULL	NULL	2	
test_sequence
------ delete tests ------
delete from t1 ;
insert into t1 values (1,'one');
insert into t1 values (2,'two');
insert into t1 values (3,'three');
insert into t1 values (4,'four');
commit ;
delete from t9 ;
insert into t9
set c1= 1, c2= 1, c3= 1, c4= 1, c5= 1, c6= 1, c7= 1, c8= 1, c9= 1,
c10= 1, c11= 1, c12 = 1,
c13= '2004-02-29', c14= '2004-02-29 11:11:11', c15= '2004-02-29 11:11:11',
c16= '11:11:11', c17= '2004',
c18= 1, c19=true, c20= 'a', c21= '123456789a', 
c22= '123456789a123456789b123456789c', c23= 'tinyblob', c24= 'tinytext',
c25= 'blob', c26= 'text', c27= 'mediumblob', c28= 'mediumtext',
c29= 'longblob', c30= 'longtext', c31='one', c32= 'monday';
insert into t9
set c1= 9, c2= 9, c3= 9, c4= 9, c5= 9, c6= 9, c7= 9, c8= 9, c9= 9,
c10= 9, c11= 9, c12 = 9,
c13= '2004-02-29', c14= '2004-02-29 11:11:11', c15= '2004-02-29 11:11:11',
c16= '11:11:11', c17= '2004',
c18= 1, c19=false, c20= 'a', c21= '123456789a', 
c22= '123456789a123456789b123456789c', c23= 'tinyblob', c24= 'tinytext',
c25= 'blob', c26= 'text', c27= 'mediumblob', c28= 'mediumtext',
c29= 'longblob', c30= 'longtext', c31='two', c32= 'tuesday';
commit ;
prepare stmt1 from 'delete from t1 where a=2' ;
execute stmt1;
select a,b from t1 where a=2;
a	b
execute stmt1;
insert into t1 values(0,NULL);
set @arg00=NULL;
prepare stmt1 from 'delete from t1 where b=?' ;
execute stmt1 using @arg00;
select a,b from t1 where b is NULL ;
a	b
0	NULL
set @arg00='one';
execute stmt1 using @arg00;
select a,b from t1 where b=@arg00;
a	b
prepare stmt1 from 'truncate table t1' ;
ERROR HY000: This command is not supported in the prepared statement protocol yet
test_sequence
------ update tests ------
delete from t1 ;
insert into t1 values (1,'one');
insert into t1 values (2,'two');
insert into t1 values (3,'three');
insert into t1 values (4,'four');
commit ;
delete from t9 ;
insert into t9
set c1= 1, c2= 1, c3= 1, c4= 1, c5= 1, c6= 1, c7= 1, c8= 1, c9= 1,
c10= 1, c11= 1, c12 = 1,
c13= '2004-02-29', c14= '2004-02-29 11:11:11', c15= '2004-02-29 11:11:11',
c16= '11:11:11', c17= '2004',
c18= 1, c19=true, c20= 'a', c21= '123456789a', 
c22= '123456789a123456789b123456789c', c23= 'tinyblob', c24= 'tinytext',
c25= 'blob', c26= 'text', c27= 'mediumblob', c28= 'mediumtext',
c29= 'longblob', c30= 'longtext', c31='one', c32= 'monday';
insert into t9
set c1= 9, c2= 9, c3= 9, c4= 9, c5= 9, c6= 9, c7= 9, c8= 9, c9= 9,
c10= 9, c11= 9, c12 = 9,
c13= '2004-02-29', c14= '2004-02-29 11:11:11', c15= '2004-02-29 11:11:11',
c16= '11:11:11', c17= '2004',
c18= 1, c19=false, c20= 'a', c21= '123456789a', 
c22= '123456789a123456789b123456789c', c23= 'tinyblob', c24= 'tinytext',
c25= 'blob', c26= 'text', c27= 'mediumblob', c28= 'mediumtext',
c29= 'longblob', c30= 'longtext', c31='two', c32= 'tuesday';
commit ;
prepare stmt1 from 'update t1 set b=''a=two'' where a=2' ;
execute stmt1;
select a,b from t1 where a=2;
a	b
2	a=two
execute stmt1;
select a,b from t1 where a=2;
a	b
2	a=two
set @arg00=NULL;
prepare stmt1 from 'update t1 set b=? where a=2' ;
execute stmt1 using @arg00;
select a,b from t1 where a=2;
a	b
2	NULL
set @arg00='two';
execute stmt1 using @arg00;
select a,b from t1 where a=2;
a	b
2	two
set @arg00=2;
prepare stmt1 from 'update t1 set b=NULL where a=?' ;
execute stmt1 using @arg00;
select a,b from t1 where a=@arg00;
a	b
2	NULL
update t1 set b='two' where a=@arg00;
set @arg00=2000;
execute stmt1 using @arg00;
select a,b from t1 where a=@arg00;
a	b
set @arg00=2;
set @arg01=22;
prepare stmt1 from 'update t1 set a=? where a=?' ;
execute stmt1 using @arg00, @arg00;
select a,b from t1 where a=@arg00;
a	b
2	two
execute stmt1 using @arg01, @arg00;
select a,b from t1 where a=@arg01;
a	b
22	two
execute stmt1 using @arg00, @arg01;
select a,b from t1 where a=@arg00;
a	b
2	two
set @arg00=NULL;
set @arg01=2;
execute stmt1 using @arg00, @arg01;
Warnings:
Warning	1263	Data truncated; NULL supplied to NOT NULL column 'a' at row 1
select a,b from t1 order by a;
a	b
0	two
1	one
3	three
4	four
set @arg00=0;
execute stmt1 using @arg01, @arg00;
select a,b from t1 order by a;
a	b
1	one
2	two
3	three
4	four
set @arg00=23;
set @arg01='two';
set @arg02=2;
set @arg03='two';
set @arg04=2;
drop table if exists t2;
create table t2 as select a,b from t1 ;
prepare stmt1 from 'update t1 set a=? where b=?
                    and a in (select ? from t2
                              where b = ? or a = ?)';
execute stmt1 using @arg00, @arg01, @arg02, @arg03, @arg04 ;
affected rows: 1
info: Rows matched: 1  Changed: 1  Warnings: 0
select a,b from t1 where a = @arg00 ;
a	b
23	two
prepare stmt1 from 'update t1 set a=? where b=?
                    and a not in (select ? from t2
                              where b = ? or a = ?)';
execute stmt1 using @arg04, @arg01, @arg02, @arg03, @arg00 ;
affected rows: 1
info: Rows matched: 1  Changed: 1  Warnings: 0
select a,b from t1 order by a ;
a	b
1	one
2	two
3	three
4	four
drop table t2 ;
create table t2
(
a int, b varchar(30),
primary key(a)
) engine = 'MYISAM'  ;
insert into t2(a,b) select a, b from t1 ;
prepare stmt1 from 'update t1 set a=? where b=?
                    and a in (select ? from t2
                              where b = ? or a = ?)';
execute stmt1 using @arg00, @arg01, @arg02, @arg03, @arg04 ;
affected rows: 1
info: Rows matched: 1  Changed: 1  Warnings: 0
select a,b from t1 where a = @arg00 ;
a	b
23	two
prepare stmt1 from 'update t1 set a=? where b=?
                    and a not in (select ? from t2
                              where b = ? or a = ?)';
execute stmt1 using @arg04, @arg01, @arg02, @arg03, @arg00 ;
affected rows: 1
info: Rows matched: 1  Changed: 1  Warnings: 0
select a,b from t1 order by a ;
a	b
1	one
2	two
3	three
4	four
drop table t2 ;
set @arg00=1;
prepare stmt1 from 'update t1 set b=''bla''
where a=2
limit 1';
execute stmt1 ;
select a,b from t1 where b = 'bla' ;
a	b
2	bla
prepare stmt1 from 'update t1 set b=''bla''
where a=2
limit ?';
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '?' at line 3
test_sequence
------ insert tests ------
delete from t1 ;
insert into t1 values (1,'one');
insert into t1 values (2,'two');
insert into t1 values (3,'three');
insert into t1 values (4,'four');
commit ;
delete from t9 ;
insert into t9
set c1= 1, c2= 1, c3= 1, c4= 1, c5= 1, c6= 1, c7= 1, c8= 1, c9= 1,
c10= 1, c11= 1, c12 = 1,
c13= '2004-02-29', c14= '2004-02-29 11:11:11', c15= '2004-02-29 11:11:11',
c16= '11:11:11', c17= '2004',
c18= 1, c19=true, c20= 'a', c21= '123456789a', 
c22= '123456789a123456789b123456789c', c23= 'tinyblob', c24= 'tinytext',
c25= 'blob', c26= 'text', c27= 'mediumblob', c28= 'mediumtext',
c29= 'longblob', c30= 'longtext', c31='one', c32= 'monday';
insert into t9
set c1= 9, c2= 9, c3= 9, c4= 9, c5= 9, c6= 9, c7= 9, c8= 9, c9= 9,
c10= 9, c11= 9, c12 = 9,
c13= '2004-02-29', c14= '2004-02-29 11:11:11', c15= '2004-02-29 11:11:11',
c16= '11:11:11', c17= '2004',
c18= 1, c19=false, c20= 'a', c21= '123456789a', 
c22= '123456789a123456789b123456789c', c23= 'tinyblob', c24= 'tinytext',
c25= 'blob', c26= 'text', c27= 'mediumblob', c28= 'mediumtext',
c29= 'longblob', c30= 'longtext', c31='two', c32= 'tuesday';
commit ;
prepare stmt1 from 'insert into t1 values(5, ''five'' )';
execute stmt1;
select a,b from t1 where a = 5;
a	b
5	five
set @arg00='six' ;
prepare stmt1 from 'insert into t1 values(6, ? )';
execute stmt1 using @arg00;
select a,b from t1 where b = @arg00;
a	b
6	six
execute stmt1 using @arg00;
ERROR 23000: Duplicate entry '6' for key 1
set @arg00=NULL ;
prepare stmt1 from 'insert into t1 values(0, ? )';
execute stmt1 using @arg00;
select a,b from t1 where b is NULL;
a	b
0	NULL
set @arg00=8 ;
set @arg01='eight' ;
prepare stmt1 from 'insert into t1 values(?, ? )';
execute stmt1 using @arg00, @arg01 ;
select a,b from t1 where b = @arg01;
a	b
8	eight
set @NULL= null ;
set @arg00= 'abc' ;
execute stmt1 using @NULL, @NULL ;
ERROR 23000: Column 'a' cannot be null
execute stmt1 using @NULL, @NULL ;
ERROR 23000: Column 'a' cannot be null
execute stmt1 using @NULL, @arg00 ;
ERROR 23000: Column 'a' cannot be null
execute stmt1 using @NULL, @arg00 ;
ERROR 23000: Column 'a' cannot be null
set @arg01= 10000 + 2 ;
execute stmt1 using @arg01, @arg00 ;
set @arg01= 10000 + 1 ;
execute stmt1 using @arg01, @arg00 ;
select * from t1 where a > 10000 order by a ;
a	b
10001	abc
10002	abc
delete from t1 where a > 10000 ;
set @arg01= 10000 + 2 ;
execute stmt1 using @arg01, @NULL ;
set @arg01= 10000 + 1 ;
execute stmt1 using @arg01, @NULL ;
select * from t1 where a > 10000 order by a ;
a	b
10001	NULL
10002	NULL
delete from t1 where a > 10000 ;
set @arg01= 10000 + 10 ;
execute stmt1 using @arg01, @arg01 ;
set @arg01= 10000 + 9 ;
execute stmt1 using @arg01, @arg01 ;
set @arg01= 10000 + 8 ;
execute stmt1 using @arg01, @arg01 ;
set @arg01= 10000 + 7 ;
execute stmt1 using @arg01, @arg01 ;
set @arg01= 10000 + 6 ;
execute stmt1 using @arg01, @arg01 ;
set @arg01= 10000 + 5 ;
execute stmt1 using @arg01, @arg01 ;
set @arg01= 10000 + 4 ;
execute stmt1 using @arg01, @arg01 ;
set @arg01= 10000 + 3 ;
execute stmt1 using @arg01, @arg01 ;
set @arg01= 10000 + 2 ;
execute stmt1 using @arg01, @arg01 ;
set @arg01= 10000 + 1 ;
execute stmt1 using @arg01, @arg01 ;
select * from t1 where a > 10000 order by a ;
a	b
10001	10001
10002	10002
10003	10003
10004	10004
10005	10005
10006	10006
10007	10007
10008	10008
10009	10009
10010	10010
delete from t1 where a > 10000 ;
set @arg00=81 ;
set @arg01='8-1' ;
set @arg02=82 ;
set @arg03='8-2' ;
prepare stmt1 from 'insert into t1 values(?,?),(?,?)';
execute stmt1 using @arg00, @arg01, @arg02, @arg03 ;
select a,b from t1 where a in (@arg00,@arg02) ;
a	b
81	8-1
82	8-2
set @arg00=9 ;
set @arg01='nine' ;
prepare stmt1 from 'insert into t1 set a=?, b=? ';
execute stmt1 using @arg00, @arg01 ;
select a,b from t1 where a = @arg00 ;
a	b
9	nine
set @arg00=6 ;
set @arg01=1 ;
prepare stmt1 from 'insert into t1 set a=?, b=''sechs''
                    on duplicate key update a=a + ?, b=concat(b,''modified'') ';
execute stmt1 using @arg00, @arg01;
select * from t1 order by a;
a	b
0	NULL
1	one
2	two
3	three
4	four
5	five
7	sixmodified
8	eight
9	nine
81	8-1
82	8-2
set @arg00=81 ;
set @arg01=1 ;
execute stmt1 using @arg00, @arg01;
ERROR 23000: Duplicate entry '82' for key 1
drop table if exists t2 ;
create table t2 (id int auto_increment primary key) 
ENGINE= 'MYISAM'  ;
prepare stmt1 from ' select last_insert_id() ' ;
insert into t2 values (NULL) ;
execute stmt1 ;
last_insert_id()
1
insert into t2 values (NULL) ;
execute stmt1 ;
last_insert_id()
2
drop table t2 ;
set @1000=1000 ;
set @x1000_2="x1000_2" ;
set @x1000_3="x1000_3" ;
set @x1000="x1000" ;
set @1100=1100 ;
set @x1100="x1100" ;
set @100=100 ;
set @updated="updated" ;
insert into t1 values(1000,'x1000_1') ;
insert into t1 values(@1000,@x1000_2),(@1000,@x1000_3)
on duplicate key update a = a + @100, b = concat(b,@updated) ;
select a,b from t1 where a >= 1000 order by a ;
a	b
1000	x1000_3
1100	x1000_1updated
delete from t1 where a >= 1000 ;
insert into t1 values(1000,'x1000_1') ;
prepare stmt1 from ' insert into t1 values(?,?),(?,?)
               on duplicate key update a = a + ?, b = concat(b,?) ';
execute stmt1 using @1000, @x1000_2, @1000, @x1000_3, @100, @updated ;
select a,b from t1 where a >= 1000 order by a ;
a	b
1000	x1000_3
1100	x1000_1updated
delete from t1 where a >= 1000 ;
insert into t1 values(1000,'x1000_1') ;
execute stmt1 using @1000, @x1000_2, @1100, @x1000_3, @100, @updated ;
select a,b from t1 where a >= 1000 order by a ;
a	b
1200	x1000_1updatedupdated
delete from t1 where a >= 1000 ;
prepare stmt1 from ' replace into t1 (a,b) select 100, ''hundred'' ';
execute stmt1;
execute stmt1;
execute stmt1;
test_sequence
------ multi table tests ------
delete from t1 ;
delete from t9 ;
insert into t1(a,b) values (1, 'one'), (2, 'two'), (3, 'three') ;
insert into t9 (c1,c21)
values (1, 'one'), (2, 'two'), (3, 'three') ;
prepare stmt_delete from " delete t1, t9 
  from t1, t9 where t1.a=t9.c1 and t1.b='updated' ";
prepare stmt_update from " update t1, t9 
  set t1.b='updated', t9.c21='updated'
  where t1.a=t9.c1 and t1.a=? ";
prepare stmt_select1 from " select a, b from t1 order by a" ;
prepare stmt_select2 from " select c1, c21 from t9 order by c1" ;
set @arg00= 1 ;
execute stmt_update using @arg00 ;
execute stmt_delete ;
execute stmt_select1 ;
a	b
2	two
3	three
execute stmt_select2 ;
c1	c21
2	two
3	three
set @arg00= @arg00 + 1 ;
execute stmt_update using @arg00 ;
execute stmt_delete ;
execute stmt_select1 ;
a	b
3	three
execute stmt_select2 ;
c1	c21
3	three
set @arg00= @arg00 + 1 ;
execute stmt_update using @arg00 ;
execute stmt_delete ;
execute stmt_select1 ;
a	b
execute stmt_select2 ;
c1	c21
set @arg00= @arg00 + 1 ;
drop table if exists t5 ;
set @arg01= 8;
set @arg02= 8.0;
set @arg03= 80.00000000000e-1;
set @arg04= 'abc' ;
set @arg05= CAST('abc' as binary) ;
set @arg06= '1991-08-05' ;
set @arg07= CAST('1991-08-05' as date);
set @arg08= '1991-08-05 01:01:01' ;
set @arg09= CAST('1991-08-05 01:01:01' as datetime) ;
set @arg10= unix_timestamp('1991-01-01 01:01:01');
set @arg11= YEAR('1991-01-01 01:01:01');
set @arg12= 8 ;
set @arg12= NULL ;
set @arg13= 8.0 ;
set @arg13= NULL ;
set @arg14= 'abc';
set @arg14= NULL ;
set @arg15= CAST('abc' as binary) ;
set @arg15= NULL ;
create table t5 as select
8                           as const01, @arg01 as param01,
8.0                         as const02, @arg02 as param02,
80.00000000000e-1           as const03, @arg03 as param03,
'abc'                       as const04, @arg04 as param04,
CAST('abc' as binary)       as const05, @arg05 as param05,
'1991-08-05'                as const06, @arg06 as param06,
CAST('1991-08-05' as date)  as const07, @arg07 as param07,
'1991-08-05 01:01:01'       as const08, @arg08 as param08,
CAST('1991-08-05 01:01:01'  as datetime) as const09, @arg09 as param09,
unix_timestamp('1991-01-01 01:01:01')    as const10, @arg10 as param10,
YEAR('1991-01-01 01:01:01') as const11, @arg11 as param11, 
NULL                        as const12, @arg12 as param12,
@arg13 as param13,
@arg14 as param14,
@arg15 as param15;
show create table t5 ;
Table	Create Table
t5	CREATE TABLE `t5` (
  `const01` bigint(1) NOT NULL default '0',
  `param01` bigint(20) default NULL,
  `const02` double(3,1) NOT NULL default '0.0',
  `param02` double default NULL,
  `const03` double NOT NULL default '0',
  `param03` double default NULL,
  `const04` char(3) NOT NULL default '',
  `param04` longtext,
  `const05` binary(3) NOT NULL default '',
  `param05` longblob,
  `const06` varchar(10) NOT NULL default '',
  `param06` longtext,
  `const07` date default NULL,
  `param07` longblob,
  `const08` varchar(19) NOT NULL default '',
  `param08` longtext,
  `const09` datetime default NULL,
  `param09` longblob,
  `const10` int(10) NOT NULL default '0',
  `param10` bigint(20) default NULL,
  `const11` int(4) default NULL,
  `param11` bigint(20) default NULL,
  `const12` binary(0) default NULL,
  `param12` bigint(20) default NULL,
  `param13` double default NULL,
  `param14` longtext,
  `param15` longblob
) ENGINE=MyISAM DEFAULT CHARSET=latin1
select * from t5 ;
Catalog	Database	Table	Table_alias	Column	Column_alias	Name	Type	Length	Max length	Is_null	Flags	Decimals	Charsetnr
def	test	t5	t5	const01	const01	8	1	1	N	32769	0	63
def	test	t5	t5	param01	param01	8	20	1	Y	32768	0	63
def	test	t5	t5	const02	const02	5	3	3	N	32769	1	63
def	test	t5	t5	param02	param02	5	20	1	Y	32768	31	63
def	test	t5	t5	const03	const03	5	23	1	N	32769	31	63
def	test	t5	t5	param03	param03	5	20	1	Y	32768	31	63
def	test	t5	t5	const04	const04	254	3	3	N	1	0	8
def	test	t5	t5	param04	param04	252	16777215	3	Y	16	0	8
def	test	t5	t5	const05	const05	254	3	3	N	129	0	63
def	test	t5	t5	param05	param05	252	16777215	3	Y	144	0	63
def	test	t5	t5	const06	const06	253	10	10	N	1	0	8
def	test	t5	t5	param06	param06	252	16777215	10	Y	16	0	8
def	test	t5	t5	const07	const07	10	10	10	Y	128	0	63
def	test	t5	t5	param07	param07	252	16777215	10	Y	144	0	63
def	test	t5	t5	const08	const08	253	19	19	N	1	0	8
def	test	t5	t5	param08	param08	252	16777215	19	Y	16	0	8
def	test	t5	t5	const09	const09	12	19	19	Y	128	0	63
def	test	t5	t5	param09	param09	252	16777215	19	Y	144	0	63
def	test	t5	t5	const10	const10	3	10	9	N	32769	0	63
def	test	t5	t5	param10	param10	8	20	9	Y	32768	0	63
def	test	t5	t5	const11	const11	3	4	4	Y	32768	0	63
def	test	t5	t5	param11	param11	8	20	4	Y	32768	0	63
def	test	t5	t5	const12	const12	254	0	0	Y	128	0	63
def	test	t5	t5	param12	param12	8	20	0	Y	32768	0	63
def	test	t5	t5	param13	param13	5	20	0	Y	32768	31	63
def	test	t5	t5	param14	param14	252	16777215	0	Y	16	0	8
def	test	t5	t5	param15	param15	252	16777215	0	Y	144	0	63
const01	8
param01	8
const02	8.0
param02	8
const03	8
param03	8
const04	abc
param04	abc
const05	abc
param05	abc
const06	1991-08-05
param06	1991-08-05
const07	1991-08-05
param07	1991-08-05
const08	1991-08-05 01:01:01
param08	1991-08-05 01:01:01
const09	1991-08-05 01:01:01
param09	1991-08-05 01:01:01
const10	662680861
param10	662680861
const11	1991
param11	1991
const12	NULL
param12	NULL
param13	NULL
param14	NULL
param15	NULL
drop table t5 ;
test_sequence
------ data type conversion tests ------
delete from t1 ;
insert into t1 values (1,'one');
insert into t1 values (2,'two');
insert into t1 values (3,'three');
insert into t1 values (4,'four');
commit ;
delete from t9 ;
insert into t9
set c1= 1, c2= 1, c3= 1, c4= 1, c5= 1, c6= 1, c7= 1, c8= 1, c9= 1,
c10= 1, c11= 1, c12 = 1,
c13= '2004-02-29', c14= '2004-02-29 11:11:11', c15= '2004-02-29 11:11:11',
c16= '11:11:11', c17= '2004',
c18= 1, c19=true, c20= 'a', c21= '123456789a', 
c22= '123456789a123456789b123456789c', c23= 'tinyblob', c24= 'tinytext',
c25= 'blob', c26= 'text', c27= 'mediumblob', c28= 'mediumtext',
c29= 'longblob', c30= 'longtext', c31='one', c32= 'monday';
insert into t9
set c1= 9, c2= 9, c3= 9, c4= 9, c5= 9, c6= 9, c7= 9, c8= 9, c9= 9,
c10= 9, c11= 9, c12 = 9,
c13= '2004-02-29', c14= '2004-02-29 11:11:11', c15= '2004-02-29 11:11:11',
c16= '11:11:11', c17= '2004',
c18= 1, c19=false, c20= 'a', c21= '123456789a', 
c22= '123456789a123456789b123456789c', c23= 'tinyblob', c24= 'tinytext',
c25= 'blob', c26= 'text', c27= 'mediumblob', c28= 'mediumtext',
c29= 'longblob', c30= 'longtext', c31='two', c32= 'tuesday';
commit ;
insert into t9 set c1= 0, c15= '1991-01-01 01:01:01' ;
select * from t9 order by c1 ;
c1	c2	c3	c4	c5	c6	c7	c8	c9	c10	c11	c12	c13	c14	c15	c16	c17	c18	c19	c20	c21	c22	c23	c24	c25	c26	c27	c28	c29	c30	c31	c32
0	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	1991-01-01 01:01:01	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
1	1	1	1	1	1	1	1	1	1	1.0000	1.0000	2004-02-29	2004-02-29 11:11:11	2004-02-29 11:11:11	11:11:11	2004	1	1	a	123456789a	123456789a123456789b123456789c	tinyblob	tinytext	blob	text	mediumblob	mediumtext	longblob	longtext	one	monday
9	9	9	9	9	9	9	9	9	9	9.0000	9.0000	2004-02-29	2004-02-29 11:11:11	2004-02-29 11:11:11	11:11:11	2004	1	0	a	123456789a	123456789a123456789b123456789c	tinyblob	tinytext	blob	text	mediumblob	mediumtext	longblob	longtext	two	tuesday
test_sequence
------ select @parameter:= column ------
prepare full_info from "select @arg01, @arg02, @arg03, @arg04,
       @arg05, @arg06, @arg07, @arg08,
       @arg09, @arg10, @arg11, @arg12,
       @arg13, @arg14, @arg15, @arg16,
       @arg17, @arg18, @arg19, @arg20,
       @arg21, @arg22, @arg23, @arg24,
       @arg25, @arg26, @arg27, @arg28,
       @arg29, @arg30, @arg31, @arg32" ;
select @arg01:=  c1, @arg02:=  c2, @arg03:=  c3, @arg04:=  c4,
@arg05:=  c5, @arg06:=  c6, @arg07:=  c7, @arg08:=  c8,
@arg09:=  c9, @arg10:= c10, @arg11:= c11, @arg12:= c12,
@arg13:= c13, @arg14:= c14, @arg15:= c15, @arg16:= c16,
@arg17:= c17, @arg18:= c18, @arg19:= c19, @arg20:= c20,
@arg21:= c21, @arg22:= c22, @arg23:= c23, @arg24:= c24,
@arg25:= c25, @arg26:= c26, @arg27:= c27, @arg28:= c28,
@arg29:= c29, @arg30:= c30, @arg31:= c31, @arg32:= c32
from t9 where c1= 1 ;
@arg01:=  c1	@arg02:=  c2	@arg03:=  c3	@arg04:=  c4	@arg05:=  c5	@arg06:=  c6	@arg07:=  c7	@arg08:=  c8	@arg09:=  c9	@arg10:= c10	@arg11:= c11	@arg12:= c12	@arg13:= c13	@arg14:= c14	@arg15:= c15	@arg16:= c16	@arg17:= c17	@arg18:= c18	@arg19:= c19	@arg20:= c20	@arg21:= c21	@arg22:= c22	@arg23:= c23	@arg24:= c24	@arg25:= c25	@arg26:= c26	@arg27:= c27	@arg28:= c28	@arg29:= c29	@arg30:= c30	@arg31:= c31	@arg32:= c32
1	1	1	1	1	1	1	1	1	1	1.0000	1.0000	2004-02-29	2004-02-29 11:11:11	2004-02-29 11:11:11	11:11:11	2004	1	1	a	123456789a	123456789a123456789b123456789c	tinyblob	tinytext	blob	text	mediumblob	mediumtext	longblob	longtext	one	monday
execute full_info ;
Catalog	Database	Table	Table_alias	Column	Column_alias	Name	Type	Length	Max length	Is_null	Flags	Decimals	Charsetnr
def					@arg01	254	20	1	Y	128	31	63
def					@arg02	254	20	1	Y	128	31	63
def					@arg03	254	20	1	Y	128	31	63
def					@arg04	254	20	1	Y	128	31	63
def					@arg05	254	20	1	Y	128	31	63
def					@arg06	254	20	1	Y	128	31	63
def					@arg07	254	20	1	Y	128	31	63
def					@arg08	254	20	1	Y	128	31	63
def					@arg09	254	20	1	Y	128	31	63
def					@arg10	254	20	1	Y	128	31	63
def					@arg11	254	20	1	Y	128	31	63
def					@arg12	254	20	1	Y	128	31	63
def					@arg13	254	8192	10	Y	128	31	63
def					@arg14	254	8192	19	Y	128	31	63
def					@arg15	254	8192	19	Y	128	31	63
def					@arg16	254	8192	8	Y	128	31	63
def					@arg17	254	20	4	Y	128	31	63
def					@arg18	254	20	1	Y	128	31	63
def					@arg19	254	20	1	Y	128	31	63
def					@arg20	254	8192	1	Y	0	31	8
def					@arg21	254	8192	10	Y	0	31	8
def					@arg22	254	8192	30	Y	0	31	8
def					@arg23	254	8192	8	Y	128	31	63
def					@arg24	254	8192	8	Y	0	31	8
def					@arg25	254	8192	4	Y	128	31	63
def					@arg26	254	8192	4	Y	0	31	8
def					@arg27	254	8192	10	Y	128	31	63
def					@arg28	254	8192	10	Y	0	31	8
def					@arg29	254	8192	8	Y	128	31	63
def					@arg30	254	8192	8	Y	0	31	8
def					@arg31	254	8192	3	Y	0	31	8
def					@arg32	254	8192	6	Y	128	31	63
@arg01	@arg02	@arg03	@arg04	@arg05	@arg06	@arg07	@arg08	@arg09	@arg10	@arg11	@arg12	@arg13	@arg14	@arg15	@arg16	@arg17	@arg18	@arg19	@arg20	@arg21	@arg22	@arg23	@arg24	@arg25	@arg26	@arg27	@arg28	@arg29	@arg30	@arg31	@arg32
1	1	1	1	1	1	1	1	1	1	1	1	2004-02-29	2004-02-29 11:11:11	2004-02-29 11:11:11	11:11:11	2004	1	1	a	123456789a	123456789a123456789b123456789c	tinyblob	tinytext	blob	text	mediumblob	mediumtext	longblob	longtext	one	monday
select @arg01:=  c1, @arg02:=  c2, @arg03:=  c3, @arg04:=  c4,
@arg05:=  c5, @arg06:=  c6, @arg07:=  c7, @arg08:=  c8,
@arg09:=  c9, @arg10:= c10, @arg11:= c11, @arg12:= c12,
@arg13:= c13, @arg14:= c14, @arg15:= c15, @arg16:= c16,
@arg17:= c17, @arg18:= c18, @arg19:= c19, @arg20:= c20,
@arg21:= c21, @arg22:= c22, @arg23:= c23, @arg24:= c24,
@arg25:= c25, @arg26:= c26, @arg27:= c27, @arg28:= c28,
@arg29:= c29, @arg30:= c30, @arg31:= c31, @arg32:= c32
from t9 where c1= 0 ;
@arg01:=  c1	@arg02:=  c2	@arg03:=  c3	@arg04:=  c4	@arg05:=  c5	@arg06:=  c6	@arg07:=  c7	@arg08:=  c8	@arg09:=  c9	@arg10:= c10	@arg11:= c11	@arg12:= c12	@arg13:= c13	@arg14:= c14	@arg15:= c15	@arg16:= c16	@arg17:= c17	@arg18:= c18	@arg19:= c19	@arg20:= c20	@arg21:= c21	@arg22:= c22	@arg23:= c23	@arg24:= c24	@arg25:= c25	@arg26:= c26	@arg27:= c27	@arg28:= c28	@arg29:= c29	@arg30:= c30	@arg31:= c31	@arg32:= c32
0	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	1991-01-01 01:01:01	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
execute full_info ;
Catalog	Database	Table	Table_alias	Column	Column_alias	Name	Type	Length	Max length	Is_null	Flags	Decimals	Charsetnr
def					@arg01	254	20	1	Y	128	31	63
def					@arg02	254	20	0	Y	128	31	63
def					@arg03	254	20	0	Y	128	31	63
def					@arg04	254	20	0	Y	128	31	63
def					@arg05	254	20	0	Y	128	31	63
def					@arg06	254	20	0	Y	128	31	63
def					@arg07	254	20	0	Y	128	31	63
def					@arg08	254	20	0	Y	128	31	63
def					@arg09	254	20	0	Y	128	31	63
def					@arg10	254	20	0	Y	128	31	63
def					@arg11	254	20	0	Y	128	31	63
def					@arg12	254	20	0	Y	128	31	63
def					@arg13	254	8192	0	Y	128	31	63
def					@arg14	254	8192	0	Y	128	31	63
def					@arg15	254	8192	19	Y	128	31	63
def					@arg16	254	8192	0	Y	128	31	63
def					@arg17	254	20	0	Y	128	31	63
def					@arg18	254	20	0	Y	128	31	63
def					@arg19	254	20	0	Y	128	31	63
def					@arg20	254	8192	0	Y	0	31	8
def					@arg21	254	8192	0	Y	0	31	8
def					@arg22	254	8192	0	Y	0	31	8
def					@arg23	254	8192	0	Y	128	31	63
def					@arg24	254	8192	0	Y	0	31	8
def					@arg25	254	8192	0	Y	128	31	63
def					@arg26	254	8192	0	Y	0	31	8
def					@arg27	254	8192	0	Y	128	31	63
def					@arg28	254	8192	0	Y	0	31	8
def					@arg29	254	8192	0	Y	128	31	63
def					@arg30	254	8192	0	Y	0	31	8
def					@arg31	254	8192	0	Y	0	31	8
def					@arg32	254	8192	0	Y	0	31	8
@arg01	@arg02	@arg03	@arg04	@arg05	@arg06	@arg07	@arg08	@arg09	@arg10	@arg11	@arg12	@arg13	@arg14	@arg15	@arg16	@arg17	@arg18	@arg19	@arg20	@arg21	@arg22	@arg23	@arg24	@arg25	@arg26	@arg27	@arg28	@arg29	@arg30	@arg31	@arg32
0	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	1991-01-01 01:01:01	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
prepare stmt1 from "select 
       @arg01:=  c1, @arg02:=  c2, @arg03:=  c3, @arg04:=  c4,
       @arg05:=  c5, @arg06:=  c6, @arg07:=  c7, @arg08:=  c8,
       @arg09:=  c9, @arg10:= c10, @arg11:= c11, @arg12:= c12,
       @arg13:= c13, @arg14:= c14, @arg15:= c15, @arg16:= c16,
       @arg17:= c17, @arg18:= c18, @arg19:= c19, @arg20:= c20,
       @arg21:= c21, @arg22:= c22, @arg23:= c23, @arg24:= c24,
       @arg25:= c25, @arg26:= c26, @arg27:= c27, @arg28:= c28,
       @arg29:= c29, @arg30:= c30, @arg31:= c31, @arg32:= c32
from t9 where c1= ?" ;
set @my_key= 1 ;
execute stmt1 using @my_key ;
@arg01:=  c1	@arg02:=  c2	@arg03:=  c3	@arg04:=  c4	@arg05:=  c5	@arg06:=  c6	@arg07:=  c7	@arg08:=  c8	@arg09:=  c9	@arg10:= c10	@arg11:= c11	@arg12:= c12	@arg13:= c13	@arg14:= c14	@arg15:= c15	@arg16:= c16	@arg17:= c17	@arg18:= c18	@arg19:= c19	@arg20:= c20	@arg21:= c21	@arg22:= c22	@arg23:= c23	@arg24:= c24	@arg25:= c25	@arg26:= c26	@arg27:= c27	@arg28:= c28	@arg29:= c29	@arg30:= c30	@arg31:= c31	@arg32:= c32
1	1	1	1	1	1	1	1	1	1	1.0000	1.0000	2004-02-29	2004-02-29 11:11:11	2004-02-29 11:11:11	11:11:11	2004	1	1	a	123456789a	123456789a123456789b123456789c	tinyblob	tinytext	blob	text	mediumblob	mediumtext	longblob	longtext	one	monday
execute full_info ;
Catalog	Database	Table	Table_alias	Column	Column_alias	Name	Type	Length	Max length	Is_null	Flags	Decimals	Charsetnr
def					@arg01	254	20	1	Y	128	31	63
def					@arg02	254	20	1	Y	128	31	63
def					@arg03	254	20	1	Y	128	31	63
def					@arg04	254	20	1	Y	128	31	63
def					@arg05	254	20	1	Y	128	31	63
def					@arg06	254	20	1	Y	128	31	63
def					@arg07	254	20	1	Y	128	31	63
def					@arg08	254	20	1	Y	128	31	63
def					@arg09	254	20	1	Y	128	31	63
def					@arg10	254	20	1	Y	128	31	63
def					@arg11	254	20	1	Y	128	31	63
def					@arg12	254	20	1	Y	128	31	63
def					@arg13	254	8192	10	Y	128	31	63
def					@arg14	254	8192	19	Y	128	31	63
def					@arg15	254	8192	19	Y	128	31	63
def					@arg16	254	8192	8	Y	128	31	63
def					@arg17	254	20	4	Y	128	31	63
def					@arg18	254	20	1	Y	128	31	63
def					@arg19	254	20	1	Y	128	31	63
def					@arg20	254	8192	1	Y	0	31	8
def					@arg21	254	8192	10	Y	0	31	8
def					@arg22	254	8192	30	Y	0	31	8
def					@arg23	254	8192	8	Y	128	31	63
def					@arg24	254	8192	8	Y	0	31	8
def					@arg25	254	8192	4	Y	128	31	63
def					@arg26	254	8192	4	Y	0	31	8
def					@arg27	254	8192	10	Y	128	31	63
def					@arg28	254	8192	10	Y	0	31	8
def					@arg29	254	8192	8	Y	128	31	63
def					@arg30	254	8192	8	Y	0	31	8
def					@arg31	254	8192	3	Y	0	31	8
def					@arg32	254	8192	6	Y	128	31	63
@arg01	@arg02	@arg03	@arg04	@arg05	@arg06	@arg07	@arg08	@arg09	@arg10	@arg11	@arg12	@arg13	@arg14	@arg15	@arg16	@arg17	@arg18	@arg19	@arg20	@arg21	@arg22	@arg23	@arg24	@arg25	@arg26	@arg27	@arg28	@arg29	@arg30	@arg31	@arg32
1	1	1	1	1	1	1	1	1	1	1	1	2004-02-29	2004-02-29 11:11:11	2004-02-29 11:11:11	11:11:11	2004	1	1	a	123456789a	123456789a123456789b123456789c	tinyblob	tinytext	blob	text	mediumblob	mediumtext	longblob	longtext	one	monday
set @my_key= 0 ;
execute stmt1 using @my_key ;
@arg01:=  c1	@arg02:=  c2	@arg03:=  c3	@arg04:=  c4	@arg05:=  c5	@arg06:=  c6	@arg07:=  c7	@arg08:=  c8	@arg09:=  c9	@arg10:= c10	@arg11:= c11	@arg12:= c12	@arg13:= c13	@arg14:= c14	@arg15:= c15	@arg16:= c16	@arg17:= c17	@arg18:= c18	@arg19:= c19	@arg20:= c20	@arg21:= c21	@arg22:= c22	@arg23:= c23	@arg24:= c24	@arg25:= c25	@arg26:= c26	@arg27:= c27	@arg28:= c28	@arg29:= c29	@arg30:= c30	@arg31:= c31	@arg32:= c32
0	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	1991-01-01 01:01:01	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
execute full_info ;
Catalog	Database	Table	Table_alias	Column	Column_alias	Name	Type	Length	Max length	Is_null	Flags	Decimals	Charsetnr
def					@arg01	254	20	1	Y	128	31	63
def					@arg02	254	20	0	Y	128	31	63
def					@arg03	254	20	0	Y	128	31	63
def					@arg04	254	20	0	Y	128	31	63
def					@arg05	254	20	0	Y	128	31	63
def					@arg06	254	20	0	Y	128	31	63
def					@arg07	254	20	0	Y	128	31	63
def					@arg08	254	20	0	Y	128	31	63
def					@arg09	254	20	0	Y	128	31	63
def					@arg10	254	20	0	Y	128	31	63
def					@arg11	254	20	0	Y	128	31	63
def					@arg12	254	20	0	Y	128	31	63
def					@arg13	254	8192	0	Y	128	31	63
def					@arg14	254	8192	0	Y	128	31	63
def					@arg15	254	8192	19	Y	128	31	63
def					@arg16	254	8192	0	Y	128	31	63
def					@arg17	254	20	0	Y	128	31	63
def					@arg18	254	20	0	Y	128	31	63
def					@arg19	254	20	0	Y	128	31	63
def					@arg20	254	8192	0	Y	0	31	8
def					@arg21	254	8192	0	Y	0	31	8
def					@arg22	254	8192	0	Y	0	31	8
def					@arg23	254	8192	0	Y	128	31	63
def					@arg24	254	8192	0	Y	0	31	8
def					@arg25	254	8192	0	Y	128	31	63
def					@arg26	254	8192	0	Y	0	31	8
def					@arg27	254	8192	0	Y	128	31	63
def					@arg28	254	8192	0	Y	0	31	8
def					@arg29	254	8192	0	Y	128	31	63
def					@arg30	254	8192	0	Y	0	31	8
def					@arg31	254	8192	0	Y	0	31	8
def					@arg32	254	8192	0	Y	0	31	8
@arg01	@arg02	@arg03	@arg04	@arg05	@arg06	@arg07	@arg08	@arg09	@arg10	@arg11	@arg12	@arg13	@arg14	@arg15	@arg16	@arg17	@arg18	@arg19	@arg20	@arg21	@arg22	@arg23	@arg24	@arg25	@arg26	@arg27	@arg28	@arg29	@arg30	@arg31	@arg32
0	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	1991-01-01 01:01:01	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
prepare stmt1 from "select ? := c1 from t9 where c1= 1" ;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ':= c1 from t9 where c1= 1' at line 1
test_sequence
------ select column, .. into @parm,.. ------
select c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12,
c13, c14, c15, c16, c17, c18, c19, c20, c21, c22, c23, c24,
c25, c26, c27, c28, c29, c30, c31, c32
into @arg01, @arg02, @arg03, @arg04, @arg05, @arg06, @arg07, @arg08,
@arg09, @arg10, @arg11, @arg12, @arg13, @arg14, @arg15, @arg16,
@arg17, @arg18, @arg19, @arg20, @arg21, @arg22, @arg23, @arg24,
@arg25, @arg26, @arg27, @arg28, @arg29, @arg30, @arg31, @arg32
from t9 where c1= 1 ;
execute full_info ;
Catalog	Database	Table	Table_alias	Column	Column_alias	Name	Type	Length	Max length	Is_null	Flags	Decimals	Charsetnr
def					@arg01	254	20	1	Y	128	31	63
def					@arg02	254	20	1	Y	128	31	63
def					@arg03	254	20	1	Y	128	31	63
def					@arg04	254	20	1	Y	128	31	63
def					@arg05	254	20	1	Y	128	31	63
def					@arg06	254	20	1	Y	128	31	63
def					@arg07	254	20	1	Y	128	31	63
def					@arg08	254	20	1	Y	128	31	63
def					@arg09	254	20	1	Y	128	31	63
def					@arg10	254	20	1	Y	128	31	63
def					@arg11	254	20	1	Y	128	31	63
def					@arg12	254	20	1	Y	128	31	63
def					@arg13	254	8192	10	Y	128	31	63
def					@arg14	254	8192	19	Y	128	31	63
def					@arg15	254	8192	19	Y	128	31	63
def					@arg16	254	8192	8	Y	128	31	63
def					@arg17	254	20	4	Y	128	31	63
def					@arg18	254	20	1	Y	128	31	63
def					@arg19	254	20	1	Y	128	31	63
def					@arg20	254	8192	1	Y	0	31	8
def					@arg21	254	8192	10	Y	0	31	8
def					@arg22	254	8192	30	Y	0	31	8
def					@arg23	254	8192	8	Y	128	31	63
def					@arg24	254	8192	8	Y	0	31	8
def					@arg25	254	8192	4	Y	128	31	63
def					@arg26	254	8192	4	Y	0	31	8
def					@arg27	254	8192	10	Y	128	31	63
def					@arg28	254	8192	10	Y	0	31	8
def					@arg29	254	8192	8	Y	128	31	63
def					@arg30	254	8192	8	Y	0	31	8
def					@arg31	254	8192	3	Y	0	31	8
def					@arg32	254	8192	6	Y	128	31	63
@arg01	@arg02	@arg03	@arg04	@arg05	@arg06	@arg07	@arg08	@arg09	@arg10	@arg11	@arg12	@arg13	@arg14	@arg15	@arg16	@arg17	@arg18	@arg19	@arg20	@arg21	@arg22	@arg23	@arg24	@arg25	@arg26	@arg27	@arg28	@arg29	@arg30	@arg31	@arg32
1	1	1	1	1	1	1	1	1	1	1	1	2004-02-29	2004-02-29 11:11:11	2004-02-29 11:11:11	11:11:11	2004	1	1	a	123456789a	123456789a123456789b123456789c	tinyblob	tinytext	blob	text	mediumblob	mediumtext	longblob	longtext	one	monday
select c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12,
c13, c14, c15, c16, c17, c18, c19, c20, c21, c22, c23, c24,
c25, c26, c27, c28, c29, c30, c31, c32
into @arg01, @arg02, @arg03, @arg04, @arg05, @arg06, @arg07, @arg08,
@arg09, @arg10, @arg11, @arg12, @arg13, @arg14, @arg15, @arg16,
@arg17, @arg18, @arg19, @arg20, @arg21, @arg22, @arg23, @arg24,
@arg25, @arg26, @arg27, @arg28, @arg29, @arg30, @arg31, @arg32
from t9 where c1= 0 ;
execute full_info ;
Catalog	Database	Table	Table_alias	Column	Column_alias	Name	Type	Length	Max length	Is_null	Flags	Decimals	Charsetnr
def					@arg01	254	20	1	Y	128	31	63
def					@arg02	254	20	0	Y	128	31	63
def					@arg03	254	20	0	Y	128	31	63
def					@arg04	254	20	0	Y	128	31	63
def					@arg05	254	20	0	Y	128	31	63
def					@arg06	254	20	0	Y	128	31	63
def					@arg07	254	20	0	Y	128	31	63
def					@arg08	254	20	0	Y	128	31	63
def					@arg09	254	20	0	Y	128	31	63
def					@arg10	254	20	0	Y	128	31	63
def					@arg11	254	20	0	Y	128	31	63
def					@arg12	254	20	0	Y	128	31	63
def					@arg13	254	8192	0	Y	128	31	63
def					@arg14	254	8192	0	Y	128	31	63
def					@arg15	254	8192	19	Y	128	31	63
def					@arg16	254	8192	0	Y	128	31	63
def					@arg17	254	20	0	Y	128	31	63
def					@arg18	254	20	0	Y	128	31	63
def					@arg19	254	20	0	Y	128	31	63
def					@arg20	254	8192	0	Y	0	31	8
def					@arg21	254	8192	0	Y	0	31	8
def					@arg22	254	8192	0	Y	0	31	8
def					@arg23	254	8192	0	Y	128	31	63
def					@arg24	254	8192	0	Y	0	31	8
def					@arg25	254	8192	0	Y	128	31	63
def					@arg26	254	8192	0	Y	0	31	8
def					@arg27	254	8192	0	Y	128	31	63
def					@arg28	254	8192	0	Y	0	31	8
def					@arg29	254	8192	0	Y	128	31	63
def					@arg30	254	8192	0	Y	0	31	8
def					@arg31	254	8192	0	Y	0	31	8
def					@arg32	254	8192	0	Y	0	31	8
@arg01	@arg02	@arg03	@arg04	@arg05	@arg06	@arg07	@arg08	@arg09	@arg10	@arg11	@arg12	@arg13	@arg14	@arg15	@arg16	@arg17	@arg18	@arg19	@arg20	@arg21	@arg22	@arg23	@arg24	@arg25	@arg26	@arg27	@arg28	@arg29	@arg30	@arg31	@arg32
0	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	1991-01-01 01:01:01	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
prepare stmt1 from "select c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12,
       c13, c14, c15, c16, c17, c18, c19, c20, c21, c22, c23, c24,
       c25, c26, c27, c28, c29, c30, c31, c32
into @arg01, @arg02, @arg03, @arg04, @arg05, @arg06, @arg07, @arg08,
     @arg09, @arg10, @arg11, @arg12, @arg13, @arg14, @arg15, @arg16,
     @arg17, @arg18, @arg19, @arg20, @arg21, @arg22, @arg23, @arg24,
     @arg25, @arg26, @arg27, @arg28, @arg29, @arg30, @arg31, @arg32
from t9 where c1= ?" ;
set @my_key= 1 ;
execute stmt1 using @my_key ;
execute full_info ;
Catalog	Database	Table	Table_alias	Column	Column_alias	Name	Type	Length	Max length	Is_null	Flags	Decimals	Charsetnr
def					@arg01	254	20	1	Y	128	31	63
def					@arg02	254	20	1	Y	128	31	63
def					@arg03	254	20	1	Y	128	31	63
def					@arg04	254	20	1	Y	128	31	63
def					@arg05	254	20	1	Y	128	31	63
def					@arg06	254	20	1	Y	128	31	63
def					@arg07	254	20	1	Y	128	31	63
def					@arg08	254	20	1	Y	128	31	63
def					@arg09	254	20	1	Y	128	31	63
def					@arg10	254	20	1	Y	128	31	63
def					@arg11	254	20	1	Y	128	31	63
def					@arg12	254	20	1	Y	128	31	63
def					@arg13	254	8192	10	Y	128	31	63
def					@arg14	254	8192	19	Y	128	31	63
def					@arg15	254	8192	19	Y	128	31	63
def					@arg16	254	8192	8	Y	128	31	63
def					@arg17	254	20	4	Y	128	31	63
def					@arg18	254	20	1	Y	128	31	63
def					@arg19	254	20	1	Y	128	31	63
def					@arg20	254	8192	1	Y	0	31	8
def					@arg21	254	8192	10	Y	0	31	8
def					@arg22	254	8192	30	Y	0	31	8
def					@arg23	254	8192	8	Y	128	31	63
def					@arg24	254	8192	8	Y	0	31	8
def					@arg25	254	8192	4	Y	128	31	63
def					@arg26	254	8192	4	Y	0	31	8
def					@arg27	254	8192	10	Y	128	31	63
def					@arg28	254	8192	10	Y	0	31	8
def					@arg29	254	8192	8	Y	128	31	63
def					@arg30	254	8192	8	Y	0	31	8
def					@arg31	254	8192	3	Y	0	31	8
def					@arg32	254	8192	6	Y	128	31	63
@arg01	@arg02	@arg03	@arg04	@arg05	@arg06	@arg07	@arg08	@arg09	@arg10	@arg11	@arg12	@arg13	@arg14	@arg15	@arg16	@arg17	@arg18	@arg19	@arg20	@arg21	@arg22	@arg23	@arg24	@arg25	@arg26	@arg27	@arg28	@arg29	@arg30	@arg31	@arg32
1	1	1	1	1	1	1	1	1	1	1	1	2004-02-29	2004-02-29 11:11:11	2004-02-29 11:11:11	11:11:11	2004	1	1	a	123456789a	123456789a123456789b123456789c	tinyblob	tinytext	blob	text	mediumblob	mediumtext	longblob	longtext	one	monday
set @my_key= 0 ;
execute stmt1 using @my_key ;
execute full_info ;
Catalog	Database	Table	Table_alias	Column	Column_alias	Name	Type	Length	Max length	Is_null	Flags	Decimals	Charsetnr
def					@arg01	254	20	1	Y	128	31	63
def					@arg02	254	20	0	Y	128	31	63
def					@arg03	254	20	0	Y	128	31	63
def					@arg04	254	20	0	Y	128	31	63
def					@arg05	254	20	0	Y	128	31	63
def					@arg06	254	20	0	Y	128	31	63
def					@arg07	254	20	0	Y	128	31	63
def					@arg08	254	20	0	Y	128	31	63
def					@arg09	254	20	0	Y	128	31	63
def					@arg10	254	20	0	Y	128	31	63
def					@arg11	254	20	0	Y	128	31	63
def					@arg12	254	20	0	Y	128	31	63
def					@arg13	254	8192	0	Y	128	31	63
def					@arg14	254	8192	0	Y	128	31	63
def					@arg15	254	8192	19	Y	128	31	63
def					@arg16	254	8192	0	Y	128	31	63
def					@arg17	254	20	0	Y	128	31	63
def					@arg18	254	20	0	Y	128	31	63
def					@arg19	254	20	0	Y	128	31	63
def					@arg20	254	8192	0	Y	0	31	8
def					@arg21	254	8192	0	Y	0	31	8
def					@arg22	254	8192	0	Y	0	31	8
def					@arg23	254	8192	0	Y	128	31	63
def					@arg24	254	8192	0	Y	0	31	8
def					@arg25	254	8192	0	Y	128	31	63
def					@arg26	254	8192	0	Y	0	31	8
def					@arg27	254	8192	0	Y	128	31	63
def					@arg28	254	8192	0	Y	0	31	8
def					@arg29	254	8192	0	Y	128	31	63
def					@arg30	254	8192	0	Y	0	31	8
def					@arg31	254	8192	0	Y	0	31	8
def					@arg32	254	8192	0	Y	0	31	8
@arg01	@arg02	@arg03	@arg04	@arg05	@arg06	@arg07	@arg08	@arg09	@arg10	@arg11	@arg12	@arg13	@arg14	@arg15	@arg16	@arg17	@arg18	@arg19	@arg20	@arg21	@arg22	@arg23	@arg24	@arg25	@arg26	@arg27	@arg28	@arg29	@arg30	@arg31	@arg32
0	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	1991-01-01 01:01:01	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
prepare stmt1 from "select c1 into ? from t9 where c1= 1" ;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '? from t9 where c1= 1' at line 1
test_sequence
-- insert into numeric columns --
insert into t9 
( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
( 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20 ) ;
set @arg00= 21 ;
insert into t9 
( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
( @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00, @arg00 ) ;
prepare stmt1 from "insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22 )" ;
execute stmt1 ;
set @arg00= 23;
prepare stmt2 from "insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values 
  (  ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00 ;
insert into t9 
( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
( 30.0, 30.0, 30.0, 30.0, 30.0, 30.0, 30.0, 30.0,
30.0, 30.0, 30.0 ) ;
set @arg00= 31.0 ;
insert into t9 
( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
( @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00, @arg00 ) ;
prepare stmt1 from "insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( 32.0, 32.0, 32.0, 32.0, 32.0, 32.0, 32.0, 32.0,
    32.0, 32.0, 32.0 )" ;
execute stmt1 ;
set @arg00= 33.0;
prepare stmt2 from "insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values 
  (  ?,  ?,  ?,  ?,  ?,  ?,  ?,  ?,  ?,   ?,   ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00 ;
insert into t9 
( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
( '40', '40', '40', '40', '40', '40', '40', '40',
'40', '40', '40' ) ;
set @arg00= '41' ;
insert into t9 
( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
( @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00, @arg00 ) ;
prepare stmt1 from "insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( '42', '42', '42', '42', '42', '42', '42', '42',
    '42', '42', '42' )" ;
execute stmt1 ;
set @arg00= '43';
prepare stmt2 from "insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values 
  ( ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00 ;
insert into t9 
( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
( CAST('50' as binary), CAST('50' as binary), 
CAST('50' as binary), CAST('50' as binary), CAST('50' as binary), 
CAST('50' as binary), CAST('50' as binary), CAST('50' as binary),
CAST('50' as binary), CAST('50' as binary), CAST('50' as binary) ) ;
set @arg00= CAST('51' as binary) ;
insert into t9 
( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
( @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00, @arg00 ) ;
prepare stmt1 from "insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( CAST('52' as binary), CAST('52' as binary),
  CAST('52' as binary), CAST('52' as binary), CAST('52' as binary), 
  CAST('52' as binary), CAST('52' as binary), CAST('52' as binary),
  CAST('52' as binary), CAST('52' as binary), CAST('52' as binary) )" ;
execute stmt1 ;
set @arg00= CAST('53' as binary) ;
prepare stmt2 from "insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values 
  ( ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00 ;
set @arg00= 2 ;
set @arg00= NULL ;
insert into t9
( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
( 60, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
NULL, NULL, NULL ) ;
insert into t9
( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
( 61, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00, @arg00 ) ;
prepare stmt1 from "insert into t9
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( 62, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL )" ;
execute stmt1 ;
prepare stmt2 from "insert into t9
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( 63, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00 ;
set @arg00= 8.0 ;
set @arg00= NULL ;
insert into t9
( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
( 71, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00, @arg00 ) ;
prepare stmt2 from "insert into t9
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( 73, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00 ;
set @arg00= 'abc' ;
set @arg00= NULL ;
insert into t9
( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
( 81, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00, @arg00 ) ;
prepare stmt2 from "insert into t9
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( 83, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00 ;
select c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12
from t9 where c1 >= 20
order by c1 ;
c1	c2	c3	c4	c5	c6	c7	c8	c9	c10	c12
20	20	20	20	20	20	20	20	20	20	20.0000
21	21	21	21	21	21	21	21	21	21	21.0000
22	22	22	22	22	22	22	22	22	22	22.0000
23	23	23	23	23	23	23	23	23	23	23.0000
30	30	30	30	30	30	30	30	30	30	30.0000
31	31	31	31	31	31	31	31	31	31	31.0000
32	32	32	32	32	32	32	32	32	32	32.0000
33	33	33	33	33	33	33	33	33	33	33.0000
40	40	40	40	40	40	40	40	40	40	40.0000
41	41	41	41	41	41	41	41	41	41	41.0000
42	42	42	42	42	42	42	42	42	42	42.0000
43	43	43	43	43	43	43	43	43	43	43.0000
50	50	50	50	50	50	50	50	50	50	50.0000
51	51	51	51	51	51	51	51	51	51	51.0000
52	52	52	52	52	52	52	52	52	52	52.0000
53	53	53	53	53	53	53	53	53	53	53.0000
60	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
61	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
62	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
63	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
71	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
73	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
81	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
83	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
test_sequence
-- select .. where numeric column = .. --
set @arg00= 20;
select 'true' as found from t9 
where c1= 20 and c2= 20 and c3= 20 and c4= 20 and c5= 20 and c6= 20 and c7= 20
and c8= 20 and c9= 20 and c10= 20 and c12= 20;
found
true
select 'true' as found from t9 
where c1= @arg00 and c2= @arg00 and c3= @arg00 and c4= @arg00 and c5= @arg00 
and c6= @arg00 and c7= @arg00 and c8= @arg00 and c9= @arg00 and c10= @arg00
and c12= @arg00;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and c2= 20 and c3= 20 and c4= 20 and c5= 20 and c6= 20 and c7= 20
  and c8= 20 and c9= 20 and c10= 20 and c12= 20 ";
execute stmt1 ;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= ? and c2= ? and c3= ? and c4= ? and c5= ? 
  and c6= ? and c7= ? and c8= ? and c9= ? and c10= ?
  and c12= ? ";
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00 ;
found
true
set @arg00= 20.0;
select 'true' as found from t9 
where c1= 20.0 and c2= 20.0 and c3= 20.0 and c4= 20.0 and c5= 20.0 and c6= 20.0
and c7= 20.0 and c8= 20.0 and c9= 20.0 and c10= 20.0 and c12= 20.0;
found
true
select 'true' as found from t9 
where c1= @arg00 and c2= @arg00 and c3= @arg00 and c4= @arg00 and c5= @arg00 
and c6= @arg00 and c7= @arg00 and c8= @arg00 and c9= @arg00 and c10= @arg00
and c12= @arg00;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20.0 and c2= 20.0 and c3= 20.0 and c4= 20.0 and c5= 20.0 and c6= 20.0
  and c7= 20.0 and c8= 20.0 and c9= 20.0 and c10= 20.0 and c12= 20.0 ";
execute stmt1 ;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= ? and c2= ? and c3= ? and c4= ? and c5= ? 
  and c6= ? and c7= ? and c8= ? and c9= ? and c10= ?
  and c12= ? ";
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00 ;
found
true
select 'true' as found from t9 
where c1= '20' and c2= '20' and c3= '20' and c4= '20' and c5= '20' and c6= '20'
  and c7= '20' and c8= '20' and c9= '20' and c10= '20' and c12= '20';
found
true
prepare stmt1 from "select 'true' as found from t9
where c1= '20' and c2= '20' and c3= '20' and c4= '20' and c5= '20' and c6= '20'
  and c7= '20' and c8= '20' and c9= '20' and c10= '20' and c12= '20' ";
execute stmt1 ;
found
true
set @arg00= '20';
select 'true' as found from t9 
where c1= @arg00 and c2= @arg00 and c3= @arg00 and c4= @arg00 and c5= @arg00 
and c6= @arg00 and c7= @arg00 and c8= @arg00 and c9= @arg00 and c10= @arg00
and c12= @arg00;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= ? and c2= ? and c3= ? and c4= ? and c5= ? 
  and c6= ? and c7= ? and c8= ? and c9= ? and c10= ?
  and c12= ? ";
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00 ;
found
true
select 'true' as found from t9 
where c1= CAST('20' as binary) and c2= CAST('20' as binary) and 
c3= CAST('20' as binary) and c4= CAST('20' as binary) and 
c5= CAST('20' as binary) and c6= CAST('20' as binary) and 
c7= CAST('20' as binary) and c8= CAST('20' as binary) and 
c9= CAST('20' as binary) and c10= CAST('20' as binary) and 
c12= CAST('20' as binary);
found
true
prepare stmt1 from "select 'true' as found from t9
where c1= CAST('20' as binary) and c2= CAST('20' as binary) and 
      c3= CAST('20' as binary) and c4= CAST('20' as binary) and 
      c5= CAST('20' as binary) and c6= CAST('20' as binary) and 
      c7= CAST('20' as binary) and c8= CAST('20' as binary) and 
      c9= CAST('20' as binary) and c10= CAST('20' as binary) and 
      c12= CAST('20' as binary) ";
execute stmt1 ;
found
true
set @arg00= CAST('20' as binary) ;
select 'true' as found from t9 
where c1= @arg00 and c2= @arg00 and c3= @arg00 and c4= @arg00 and c5= @arg00 
and c6= @arg00 and c7= @arg00 and c8= @arg00 and c9= @arg00 and c10= @arg00
and c12= @arg00;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= ? and c2= ? and c3= ? and c4= ? and c5= ? 
  and c6= ? and c7= ? and c8= ? and c9= ? and c10= ?
  and c12= ? ";
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00 ;
found
true
delete from t9 ;
test_sequence
-- some numeric overflow experiments --
prepare my_insert from "insert into t9 
   ( c21, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values 
   ( 'O',  ?,  ?,  ?,  ?,  ?,  ?,  ?,  ?,  ?,   ?,   ? )" ;
prepare my_select from "select c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12
from t9 where c21 = 'O' ";
prepare my_delete from "delete from t9 where c21 = 'O' ";
set @arg00= 9223372036854775807 ;
execute my_insert using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00, @arg00 ;
Warnings:
Warning	1264	Data truncated; out of range for column 'c1' at row 1
Warning	1264	Data truncated; out of range for column 'c2' at row 1
Warning	1264	Data truncated; out of range for column 'c3' at row 1
Warning	1264	Data truncated; out of range for column 'c4' at row 1
Warning	1264	Data truncated; out of range for column 'c5' at row 1
Warning	1264	Data truncated; out of range for column 'c12' at row 1
execute my_select ;
c1	127
c2	32767
c3	8388607
c4	2147483647
c5	2147483647
c6	9223372036854775807
c7	9.22337e+18
c8	9.22337203685478e+18
c9	9.22337203685478e+18
c10	9.22337203685478e+18
c12	99999.9999
execute my_delete ;
set @arg00= '9223372036854775807' ;
execute my_insert using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00, @arg00 ;
Warnings:
Warning	1264	Data truncated; out of range for column 'c1' at row 1
Warning	1264	Data truncated; out of range for column 'c2' at row 1
Warning	1264	Data truncated; out of range for column 'c3' at row 1
Warning	1265	Data truncated for column 'c4' at row 1
Warning	1265	Data truncated for column 'c5' at row 1
Warning	1264	Data truncated; out of range for column 'c12' at row 1
execute my_select ;
c1	127
c2	32767
c3	8388607
c4	2147483647
c5	2147483647
c6	9223372036854775807
c7	9.22337e+18
c8	9.22337203685478e+18
c9	9.22337203685478e+18
c10	9.22337203685478e+18
c12	99999.9999
execute my_delete ;
set @arg00= -9223372036854775808 ;
execute my_insert using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00, @arg00 ;
Warnings:
Warning	1264	Data truncated; out of range for column 'c1' at row 1
Warning	1264	Data truncated; out of range for column 'c2' at row 1
Warning	1264	Data truncated; out of range for column 'c3' at row 1
Warning	1264	Data truncated; out of range for column 'c4' at row 1
Warning	1264	Data truncated; out of range for column 'c5' at row 1
Warning	1264	Data truncated; out of range for column 'c12' at row 1
execute my_select ;
c1	-128
c2	-32768
c3	-8388608
c4	-2147483648
c5	-2147483648
c6	-9223372036854775808
c7	-9.22337e+18
c8	-9.22337203685478e+18
c9	-9.22337203685478e+18
c10	-9.22337203685478e+18
c12	-9999.9999
execute my_delete ;
set @arg00= '-9223372036854775808' ;
execute my_insert using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00, @arg00 ;
Warnings:
Warning	1264	Data truncated; out of range for column 'c1' at row 1
Warning	1264	Data truncated; out of range for column 'c2' at row 1
Warning	1264	Data truncated; out of range for column 'c3' at row 1
Warning	1265	Data truncated for column 'c4' at row 1
Warning	1265	Data truncated for column 'c5' at row 1
Warning	1264	Data truncated; out of range for column 'c12' at row 1
execute my_select ;
c1	-128
c2	-32768
c3	-8388608
c4	-2147483648
c5	-2147483648
c6	-9223372036854775808
c7	-9.22337e+18
c8	-9.22337203685478e+18
c9	-9.22337203685478e+18
c10	-9.22337203685478e+18
c12	-9999.9999
execute my_delete ;
set @arg00= 1.11111111111111111111e+50 ;
execute my_insert using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00, @arg00 ;
Warnings:
Warning	1264	Data truncated; out of range for column 'c1' at row 1
Warning	1264	Data truncated; out of range for column 'c2' at row 1
Warning	1264	Data truncated; out of range for column 'c3' at row 1
Warning	1264	Data truncated; out of range for column 'c4' at row 1
Warning	1264	Data truncated; out of range for column 'c5' at row 1
Warning	1264	Data truncated; out of range for column 'c6' at row 1
Warning	1264	Data truncated; out of range for column 'c7' at row 1
Warning	1264	Data truncated; out of range for column 'c12' at row 1
execute my_select ;
c1	127
c2	32767
c3	8388607
c4	2147483647
c5	2147483647
c6	9223372036854775807
c7	3.40282e+38
c8	1.11111111111111e+50
c9	1.11111111111111e+50
c10	1.11111111111111e+50
c12	99999.9999
execute my_delete ;
set @arg00= '1.11111111111111111111e+50' ;
execute my_insert using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00, @arg00 ;
Warnings:
Warning	1265	Data truncated for column 'c1' at row 1
Warning	1265	Data truncated for column 'c2' at row 1
Warning	1265	Data truncated for column 'c3' at row 1
Warning	1265	Data truncated for column 'c4' at row 1
Warning	1265	Data truncated for column 'c5' at row 1
Warning	1265	Data truncated for column 'c6' at row 1
Warning	1264	Data truncated; out of range for column 'c7' at row 1
Warning	1264	Data truncated; out of range for column 'c12' at row 1
execute my_select ;
c1	1
c2	1
c3	1
c4	1
c5	1
c6	1
c7	3.40282e+38
c8	1.11111111111111e+50
c9	1.11111111111111e+50
c10	1.11111111111111e+50
c12	99999.9999
execute my_delete ;
set @arg00= -1.11111111111111111111e+50 ;
execute my_insert using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00, @arg00 ;
Warnings:
Warning	1264	Data truncated; out of range for column 'c1' at row 1
Warning	1264	Data truncated; out of range for column 'c2' at row 1
Warning	1264	Data truncated; out of range for column 'c3' at row 1
Warning	1264	Data truncated; out of range for column 'c4' at row 1
Warning	1264	Data truncated; out of range for column 'c5' at row 1
Warning	1264	Data truncated; out of range for column 'c6' at row 1
Warning	1264	Data truncated; out of range for column 'c7' at row 1
Warning	1264	Data truncated; out of range for column 'c12' at row 1
execute my_select ;
c1	-128
c2	-32768
c3	-8388608
c4	-2147483648
c5	-2147483648
c6	-9223372036854775808
c7	-3.40282e+38
c8	-1.11111111111111e+50
c9	-1.11111111111111e+50
c10	-1.11111111111111e+50
c12	-9999.9999
execute my_delete ;
set @arg00= '-1.11111111111111111111e+50' ;
execute my_insert using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00, @arg00 ;
Warnings:
Warning	1265	Data truncated for column 'c1' at row 1
Warning	1265	Data truncated for column 'c2' at row 1
Warning	1265	Data truncated for column 'c3' at row 1
Warning	1265	Data truncated for column 'c4' at row 1
Warning	1265	Data truncated for column 'c5' at row 1
Warning	1265	Data truncated for column 'c6' at row 1
Warning	1264	Data truncated; out of range for column 'c7' at row 1
Warning	1264	Data truncated; out of range for column 'c12' at row 1
execute my_select ;
c1	-1
c2	-1
c3	-1
c4	-1
c5	-1
c6	-1
c7	-3.40282e+38
c8	-1.11111111111111e+50
c9	-1.11111111111111e+50
c10	-1.11111111111111e+50
c12	-9999.9999
execute my_delete ;
test_sequence
-- insert into string columns --
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
select c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30
from t9 where c1 >= 20
order by c1 ;
c1	c20	c21	c22	c23	c24	c25	c26	c27	c28	c29	c30
20	2	20	20	20	20	20	20	20	20	20	20
21	2	21	21	21	21	21	21	21	21	21	21
22	2	22	22	22	22	22	22	22	22	22	22
23	2	23	23	23	23	23	23	23	23	23	23
30	3	30	30	30	30	30	30	30	30	30	30
31	3	31	31	31	31	31	31	31	31	31	31
32	3	32	32	32	32	32	32	32	32	32	32
33	3	33	33	33	33	33	33	33	33	33	33
40	4	40	40	40	40	40	40	40	40	40	40
41	4	41	41	41	41	41	41	41	41	41	41
42	4	42	42	42	42	42	42	42	42	42	42
43	4	43	43	43	43	43	43	43	43	43	43
50	5	50	50	50.00	50.00	50.00	50.00	50.00	50.00	50.00	50.00
51	5	51	51	51	51	51	51	51	51	51	51
52	5	52	52	52.00	52.00	52.00	52.00	52.00	52.00	52.00	52.00
53	5	53	53	53.00	53.00	53.00	53.00	53.00	53.00	53.00	53.00
54	5	54	54	54.00	54.00	54.00	54.00	54.00	54.00	54.00	54.00
55	5	55	55	55	55	55	55	55	55	55	55
56	6	56	56	56.00	56.00	56.00	56.00	56.00	56.00	56.00	56.00
57	6	57	57	57.00	57.00	57.00	57.00	57.00	57.00	57.00	57.00
60	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
61	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
62	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
63	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
71	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
73	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
81	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
83	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
test_sequence
-- select .. where string column = .. --
set @arg00= '20';
select 'true' as found from t9 
where c1= 20 and concat(c20,substr('20',1+length(c20)))= '20' and c21= '20' and
c22= '20' and c23= '20' and c24= '20' and c25= '20' and c26= '20' and
c27= '20' and c28= '20' and c29= '20' and c30= '20' ;
found
true
select 'true' as found from t9 
where c1= 20 and concat(c20,substr(@arg00,1+length(c20)))= @arg00 and
c21= @arg00 and c22= @arg00 and c23= @arg00 and c25= @arg00 and
c26= @arg00 and c27= @arg00 and c28= @arg00 and c29= @arg00 and c30= @arg00;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and concat(c20,substr('20',1+length(c20)))= '20' and c21= '20' and
  c22= '20' and c23= '20' and c24= '20' and c25= '20' and c26= '20' and
  c27= '20' and c28= '20' and c29= '20' and c30= '20'" ;
execute stmt1 ;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and concat(c20,substr(?,1+length(c20)))= ? and
  c21= ? and c22= ? and c23= ? and c25= ? and
  c26= ? and c27= ? and c28= ? and c29= ? and c30= ?" ;
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00, @arg00 ;
found
true
set @arg00= CAST('20' as binary);
select 'true' as found from t9 
where c1= 20 and concat(c20,substr(CAST('20' as binary),1+length(c20)))
= CAST('20' as binary) and c21= CAST('20' as binary)
and c22= CAST('20' as binary) and c23= CAST('20' as binary) and
c24= CAST('20' as binary) and c25= CAST('20' as binary) and
c26= CAST('20' as binary) and c27= CAST('20' as binary) and
c28= CAST('20' as binary) and c29= CAST('20' as binary) and
c30= CAST('20' as binary) ;
found
true
select 'true' as found from t9 
where c1= 20 and concat(c20,substr(@arg00,1+length(c20))) = @arg00 and
c21= @arg00 and c22= @arg00 and c23= @arg00 and c25= @arg00 and
c26= @arg00 and c27= @arg00 and c28= @arg00 and c29= @arg00 and
c30= @arg00;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and concat(c20,substr(CAST('20' as binary),1+length(c20)))
                 = CAST('20' as binary) and c21= CAST('20' as binary)
  and c22= CAST('20' as binary) and c23= CAST('20' as binary) and
  c24= CAST('20' as binary) and c25= CAST('20' as binary) and
  c26= CAST('20' as binary) and c27= CAST('20' as binary) and
  c28= CAST('20' as binary) and c29= CAST('20' as binary) and
  c30= CAST('20' as binary)" ;
execute stmt1 ;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and concat(c20,substr(?,1+length(c20))) = ? and c21= ? and
  c22= ? and c23= ? and c25= ? and c26= ? and c27= ? and c28= ? and
  c29= ? and c30= ?";
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00, @arg00 ;
found
true
set @arg00= 20;
select 'true' as found from t9 
where c1= 20 and concat(c20,substr(20,1+length(c20)))= 20 and c21= 20 and
c22= 20 and c23= 20 and c24= 20 and c25= 20 and c26= 20 and
c27= 20 and c28= 20 and c29= 20 and c30= 20 ;
found
true
select 'true' as found from t9 
where c1= 20 and concat(c20,substr(@arg00,1+length(c20)))= @arg00 and
c21= @arg00 and c22= @arg00 and c23= @arg00 and c25= @arg00 and
c26= @arg00 and c27= @arg00 and c28= @arg00 and c29= @arg00 and c30= @arg00;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and concat(c20,substr(20,1+length(c20)))= 20 and c21= 20 and
  c22= 20 and c23= 20 and c24= 20 and c25= 20 and c26= 20 and
  c27= 20 and c28= 20 and c29= 20 and c30= 20" ;
execute stmt1 ;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and concat(c20,substr(?,1+length(c20)))= ? and
  c21= ? and c22= ? and c23= ? and c25= ? and
  c26= ? and c27= ? and c28= ? and c29= ? and c30= ?" ;
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00, @arg00 ;
found
true
set @arg00= 20.0;
select 'true' as found from t9 
where c1= 20 and concat(c20,substr(20.0,1+length(c20)))= 20.0 and c21= 20.0 and
c22= 20.0 and c23= 20.0 and c24= 20.0 and c25= 20.0 and c26= 20.0 and
c27= 20.0 and c28= 20.0 and c29= 20.0 and c30= 20.0 ;
found
true
select 'true' as found from t9 
where c1= 20 and concat(c20,substr(@arg00,1+length(c20)))= @arg00 and
c21= @arg00 and c22= @arg00 and c23= @arg00 and c25= @arg00 and
c26= @arg00 and c27= @arg00 and c28= @arg00 and c29= @arg00 and c30= @arg00;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and concat(c20,substr(20.0,1+length(c20)))= 20.0 and c21= 20.0 and
  c22= 20.0 and c23= 20.0 and c24= 20.0 and c25= 20.0 and c26= 20.0 and
  c27= 20.0 and c28= 20.0 and c29= 20.0 and c30= 20.0" ;
execute stmt1 ;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and concat(c20,substr(?,1+length(c20)))= ? and
  c21= ? and c22= ? and c23= ? and c25= ? and
  c26= ? and c27= ? and c28= ? and c29= ? and c30= ?" ;
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00, @arg00 ;
found
true
delete from t9 ;
test_sequence
-- insert into date/time columns --
Warnings:
Warning	1265	Data truncated for column 'c17' at row 1
Warnings:
Warning	1265	Data truncated for column 'c17' at row 1
Warnings:
Warning	1265	Data truncated for column 'c17' at row 1
Warnings:
Warning	1265	Data truncated for column 'c17' at row 1
Warnings:
Warning	1265	Data truncated for column 'c17' at row 1
Warnings:
Warning	1265	Data truncated for column 'c17' at row 1
Warnings:
Warning	1265	Data truncated for column 'c17' at row 1
Warnings:
Warning	1265	Data truncated for column 'c17' at row 1
Warnings:
Warning	1264	Data truncated; out of range for column 'c13' at row 1
Warning	1265	Data truncated for column 'c14' at row 1
Warning	1265	Data truncated for column 'c15' at row 1
Warning	1264	Data truncated; out of range for column 'c16' at row 1
Warning	1264	Data truncated; out of range for column 'c17' at row 1
Warnings:
Warning	1264	Data truncated; out of range for column 'c13' at row 1
Warning	1265	Data truncated for column 'c14' at row 1
Warning	1265	Data truncated for column 'c15' at row 1
Warning	1264	Data truncated; out of range for column 'c16' at row 1
Warning	1264	Data truncated; out of range for column 'c17' at row 1
Warnings:
Warning	1264	Data truncated; out of range for column 'c13' at row 1
Warning	1265	Data truncated for column 'c14' at row 1
Warning	1265	Data truncated for column 'c15' at row 1
Warning	1264	Data truncated; out of range for column 'c16' at row 1
Warning	1264	Data truncated; out of range for column 'c17' at row 1
Warnings:
Warning	1264	Data truncated; out of range for column 'c13' at row 1
Warning	1265	Data truncated for column 'c14' at row 1
Warning	1265	Data truncated for column 'c15' at row 1
Warning	1264	Data truncated; out of range for column 'c16' at row 1
Warning	1264	Data truncated; out of range for column 'c17' at row 1
Warnings:
Warning	1265	Data truncated for column 'c15' at row 1
Warning	1264	Data truncated; out of range for column 'c16' at row 1
Warning	1264	Data truncated; out of range for column 'c17' at row 1
Warnings:
Warning	1265	Data truncated for column 'c15' at row 1
Warning	1264	Data truncated; out of range for column 'c16' at row 1
Warning	1264	Data truncated; out of range for column 'c17' at row 1
Warnings:
Warning	1265	Data truncated for column 'c15' at row 1
Warning	1264	Data truncated; out of range for column 'c16' at row 1
Warning	1264	Data truncated; out of range for column 'c17' at row 1
Warnings:
Warning	1265	Data truncated for column 'c15' at row 1
Warning	1264	Data truncated; out of range for column 'c16' at row 1
Warning	1264	Data truncated; out of range for column 'c17' at row 1
select c1, c13, c14, c15, c16, c17 from t9 order by c1 ;
c1	c13	c14	c15	c16	c17
20	1991-01-01	1991-01-01 01:01:01	1991-01-01 01:01:01	01:01:01	1991
21	1991-01-01	1991-01-01 01:01:01	1991-01-01 01:01:01	01:01:01	1991
22	1991-01-01	1991-01-01 01:01:01	1991-01-01 01:01:01	01:01:01	1991
23	1991-01-01	1991-01-01 01:01:01	1991-01-01 01:01:01	01:01:01	1991
30	1991-01-01	1991-01-01 01:01:01	1991-01-01 01:01:01	01:01:01	1991
31	1991-01-01	1991-01-01 01:01:01	1991-01-01 01:01:01	01:01:01	1991
32	1991-01-01	1991-01-01 01:01:01	1991-01-01 01:01:01	01:01:01	1991
33	1991-01-01	1991-01-01 01:01:01	1991-01-01 01:01:01	01:01:01	1991
40	0000-00-00	0000-00-00 00:00:00	0000-00-00 00:00:00	838:59:59	0000
41	0000-00-00	0000-00-00 00:00:00	0000-00-00 00:00:00	838:59:59	0000
42	0000-00-00	0000-00-00 00:00:00	0000-00-00 00:00:00	838:59:59	0000
43	0000-00-00	0000-00-00 00:00:00	0000-00-00 00:00:00	838:59:59	0000
50	2001-00-00	2001-00-00 00:00:00	0000-00-00 00:00:00	838:59:59	0000
51	0010-00-00	0010-00-00 00:00:00	0000-00-00 00:00:00	838:59:59	0000
52	2001-00-00	2001-00-00 00:00:00	0000-00-00 00:00:00	838:59:59	0000
53	2001-00-00	2001-00-00 00:00:00	0000-00-00 00:00:00	838:59:59	0000
60	NULL	NULL	1991-01-01 01:01:01	NULL	NULL
61	NULL	NULL	1991-01-01 01:01:01	NULL	NULL
62	NULL	NULL	1991-01-01 01:01:01	NULL	NULL
63	NULL	NULL	1991-01-01 01:01:01	NULL	NULL
71	NULL	NULL	1991-01-01 01:01:01	NULL	NULL
73	NULL	NULL	1991-01-01 01:01:01	NULL	NULL
81	NULL	NULL	1991-01-01 01:01:01	NULL	NULL
83	NULL	NULL	1991-01-01 01:01:01	NULL	NULL
test_sequence
-- select .. where date/time column = .. --
set @arg00= '1991-01-01 01:01:01' ;
select 'true' as found from t9 
where c1= 20 and c13= '1991-01-01 01:01:01' and c14= '1991-01-01 01:01:01' and
c15= '1991-01-01 01:01:01' and c16= '1991-01-01 01:01:01' and
c17= '1991-01-01 01:01:01' ;
found
true
select 'true' as found from t9 
where c1= 20 and c13= @arg00 and c14= @arg00 and c15= @arg00 and c16= @arg00
and c17= @arg00 ;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and c13= '1991-01-01 01:01:01' and c14= '1991-01-01 01:01:01' and
  c15= '1991-01-01 01:01:01' and c16= '1991-01-01 01:01:01' and
  c17= '1991-01-01 01:01:01'" ;
execute stmt1 ;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and c13= ? and c14= ? and c15= ? and c16= ? and c17= ?" ;
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00 ;
found
true
set @arg00= CAST('1991-01-01 01:01:01' as datetime) ;
select 'true' as found from t9 
where c1= 20 and c13= CAST('1991-01-01 01:01:01' as datetime) and
c14= CAST('1991-01-01 01:01:01' as datetime) and
c15= CAST('1991-01-01 01:01:01' as datetime) and
c16= CAST('1991-01-01 01:01:01' as datetime) and
c17= CAST('1991-01-01 01:01:01' as datetime) ;
found
true
select 'true' as found from t9 
where c1= 20 and c13= @arg00 and c14= @arg00 and c15= @arg00 and c16= @arg00
and c17= @arg00 ;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and c13= CAST('1991-01-01 01:01:01' as datetime) and
  c14= CAST('1991-01-01 01:01:01' as datetime) and
  c15= CAST('1991-01-01 01:01:01' as datetime) and
  c16= CAST('1991-01-01 01:01:01' as datetime) and
  c17= CAST('1991-01-01 01:01:01' as datetime)" ;
execute stmt1 ;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and c13= ? and c14= ? and c15= ? and c16= ? and c17= ?" ;
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00 ;
found
true
set @arg00= 1991 ;
select 'true' as found from t9 
where c1= 20 and c17= 1991 ;
found
true
select 'true' as found from t9 
where c1= 20 and c17= @arg00 ;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and c17= 1991" ;
execute stmt1 ;
found
true
prepare stmt1 from "select 'true' as found from t9
where c1= 20 and c17= ?" ;
execute stmt1 using @arg00 ;
found
true
set @arg00= 1.991e+3 ;
select 'true' as found from t9 
where c1= 20 and abs(c17 - 1.991e+3) < 0.01 ;
found
true
select 'true' as found from t9 
where c1= 20 and abs(c17 - @arg00) < 0.01 ;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and abs(c17 - 1.991e+3) < 0.01" ;
execute stmt1 ;
found
true
prepare stmt1 from "select 'true' as found from t9
where c1= 20 and abs(c17 - ?) < 0.01" ;
execute stmt1 using @arg00 ;
found
true
drop table t1, t9 ;
create table t1
(
a int, b varchar(30),
primary key(a)
) ENGINE = MERGE UNION=(t1_1,t1_2)
INSERT_METHOD=LAST;
create table t9
(
c1  tinyint, c2  smallint, c3  mediumint, c4  int,
c5  integer, c6  bigint, c7  float, c8  double,
c9  double precision, c10 real, c11 decimal(7, 4), c12 numeric(8, 4),
c13 date, c14 datetime, c15 timestamp(14), c16 time,
c17 year, c18 bit, c19 bool, c20 char,
c21 char(10), c22 varchar(30), c23 tinyblob, c24 tinytext,
c25 blob, c26 text, c27 mediumblob, c28 mediumtext,
c29 longblob, c30 longtext, c31 enum('one', 'two', 'three'),
c32 set('monday', 'tuesday', 'wednesday'),
primary key(c1)
)  ENGINE = MERGE UNION=(t9_1,t9_2)
INSERT_METHOD=LAST;
delete from t1 ;
insert into t1 values (1,'one');
insert into t1 values (2,'two');
insert into t1 values (3,'three');
insert into t1 values (4,'four');
commit ;
delete from t9 ;
insert into t9
set c1= 1, c2= 1, c3= 1, c4= 1, c5= 1, c6= 1, c7= 1, c8= 1, c9= 1,
c10= 1, c11= 1, c12 = 1,
c13= '2004-02-29', c14= '2004-02-29 11:11:11', c15= '2004-02-29 11:11:11',
c16= '11:11:11', c17= '2004',
c18= 1, c19=true, c20= 'a', c21= '123456789a', 
c22= '123456789a123456789b123456789c', c23= 'tinyblob', c24= 'tinytext',
c25= 'blob', c26= 'text', c27= 'mediumblob', c28= 'mediumtext',
c29= 'longblob', c30= 'longtext', c31='one', c32= 'monday';
insert into t9
set c1= 9, c2= 9, c3= 9, c4= 9, c5= 9, c6= 9, c7= 9, c8= 9, c9= 9,
c10= 9, c11= 9, c12 = 9,
c13= '2004-02-29', c14= '2004-02-29 11:11:11', c15= '2004-02-29 11:11:11',
c16= '11:11:11', c17= '2004',
c18= 1, c19=false, c20= 'a', c21= '123456789a', 
c22= '123456789a123456789b123456789c', c23= 'tinyblob', c24= 'tinytext',
c25= 'blob', c26= 'text', c27= 'mediumblob', c28= 'mediumtext',
c29= 'longblob', c30= 'longtext', c31='two', c32= 'tuesday';
commit ;
test_sequence
------ simple select tests ------
prepare stmt1 from ' select * from t9 order by c1 ' ;
execute stmt1;
Catalog	Database	Table	Table_alias	Column	Column_alias	Name	Type	Length	Max length	Is_null	Flags	Decimals	Charsetnr
def	test	t9	t9	c1	c1	1	4	1	N	49155	0	63
def	test	t9	t9	c2	c2	2	6	1	Y	32768	0	63
def	test	t9	t9	c3	c3	9	9	1	Y	32768	0	63
def	test	t9	t9	c4	c4	3	11	1	Y	32768	0	63
def	test	t9	t9	c5	c5	3	11	1	Y	32768	0	63
def	test	t9	t9	c6	c6	8	20	1	Y	32768	0	63
def	test	t9	t9	c7	c7	4	12	1	Y	32768	31	63
def	test	t9	t9	c8	c8	5	22	1	Y	32768	31	63
def	test	t9	t9	c9	c9	5	22	1	Y	32768	31	63
def	test	t9	t9	c10	c10	5	22	1	Y	32768	31	63
def	test	t9	t9	c11	c11	0	9	6	Y	32768	4	63
def	test	t9	t9	c12	c12	0	10	6	Y	32768	4	63
def	test	t9	t9	c13	c13	10	10	10	Y	128	0	63
def	test	t9	t9	c14	c14	12	19	19	Y	128	0	63
def	test	t9	t9	c15	c15	7	19	19	N	1249	0	63
def	test	t9	t9	c16	c16	11	8	8	Y	128	0	63
def	test	t9	t9	c17	c17	13	4	4	Y	32864	0	63
def	test	t9	t9	c18	c18	1	1	1	Y	32768	0	63
def	test	t9	t9	c19	c19	1	1	1	Y	32768	0	63
def	test	t9	t9	c20	c20	254	1	1	Y	0	0	8
def	test	t9	t9	c21	c21	253	10	10	Y	0	0	8
def	test	t9	t9	c22	c22	253	30	30	Y	0	0	8
def	test	t9	t9	c23	c23	252	255	8	Y	144	0	63
def	test	t9	t9	c24	c24	252	255	8	Y	16	0	8
def	test	t9	t9	c25	c25	252	65535	4	Y	144	0	63
def	test	t9	t9	c26	c26	252	65535	4	Y	16	0	8
def	test	t9	t9	c27	c27	252	16777215	10	Y	144	0	63
def	test	t9	t9	c28	c28	252	16777215	10	Y	16	0	8
def	test	t9	t9	c29	c29	252	16777215	8	Y	144	0	63
def	test	t9	t9	c30	c30	252	16777215	8	Y	16	0	8
def	test	t9	t9	c31	c31	254	5	3	Y	256	0	8
def	test	t9	t9	c32	c32	254	24	7	Y	2048	0	8
c1	c2	c3	c4	c5	c6	c7	c8	c9	c10	c11	c12	c13	c14	c15	c16	c17	c18	c19	c20	c21	c22	c23	c24	c25	c26	c27	c28	c29	c30	c31	c32
1	1	1	1	1	1	1	1	1	1	1.0000	1.0000	2004-02-29	2004-02-29 11:11:11	2004-02-29 11:11:11	11:11:11	2004	1	1	a	123456789a	123456789a123456789b123456789c	tinyblob	tinytext	blob	text	mediumblob	mediumtext	longblob	longtext	one	monday
9	9	9	9	9	9	9	9	9	9	9.0000	9.0000	2004-02-29	2004-02-29 11:11:11	2004-02-29 11:11:11	11:11:11	2004	1	0	a	123456789a	123456789a123456789b123456789c	tinyblob	tinytext	blob	text	mediumblob	mediumtext	longblob	longtext	two	tuesday
set @arg00='SELECT' ;
prepare stmt1 from ' ? a from t1 where a=1 ';
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '? a from t1 where a=1' at line 1
set @arg00=1 ;
select @arg00, b from t1 where a=1 ;
@arg00	b
1	one
prepare stmt1 from ' select ?, b from t1 where a=1 ' ;
execute stmt1 using @arg00 ;
?	b
1	one
set @arg00='lion' ;
select @arg00, b from t1 where a=1 ;
@arg00	b
lion	one
prepare stmt1 from ' select ?, b from t1 where a=1 ' ;
execute stmt1 using @arg00 ;
?	b
lion	one
set @arg00=NULL ;
select @arg00, b from t1 where a=1 ;
@arg00	b
NULL	one
prepare stmt1 from ' select ?, b from t1 where a=1 ' ;
execute stmt1 using @arg00 ;
?	b
NULL	one
set @arg00=1 ;
select b, a - @arg00 from t1 where a=1 ;
b	a - @arg00
one	0
prepare stmt1 from ' select b, a - ? from t1 where a=1 ' ;
execute stmt1 using @arg00 ;
b	a - ?
one	0
set @arg00=null ;
select @arg00 as my_col ;
my_col
NULL
prepare stmt1 from ' select ? as my_col';
execute stmt1 using @arg00 ;
my_col
NULL
select @arg00 + 1 as my_col ;
my_col
NULL
prepare stmt1 from ' select ? + 1 as my_col';
execute stmt1 using @arg00 ;
my_col
NULL
select 1 + @arg00 as my_col ;
my_col
NULL
prepare stmt1 from ' select 1 + ? as my_col';
execute stmt1 using @arg00 ;
my_col
NULL
set @arg00='MySQL' ;
select substr(@arg00,1,2) from t1 where a=1 ;
substr(@arg00,1,2)
My
prepare stmt1 from ' select substr(?,1,2) from t1 where a=1 ' ;
execute stmt1 using @arg00 ;
substr(?,1,2)
My
set @arg00=3 ;
select substr('MySQL',@arg00,5) from t1 where a=1 ;
substr('MySQL',@arg00,5)
SQL
prepare stmt1 from ' select substr(''MySQL'',?,5) from t1 where a=1 ' ;
execute stmt1 using @arg00 ;
substr('MySQL',?,5)
SQL
select substr('MySQL',1,@arg00) from t1 where a=1 ;
substr('MySQL',1,@arg00)
MyS
prepare stmt1 from ' select substr(''MySQL'',1,?) from t1 where a=1 ' ;
execute stmt1 using @arg00 ;
substr('MySQL',1,?)
MyS
set @arg00='MySQL' ;
select a , concat(@arg00,b) from t1 order by a;
a	concat(@arg00,b)
1	MySQLone
2	MySQLtwo
3	MySQLthree
4	MySQLfour
prepare stmt1 from ' select a , concat(?,b) from t1 order by a ' ;
execute stmt1 using @arg00;
a	concat(?,b)
1	MySQLone
2	MySQLtwo
3	MySQLthree
4	MySQLfour
select a , concat(b,@arg00) from t1 order by a ;
a	concat(b,@arg00)
1	oneMySQL
2	twoMySQL
3	threeMySQL
4	fourMySQL
prepare stmt1 from ' select a , concat(b,?) from t1 order by a ' ;
execute stmt1 using @arg00;
a	concat(b,?)
1	oneMySQL
2	twoMySQL
3	threeMySQL
4	fourMySQL
set @arg00='MySQL' ;
select group_concat(@arg00,b order by a) from t1 
group by 'a' ;
group_concat(@arg00,b order by a)
MySQLone,MySQLtwo,MySQLthree,MySQLfour
prepare stmt1 from ' select group_concat(?,b order by a) from t1
group by ''a'' ' ;
execute stmt1 using @arg00;
group_concat(?,b order by a)
MySQLone,MySQLtwo,MySQLthree,MySQLfour
select group_concat(b,@arg00 order by a) from t1 
group by 'a' ;
group_concat(b,@arg00 order by a)
oneMySQL,twoMySQL,threeMySQL,fourMySQL
prepare stmt1 from ' select group_concat(b,? order by a) from t1
group by ''a'' ' ;
execute stmt1 using @arg00;
group_concat(b,? order by a)
oneMySQL,twoMySQL,threeMySQL,fourMySQL
set @arg00='first' ;
set @arg01='second' ;
set @arg02=NULL;
select @arg00, @arg01 from t1 where a=1 ;
@arg00	@arg01
first	second
prepare stmt1 from ' select ?, ? from t1 where a=1 ' ;
execute stmt1 using @arg00, @arg01 ;
?	?
first	second
execute stmt1 using @arg02, @arg01 ;
?	?
NULL	second
execute stmt1 using @arg00, @arg02 ;
?	?
first	NULL
execute stmt1 using @arg02, @arg02 ;
?	?
NULL	NULL
drop table if exists t5 ;
create table t5 (id1 int(11) not null default '0',
value2 varchar(100), value1 varchar(100)) ;
insert into t5 values (1,'hh','hh'),(2,'hh','hh'),
(1,'ii','ii'),(2,'ii','ii') ;
prepare stmt1 from ' select id1,value1 from t5 where id1=? or value1=? order by id1,value1 ' ;
set @arg00=1 ;
set @arg01='hh' ;
execute stmt1 using @arg00, @arg01 ;
id1	value1
1	hh
1	ii
2	hh
drop table t5 ;
drop table if exists t5 ;
create table t5(session_id  char(9) not null) ;
insert into t5 values ('abc') ;
prepare stmt1 from ' select * from t5
where ?=''1111'' and session_id = ''abc'' ' ;
set @arg00='abc' ;
execute stmt1 using @arg00 ;
session_id
set @arg00='1111' ;
execute stmt1 using @arg00 ;
session_id
abc
set @arg00='abc' ;
execute stmt1 using @arg00 ;
session_id
drop table t5 ;
set @arg00='FROM' ;
select a @arg00 t1 where a=1 ;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '@arg00 t1 where a=1' at line 1
prepare stmt1 from ' select a ? t1 where a=1 ' ;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '? t1 where a=1' at line 1
set @arg00='t1' ;
select a from @arg00 where a=1 ;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '@arg00 where a=1' at line 1
prepare stmt1 from ' select a from ? where a=1 ' ;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '? where a=1' at line 1
set @arg00='WHERE' ;
select a from t1 @arg00 a=1 ;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '@arg00 a=1' at line 1
prepare stmt1 from ' select a from t1 ? a=1 ' ;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '? a=1' at line 1
set @arg00=1 ;
select a FROM t1 where a=@arg00 ;
a
1
prepare stmt1 from ' select a FROM t1 where a=? ' ;
execute stmt1 using @arg00 ;
a
1
set @arg00=1000 ;
execute stmt1 using @arg00 ;
a
set @arg00=NULL ;
select a FROM t1 where a=@arg00 ;
a
prepare stmt1 from ' select a FROM t1 where a=? ' ;
execute stmt1 using @arg00 ;
a
set @arg00=4 ;
select a FROM t1 where a=sqrt(@arg00) ;
a
2
prepare stmt1 from ' select a FROM t1 where a=sqrt(?) ' ;
execute stmt1 using @arg00 ;
a
2
set @arg00=NULL ;
select a FROM t1 where a=sqrt(@arg00) ;
a
prepare stmt1 from ' select a FROM t1 where a=sqrt(?) ' ;
execute stmt1 using @arg00 ;
a
set @arg00=2 ;
set @arg01=3 ;
select a FROM t1 where a in (@arg00,@arg01) order by a;
a
2
3
prepare stmt1 from ' select a FROM t1 where a in (?,?) order by a ';
execute stmt1 using @arg00, @arg01;
a
2
3
set @arg00= 'one' ;
set @arg01= 'two' ;
set @arg02= 'five' ;
prepare stmt1 from ' select b FROM t1 where b in (?,?,?) order by b ' ;
execute stmt1 using @arg00, @arg01, @arg02 ;
b
one
two
prepare stmt1 from ' select b FROM t1 where b like ? ';
set @arg00='two' ;
execute stmt1 using @arg00 ;
b
two
set @arg00='tw%' ;
execute stmt1 using @arg00 ;
b
two
set @arg00='%wo' ;
execute stmt1 using @arg00 ;
b
two
set @arg00=null ;
insert into t9 set c1= 0, c5 = NULL ;
select c5 from t9 where c5 > NULL ;
c5
prepare stmt1 from ' select c5 from t9 where c5 > ? ';
execute stmt1 using @arg00 ;
c5
select c5 from t9 where c5 < NULL ;
c5
prepare stmt1 from ' select c5 from t9 where c5 < ? ';
execute stmt1 using @arg00 ;
c5
select c5 from t9 where c5 = NULL ;
c5
prepare stmt1 from ' select c5 from t9 where c5 = ? ';
execute stmt1 using @arg00 ;
c5
select c5 from t9 where c5 <=> NULL ;
c5
NULL
prepare stmt1 from ' select c5 from t9 where c5 <=> ? ';
execute stmt1 using @arg00 ;
c5
NULL
delete from t9 where c1= 0 ;
set @arg00='>' ;
select a FROM t1 where a @arg00 1 ;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '@arg00 1' at line 1
prepare stmt1 from ' select a FROM t1 where a ? 1 ' ;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '? 1' at line 1
set @arg00=1 ;
select a,b FROM t1 where a is not NULL
AND b is not NULL group by a - @arg00 ;
a	b
1	one
2	two
3	three
4	four
prepare stmt1 from ' select a,b FROM t1 where a is not NULL
AND b is not NULL group by a - ? ' ;
execute stmt1 using @arg00 ;
a	b
1	one
2	two
3	three
4	four
set @arg00='two' ;
select a,b FROM t1 where a is not NULL
AND b is not NULL having b <> @arg00 order by a ;
a	b
1	one
3	three
4	four
prepare stmt1 from ' select a,b FROM t1 where a is not NULL
AND b is not NULL having b <> ? order by a ' ;
execute stmt1 using @arg00 ;
a	b
1	one
3	three
4	four
set @arg00=1 ;
select a,b FROM t1 where a is not NULL
AND b is not NULL order by a - @arg00 ;
a	b
1	one
2	two
3	three
4	four
prepare stmt1 from ' select a,b FROM t1 where a is not NULL
AND b is not NULL order by a - ? ' ;
execute stmt1 using @arg00 ;
a	b
1	one
2	two
3	three
4	four
set @arg00=2 ;
select a,b from t1 order by 2 ;
a	b
4	four
1	one
3	three
2	two
prepare stmt1 from ' select a,b from t1
order by ? ';
execute stmt1 using @arg00;
a	b
4	four
1	one
3	three
2	two
set @arg00=1 ;
execute stmt1 using @arg00;
a	b
1	one
2	two
3	three
4	four
set @arg00=0 ;
execute stmt1 using @arg00;
ERROR 42S22: Unknown column '?' in 'order clause'
set @arg00=1;
prepare stmt1 from ' select a,b from t1 order by a
limit 1 ';
execute stmt1 ;
a	b
1	one
prepare stmt1 from ' select a,b from t1
limit ? ';
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '?' at line 2
set @arg00='b' ;
set @arg01=0 ;
set @arg02=2 ;
set @arg03=2 ;
select sum(a), @arg00 from t1 where a > @arg01
and b is not null group by substr(b,@arg02)
having sum(a) <> @arg03 ;
sum(a)	@arg00
3	b
1	b
4	b
prepare stmt1 from ' select sum(a), ? from t1 where a > ?
and b is not null group by substr(b,?)
having sum(a) <> ? ';
execute stmt1 using @arg00, @arg01, @arg02, @arg03;
sum(a)	?
3	b
1	b
4	b
test_sequence
------ join tests ------
select first.a as a1, second.a as a2 
from t1 first, t1 second
where first.a = second.a order by a1 ;
a1	a2
1	1
2	2
3	3
4	4
prepare stmt1 from ' select first.a as a1, second.a as a2 
        from t1 first, t1 second
        where first.a = second.a order by a1 ';
execute stmt1 ;
a1	a2
1	1
2	2
3	3
4	4
set @arg00='ABC';
set @arg01='two';
set @arg02='one';
select first.a, @arg00, second.a FROM t1 first, t1 second
where @arg01 = first.b or first.a = second.a or second.b = @arg02
order by second.a, first.a;
a	@arg00	a
1	ABC	1
2	ABC	1
3	ABC	1
4	ABC	1
2	ABC	2
2	ABC	3
3	ABC	3
2	ABC	4
4	ABC	4
prepare stmt1 from ' select first.a, ?, second.a FROM t1 first, t1 second
                    where ? = first.b or first.a = second.a or second.b = ?
                    order by second.a, first.a';
execute stmt1 using @arg00, @arg01, @arg02;
a	?	a
1	ABC	1
2	ABC	1
3	ABC	1
4	ABC	1
2	ABC	2
2	ABC	3
3	ABC	3
2	ABC	4
4	ABC	4
drop table if exists t2 ;
create table t2 as select * from t1 ;
set @query1= 'SELECT * FROM t2 join t1 on (t1.a=t2.a) order by t2.a ' ;
set @query2= 'SELECT * FROM t2 natural join t1 order by t2.a ' ;
set @query3= 'SELECT * FROM t2 join t1 using(a) order by t2.a ' ;
set @query4= 'SELECT * FROM t2 left join t1 on(t1.a=t2.a) order by t2.a ' ;
set @query5= 'SELECT * FROM t2 natural left join t1 order by t2.a ' ;
set @query6= 'SELECT * FROM t2 left join t1 using(a) order by t2.a ' ;
set @query7= 'SELECT * FROM t2 right join t1 on(t1.a=t2.a) order by t2.a ' ;
set @query8= 'SELECT * FROM t2 natural right join t1 order by t2.a ' ;
set @query9= 'SELECT * FROM t2 right join t1 using(a) order by t2.a ' ;
the join statement is:
SELECT * FROM t2 right join t1 using(a) order by t2.a 
prepare stmt1 from @query9  ;
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
the join statement is:
SELECT * FROM t2 natural right join t1 order by t2.a 
prepare stmt1 from @query8 ;
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
the join statement is:
SELECT * FROM t2 right join t1 on(t1.a=t2.a) order by t2.a 
prepare stmt1 from @query7 ;
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
the join statement is:
SELECT * FROM t2 left join t1 using(a) order by t2.a 
prepare stmt1 from @query6 ;
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
the join statement is:
SELECT * FROM t2 natural left join t1 order by t2.a 
prepare stmt1 from @query5 ;
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
the join statement is:
SELECT * FROM t2 left join t1 on(t1.a=t2.a) order by t2.a 
prepare stmt1 from @query4 ;
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
the join statement is:
SELECT * FROM t2 join t1 using(a) order by t2.a 
prepare stmt1 from @query3 ;
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
the join statement is:
SELECT * FROM t2 natural join t1 order by t2.a 
prepare stmt1 from @query2 ;
execute stmt1 ;
a	b
1	one
2	two
3	three
4	four
execute stmt1 ;
a	b
1	one
2	two
3	three
4	four
execute stmt1 ;
a	b
1	one
2	two
3	three
4	four
the join statement is:
SELECT * FROM t2 join t1 on (t1.a=t2.a) order by t2.a 
prepare stmt1 from @query1 ;
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
execute stmt1 ;
a	b	a	b
1	one	1	one
2	two	2	two
3	three	3	three
4	four	4	four
drop table t2 ;
test_sequence
------ subquery tests ------
prepare stmt1 from ' select a, b FROM t1 outer_table where
   a = (select a from t1 where b = ''two'') ';
execute stmt1 ;
a	b
2	two
set @arg00='two' ;
select a, b FROM t1 outer_table where
a = (select a from t1 where b = 'two' ) and b=@arg00 ;
a	b
2	two
prepare stmt1 from ' select a, b FROM t1 outer_table where
   a = (select a from t1 where b = ''two'') and b=? ';
execute stmt1 using @arg00;
a	b
2	two
set @arg00='two' ;
select a, b FROM t1 outer_table where
a = (select a from t1 where b = @arg00 ) and b='two' ;
a	b
2	two
prepare stmt1 from ' select a, b FROM t1 outer_table where
   a = (select a from t1 where b = ? ) and b=''two'' ' ;
execute stmt1 using @arg00;
a	b
2	two
set @arg00=3 ;
set @arg01='three' ;
select a,b FROM t1 where (a,b) in (select 3, 'three');
a	b
3	three
select a FROM t1 where (a,b) in (select @arg00,@arg01);
a
3
prepare stmt1 from ' select a FROM t1 where (a,b) in (select ?, ?) ';
execute stmt1 using @arg00, @arg01;
a
3
set @arg00=1 ;
set @arg01='two' ;
set @arg02=2 ;
set @arg03='two' ;
select a, @arg00, b FROM t1 outer_table where
b=@arg01 and a = (select @arg02 from t1 where b = @arg03 ) ;
a	@arg00	b
2	1	two
prepare stmt1 from ' select a, ?, b FROM t1 outer_table where
   b=? and a = (select ? from t1 where b = ? ) ' ;
execute stmt1 using @arg00, @arg01, @arg02, @arg03 ;
a	?	b
2	1	two
prepare stmt1 from ' select a, b FROM t1 outer_table where
   a = (select a from t1 where b = outer_table.b ) order by a ';
execute stmt1 ;
a	b
1	one
2	two
3	three
4	four
prepare stmt1 from ' SELECT a as ccc from t1 where a+1=
                           (SELECT 1+ccc from t1 where ccc+1=a+1 and a=1) ';
execute stmt1 ;
ccc
1
deallocate prepare stmt1 ;
prepare stmt1 from ' SELECT a as ccc from t1 where a+1=
                           (SELECT 1+ccc from t1 where ccc+1=a+1 and a=1) ';
execute stmt1 ;
ccc
1
deallocate prepare stmt1 ;
prepare stmt1 from ' SELECT a as ccc from t1 where a+1=
                           (SELECT 1+ccc from t1 where ccc+1=a+1 and a=1) ';
execute stmt1 ;
ccc
1
deallocate prepare stmt1 ;
set @arg00='two' ;
select a, b FROM t1 outer_table where
a = (select a from t1 where b = outer_table.b ) and b=@arg00 ;
a	b
2	two
prepare stmt1 from ' select a, b FROM t1 outer_table where
   a = (select a from t1 where b = outer_table.b) and b=? ';
execute stmt1 using @arg00;
a	b
2	two
set @arg00=2 ;
select a, b FROM t1 outer_table where
a = (select a from t1 where a = @arg00 and b = outer_table.b) and b='two' ;
a	b
2	two
prepare stmt1 from ' select a, b FROM t1 outer_table where
   a = (select a from t1 where a = ? and b = outer_table.b) and b=''two'' ' ;
execute stmt1 using @arg00;
a	b
2	two
set @arg00=2 ;
select a, b FROM t1 outer_table where
a = (select a from t1 where outer_table.a = @arg00 and a=2) and b='two' ;
a	b
2	two
prepare stmt1 from ' select a, b FROM t1 outer_table where
   a = (select a from t1 where outer_table.a = ? and a=2) and b=''two'' ' ;
execute stmt1 using @arg00;
a	b
2	two
set @arg00=1 ;
set @arg01='two' ;
set @arg02=2 ;
set @arg03='two' ;
select a, @arg00, b FROM t1 outer_table where
b=@arg01 and a = (select @arg02 from t1 where outer_table.b = @arg03
and outer_table.a=a ) ;
a	@arg00	b
2	1	two
prepare stmt1 from ' select a, ?, b FROM t1 outer_table where
   b=? and a = (select ? from t1 where outer_table.b = ? 
                   and outer_table.a=a ) ' ;
execute stmt1 using @arg00, @arg01, @arg02, @arg03 ;
a	?	b
2	1	two
set @arg00=1 ;
set @arg01=0 ;
select a, @arg00 
from ( select a - @arg00 as a from t1 where a=@arg00 ) as t2
where a=@arg01;
a	@arg00
0	1
prepare stmt1 from ' select a, ? 
                    from ( select a - ? as a from t1 where a=? ) as t2
                    where a=? ';
execute stmt1 using @arg00, @arg00, @arg00, @arg01 ;
a	?
0	1
drop table if exists t2 ;
create table t2 as select * from t1;
prepare stmt1 from ' select a in (select a from t2) from t1 ' ;
execute stmt1 ;
a in (select a from t2)
1
1
1
1
drop table if exists t5, t6, t7 ;
create table t5 (a int , b int) ;
create table t6 like t5 ;
create table t7 like t5 ;
insert into t5 values (0, 100), (1, 2), (1, 3), (2, 2), (2, 7),
(2, -1), (3, 10) ;
insert into t6 values (0, 0), (1, 1), (2, 1), (3, 1), (4, 1) ;
insert into t7 values (3, 3), (2, 2), (1, 1) ;
prepare stmt1 from ' select a, (select count(distinct t5.b) as sum from t5, t6
                     where t5.a=t6.a and t6.b > 0 and t5.a <= t7.b
                     group by t5.a order by sum limit 1) from t7 ' ;
execute stmt1 ;
a	(select count(distinct t5.b) as sum from t5, t6
                     where t5.a=t6.a and t6.b > 0 and t5.a <= t7.b
                     group by t5.a order by sum limit 1)
3	1
2	2
1	2
execute stmt1 ;
a	(select count(distinct t5.b) as sum from t5, t6
                     where t5.a=t6.a and t6.b > 0 and t5.a <= t7.b
                     group by t5.a order by sum limit 1)
3	1
2	2
1	2
execute stmt1 ;
a	(select count(distinct t5.b) as sum from t5, t6
                     where t5.a=t6.a and t6.b > 0 and t5.a <= t7.b
                     group by t5.a order by sum limit 1)
3	1
2	2
1	2
drop table t5, t6, t7 ;
drop table if exists t2 ;
create table t2 as select * from t9;
set @stmt= ' SELECT
   (SELECT SUM(c1 + c12 + 0.0) FROM t2 
    where (t9.c2 - 0e-3) = t2.c2
    GROUP BY t9.c15 LIMIT 1) as scalar_s,
   exists (select 1.0e+0 from t2 
           where t2.c3 * 9.0000000000 = t9.c4) as exists_s,
   c5 * 4 in (select c6 + 0.3e+1 from t2) as in_s,
   (c7 - 4, c8 - 4) in (select c9 + 4.0, c10 + 40e-1 from t2) as in_row_s
FROM t9,
(select c25 x, c32 y from t2) tt WHERE x = c25 ' ;
prepare stmt1 from @stmt ;
execute stmt1 ;
execute stmt1 ;
set @stmt= concat('explain ',@stmt);
prepare stmt1 from @stmt ;
execute stmt1 ;
execute stmt1 ;
set @stmt= ' SELECT
   (SELECT SUM(c1+c12+?) FROM t2 where (t9.c2-?)=t2.c2
    GROUP BY t9.c15 LIMIT 1) as scalar_s,
   exists (select ? from t2 
           where t2.c3*?=t9.c4) as exists_s,
   c5*? in (select c6+? from t2) as in_s,
   (c7-?, c8-?) in (select c9+?, c10+? from t2) as in_row_s
FROM t9,
(select c25 x, c32 y from t2) tt WHERE x =c25 ' ;
set @arg00= 0.0 ;
set @arg01= 0e-3 ;
set @arg02= 1.0e+0 ;
set @arg03= 9.0000000000 ;
set @arg04= 4 ;
set @arg05= 0.3e+1 ;
set @arg06= 4 ;
set @arg07= 4 ;
set @arg08= 4.0 ;
set @arg09= 40e-1 ;
prepare stmt1 from @stmt ;
execute stmt1 using @arg00, @arg01, @arg02, @arg03, @arg04, @arg05, @arg06,
@arg07, @arg08, @arg09 ;
execute stmt1 using @arg00, @arg01, @arg02, @arg03, @arg04, @arg05, @arg06,
@arg07, @arg08, @arg09 ;
set @stmt= concat('explain ',@stmt);
prepare stmt1 from @stmt ;
execute stmt1 using @arg00, @arg01, @arg02, @arg03, @arg04, @arg05, @arg06,
@arg07, @arg08, @arg09 ;
execute stmt1 using @arg00, @arg01, @arg02, @arg03, @arg04, @arg05, @arg06,
@arg07, @arg08, @arg09 ;
drop table t2 ;
select 1 < (select a from t1) ;
ERROR 21000: Subquery returns more than 1 row
prepare stmt1 from ' select 1 < (select a from t1) ' ;
execute stmt1 ;
ERROR 21000: Subquery returns more than 1 row
select 1 as my_col ;
my_col
1
test_sequence
------ union tests ------
prepare stmt1 from ' select a FROM t1 where a=1
                     union distinct
                     select a FROM t1 where a=1 ';
execute stmt1 ;
a
1
execute stmt1 ;
a
1
prepare stmt1 from ' select a FROM t1 where a=1
                     union all
                     select a FROM t1 where a=1 ';
execute stmt1 ;
a
1
1
prepare stmt1 from ' SELECT 1, 2 union SELECT 1 ' ;
ERROR 21000: The used SELECT statements have a different number of columns
prepare stmt1 from ' SELECT 1 union SELECT 1, 2 ' ;
ERROR 21000: The used SELECT statements have a different number of columns
prepare stmt1 from ' SELECT * from t1 union SELECT 1 ' ;
ERROR 21000: The used SELECT statements have a different number of columns
prepare stmt1 from ' SELECT 1 union SELECT * from t1 ' ;
ERROR 21000: The used SELECT statements have a different number of columns
set @arg00=1 ;
select @arg00 FROM t1 where a=1
union distinct
select 1 FROM t1 where a=1;
@arg00
1
prepare stmt1 from ' select ? FROM t1 where a=1
                     union distinct
                     select 1 FROM t1 where a=1 ' ;
execute stmt1 using @arg00;
?
1
set @arg00=1 ;
select 1 FROM t1 where a=1
union distinct
select @arg00 FROM t1 where a=1;
1
1
prepare stmt1 from ' select 1 FROM t1 where a=1
                     union distinct
                     select ? FROM t1 where a=1 ' ;
execute stmt1 using @arg00;
1
1
set @arg00='a' ;
select @arg00 FROM t1 where a=1
union distinct
select @arg00 FROM t1 where a=1;
@arg00
a
prepare stmt1 from ' select ? FROM t1 where a=1
                     union distinct
                     select ? FROM t1 where a=1 ';
execute stmt1 using @arg00, @arg00;
?
a
prepare stmt1 from ' select ? 
                     union distinct
                     select ? ';
execute stmt1 using @arg00, @arg00;
?
a
set @arg00='a' ;
set @arg01=1 ;
set @arg02='a' ;
set @arg03=2 ;
select @arg00 FROM t1 where a=@arg01
union distinct
select @arg02 FROM t1 where a=@arg03;
@arg00
a
prepare stmt1 from ' select ? FROM t1 where a=?
                     union distinct
                     select ? FROM t1 where a=? ' ;
execute stmt1 using @arg00, @arg01, @arg02, @arg03;
?
a
set @arg00=1 ;
prepare stmt1 from ' select sum(a) + 200, ? from t1
union distinct
select sum(a) + 200, 1 from t1
group by b ' ;
execute stmt1 using @arg00;
sum(a) + 200	?
210	1
204	1
201	1
203	1
202	1
set @Oporto='Oporto' ;
set @Lisboa='Lisboa' ;
set @0=0 ;
set @1=1 ;
set @2=2 ;
set @3=3 ;
set @4=4 ;
select @Oporto,@Lisboa,@0,@1,@2,@3,@4 ;
@Oporto	@Lisboa	@0	@1	@2	@3	@4
Oporto	Lisboa	0	1	2	3	4
select sum(a) + 200 as the_sum, @Oporto as the_town from t1
group by b
union distinct
select sum(a) + 200, @Lisboa from t1
group by b ;
the_sum	the_town
204	Oporto
201	Oporto
203	Oporto
202	Oporto
204	Lisboa
201	Lisboa
203	Lisboa
202	Lisboa
prepare stmt1 from ' select sum(a) + 200 as the_sum, ? as the_town from t1
                     group by b
                     union distinct
                     select sum(a) + 200, ? from t1
                     group by b ' ;
execute stmt1 using @Oporto, @Lisboa;
the_sum	the_town
204	Oporto
201	Oporto
203	Oporto
202	Oporto
204	Lisboa
201	Lisboa
203	Lisboa
202	Lisboa
select sum(a) + 200 as the_sum, @Oporto as the_town from t1
where a > @1
group by b
union distinct
select sum(a) + 200, @Lisboa from t1
where a > @2
group by b ;
the_sum	the_town
204	Oporto
203	Oporto
202	Oporto
204	Lisboa
203	Lisboa
prepare stmt1 from ' select sum(a) + 200 as the_sum, ? as the_town from t1
                     where a > ?
                     group by b
                     union distinct
                     select sum(a) + 200, ? from t1
                     where a > ?
                     group by b ' ;
execute stmt1 using @Oporto, @1, @Lisboa, @2;
the_sum	the_town
204	Oporto
203	Oporto
202	Oporto
204	Lisboa
203	Lisboa
select sum(a) + 200 as the_sum, @Oporto as the_town from t1
where a > @1
group by b
having avg(a) > @2
union distinct
select sum(a) + 200, @Lisboa from t1
where a > @2
group by b 
having avg(a) > @3;
the_sum	the_town
204	Oporto
203	Oporto
204	Lisboa
prepare stmt1 from ' select sum(a) + 200 as the_sum, ? as the_town from t1
                     where a > ?
                     group by b
                     having avg(a) > ?
                     union distinct
                     select sum(a) + 200, ? from t1
                     where a > ?
                     group by b
                     having avg(a) > ? ';
execute stmt1 using @Oporto, @1, @2, @Lisboa, @2, @3;
the_sum	the_town
204	Oporto
203	Oporto
204	Lisboa
test_sequence
------ explain select tests ------
prepare stmt1 from ' explain select * from t9 ' ;
execute stmt1;
Catalog	Database	Table	Table_alias	Column	Column_alias	Name	Type	Length	Max length	Is_null	Flags	Decimals	Charsetnr
def					id	8	3	1	N	32801	0	8
def					select_type	253	19	6	N	1	31	33
def					table	253	64	2	N	1	31	33
def					type	253	10	3	N	1	31	33
def					possible_keys	253	4096	0	Y	0	31	33
def					key	253	64	0	Y	0	31	33
def					key_len	8	3	0	Y	32800	0	8
def					ref	253	1024	0	Y	0	31	33
def					rows	8	10	1	N	32801	0	8
def					Extra	253	255	0	N	1	31	33
id	select_type	table	type	possible_keys	key	key_len	ref	rows	Extra
1	SIMPLE	t9	ALL	NULL	NULL	NULL	NULL	2	
test_sequence
------ delete tests ------
delete from t1 ;
insert into t1 values (1,'one');
insert into t1 values (2,'two');
insert into t1 values (3,'three');
insert into t1 values (4,'four');
commit ;
delete from t9 ;
insert into t9
set c1= 1, c2= 1, c3= 1, c4= 1, c5= 1, c6= 1, c7= 1, c8= 1, c9= 1,
c10= 1, c11= 1, c12 = 1,
c13= '2004-02-29', c14= '2004-02-29 11:11:11', c15= '2004-02-29 11:11:11',
c16= '11:11:11', c17= '2004',
c18= 1, c19=true, c20= 'a', c21= '123456789a', 
c22= '123456789a123456789b123456789c', c23= 'tinyblob', c24= 'tinytext',
c25= 'blob', c26= 'text', c27= 'mediumblob', c28= 'mediumtext',
c29= 'longblob', c30= 'longtext', c31='one', c32= 'monday';
insert into t9
set c1= 9, c2= 9, c3= 9, c4= 9, c5= 9, c6= 9, c7= 9, c8= 9, c9= 9,
c10= 9, c11= 9, c12 = 9,
c13= '2004-02-29', c14= '2004-02-29 11:11:11', c15= '2004-02-29 11:11:11',
c16= '11:11:11', c17= '2004',
c18= 1, c19=false, c20= 'a', c21= '123456789a', 
c22= '123456789a123456789b123456789c', c23= 'tinyblob', c24= 'tinytext',
c25= 'blob', c26= 'text', c27= 'mediumblob', c28= 'mediumtext',
c29= 'longblob', c30= 'longtext', c31='two', c32= 'tuesday';
commit ;
prepare stmt1 from 'delete from t1 where a=2' ;
execute stmt1;
select a,b from t1 where a=2;
a	b
execute stmt1;
insert into t1 values(0,NULL);
set @arg00=NULL;
prepare stmt1 from 'delete from t1 where b=?' ;
execute stmt1 using @arg00;
select a,b from t1 where b is NULL ;
a	b
0	NULL
set @arg00='one';
execute stmt1 using @arg00;
select a,b from t1 where b=@arg00;
a	b
prepare stmt1 from 'truncate table t1' ;
ERROR HY000: This command is not supported in the prepared statement protocol yet
test_sequence
------ update tests ------
delete from t1 ;
insert into t1 values (1,'one');
insert into t1 values (2,'two');
insert into t1 values (3,'three');
insert into t1 values (4,'four');
commit ;
delete from t9 ;
insert into t9
set c1= 1, c2= 1, c3= 1, c4= 1, c5= 1, c6= 1, c7= 1, c8= 1, c9= 1,
c10= 1, c11= 1, c12 = 1,
c13= '2004-02-29', c14= '2004-02-29 11:11:11', c15= '2004-02-29 11:11:11',
c16= '11:11:11', c17= '2004',
c18= 1, c19=true, c20= 'a', c21= '123456789a', 
c22= '123456789a123456789b123456789c', c23= 'tinyblob', c24= 'tinytext',
c25= 'blob', c26= 'text', c27= 'mediumblob', c28= 'mediumtext',
c29= 'longblob', c30= 'longtext', c31='one', c32= 'monday';
insert into t9
set c1= 9, c2= 9, c3= 9, c4= 9, c5= 9, c6= 9, c7= 9, c8= 9, c9= 9,
c10= 9, c11= 9, c12 = 9,
c13= '2004-02-29', c14= '2004-02-29 11:11:11', c15= '2004-02-29 11:11:11',
c16= '11:11:11', c17= '2004',
c18= 1, c19=false, c20= 'a', c21= '123456789a', 
c22= '123456789a123456789b123456789c', c23= 'tinyblob', c24= 'tinytext',
c25= 'blob', c26= 'text', c27= 'mediumblob', c28= 'mediumtext',
c29= 'longblob', c30= 'longtext', c31='two', c32= 'tuesday';
commit ;
prepare stmt1 from 'update t1 set b=''a=two'' where a=2' ;
execute stmt1;
select a,b from t1 where a=2;
a	b
2	a=two
execute stmt1;
select a,b from t1 where a=2;
a	b
2	a=two
set @arg00=NULL;
prepare stmt1 from 'update t1 set b=? where a=2' ;
execute stmt1 using @arg00;
select a,b from t1 where a=2;
a	b
2	NULL
set @arg00='two';
execute stmt1 using @arg00;
select a,b from t1 where a=2;
a	b
2	two
set @arg00=2;
prepare stmt1 from 'update t1 set b=NULL where a=?' ;
execute stmt1 using @arg00;
select a,b from t1 where a=@arg00;
a	b
2	NULL
update t1 set b='two' where a=@arg00;
set @arg00=2000;
execute stmt1 using @arg00;
select a,b from t1 where a=@arg00;
a	b
set @arg00=2;
set @arg01=22;
prepare stmt1 from 'update t1 set a=? where a=?' ;
execute stmt1 using @arg00, @arg00;
select a,b from t1 where a=@arg00;
a	b
2	two
execute stmt1 using @arg01, @arg00;
select a,b from t1 where a=@arg01;
a	b
22	two
execute stmt1 using @arg00, @arg01;
select a,b from t1 where a=@arg00;
a	b
2	two
set @arg00=NULL;
set @arg01=2;
execute stmt1 using @arg00, @arg01;
Warnings:
Warning	1263	Data truncated; NULL supplied to NOT NULL column 'a' at row 1
select a,b from t1 order by a;
a	b
0	two
1	one
3	three
4	four
set @arg00=0;
execute stmt1 using @arg01, @arg00;
select a,b from t1 order by a;
a	b
1	one
2	two
3	three
4	four
set @arg00=23;
set @arg01='two';
set @arg02=2;
set @arg03='two';
set @arg04=2;
drop table if exists t2;
create table t2 as select a,b from t1 ;
prepare stmt1 from 'update t1 set a=? where b=?
                    and a in (select ? from t2
                              where b = ? or a = ?)';
execute stmt1 using @arg00, @arg01, @arg02, @arg03, @arg04 ;
affected rows: 1
info: Rows matched: 1  Changed: 1  Warnings: 0
select a,b from t1 where a = @arg00 ;
a	b
23	two
prepare stmt1 from 'update t1 set a=? where b=?
                    and a not in (select ? from t2
                              where b = ? or a = ?)';
execute stmt1 using @arg04, @arg01, @arg02, @arg03, @arg00 ;
affected rows: 1
info: Rows matched: 1  Changed: 1  Warnings: 0
select a,b from t1 order by a ;
a	b
1	one
2	two
3	three
4	four
drop table t2 ;
create table t2
(
a int, b varchar(30),
primary key(a)
) engine = 'MYISAM'  ;
insert into t2(a,b) select a, b from t1 ;
prepare stmt1 from 'update t1 set a=? where b=?
                    and a in (select ? from t2
                              where b = ? or a = ?)';
execute stmt1 using @arg00, @arg01, @arg02, @arg03, @arg04 ;
affected rows: 1
info: Rows matched: 1  Changed: 1  Warnings: 0
select a,b from t1 where a = @arg00 ;
a	b
23	two
prepare stmt1 from 'update t1 set a=? where b=?
                    and a not in (select ? from t2
                              where b = ? or a = ?)';
execute stmt1 using @arg04, @arg01, @arg02, @arg03, @arg00 ;
affected rows: 1
info: Rows matched: 1  Changed: 1  Warnings: 0
select a,b from t1 order by a ;
a	b
1	one
2	two
3	three
4	four
drop table t2 ;
set @arg00=1;
prepare stmt1 from 'update t1 set b=''bla''
where a=2
limit 1';
execute stmt1 ;
select a,b from t1 where b = 'bla' ;
a	b
2	bla
prepare stmt1 from 'update t1 set b=''bla''
where a=2
limit ?';
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '?' at line 3
test_sequence
------ insert tests ------
delete from t1 ;
insert into t1 values (1,'one');
insert into t1 values (2,'two');
insert into t1 values (3,'three');
insert into t1 values (4,'four');
commit ;
delete from t9 ;
insert into t9
set c1= 1, c2= 1, c3= 1, c4= 1, c5= 1, c6= 1, c7= 1, c8= 1, c9= 1,
c10= 1, c11= 1, c12 = 1,
c13= '2004-02-29', c14= '2004-02-29 11:11:11', c15= '2004-02-29 11:11:11',
c16= '11:11:11', c17= '2004',
c18= 1, c19=true, c20= 'a', c21= '123456789a', 
c22= '123456789a123456789b123456789c', c23= 'tinyblob', c24= 'tinytext',
c25= 'blob', c26= 'text', c27= 'mediumblob', c28= 'mediumtext',
c29= 'longblob', c30= 'longtext', c31='one', c32= 'monday';
insert into t9
set c1= 9, c2= 9, c3= 9, c4= 9, c5= 9, c6= 9, c7= 9, c8= 9, c9= 9,
c10= 9, c11= 9, c12 = 9,
c13= '2004-02-29', c14= '2004-02-29 11:11:11', c15= '2004-02-29 11:11:11',
c16= '11:11:11', c17= '2004',
c18= 1, c19=false, c20= 'a', c21= '123456789a', 
c22= '123456789a123456789b123456789c', c23= 'tinyblob', c24= 'tinytext',
c25= 'blob', c26= 'text', c27= 'mediumblob', c28= 'mediumtext',
c29= 'longblob', c30= 'longtext', c31='two', c32= 'tuesday';
commit ;
prepare stmt1 from 'insert into t1 values(5, ''five'' )';
execute stmt1;
select a,b from t1 where a = 5;
a	b
5	five
set @arg00='six' ;
prepare stmt1 from 'insert into t1 values(6, ? )';
execute stmt1 using @arg00;
select a,b from t1 where b = @arg00;
a	b
6	six
execute stmt1 using @arg00;
ERROR 23000: Duplicate entry '6' for key 1
set @arg00=NULL ;
prepare stmt1 from 'insert into t1 values(0, ? )';
execute stmt1 using @arg00;
select a,b from t1 where b is NULL;
a	b
0	NULL
set @arg00=8 ;
set @arg01='eight' ;
prepare stmt1 from 'insert into t1 values(?, ? )';
execute stmt1 using @arg00, @arg01 ;
select a,b from t1 where b = @arg01;
a	b
8	eight
set @NULL= null ;
set @arg00= 'abc' ;
execute stmt1 using @NULL, @NULL ;
ERROR 23000: Column 'a' cannot be null
execute stmt1 using @NULL, @NULL ;
ERROR 23000: Column 'a' cannot be null
execute stmt1 using @NULL, @arg00 ;
ERROR 23000: Column 'a' cannot be null
execute stmt1 using @NULL, @arg00 ;
ERROR 23000: Column 'a' cannot be null
set @arg01= 10000 + 2 ;
execute stmt1 using @arg01, @arg00 ;
set @arg01= 10000 + 1 ;
execute stmt1 using @arg01, @arg00 ;
select * from t1 where a > 10000 order by a ;
a	b
10001	abc
10002	abc
delete from t1 where a > 10000 ;
set @arg01= 10000 + 2 ;
execute stmt1 using @arg01, @NULL ;
set @arg01= 10000 + 1 ;
execute stmt1 using @arg01, @NULL ;
select * from t1 where a > 10000 order by a ;
a	b
10001	NULL
10002	NULL
delete from t1 where a > 10000 ;
set @arg01= 10000 + 10 ;
execute stmt1 using @arg01, @arg01 ;
set @arg01= 10000 + 9 ;
execute stmt1 using @arg01, @arg01 ;
set @arg01= 10000 + 8 ;
execute stmt1 using @arg01, @arg01 ;
set @arg01= 10000 + 7 ;
execute stmt1 using @arg01, @arg01 ;
set @arg01= 10000 + 6 ;
execute stmt1 using @arg01, @arg01 ;
set @arg01= 10000 + 5 ;
execute stmt1 using @arg01, @arg01 ;
set @arg01= 10000 + 4 ;
execute stmt1 using @arg01, @arg01 ;
set @arg01= 10000 + 3 ;
execute stmt1 using @arg01, @arg01 ;
set @arg01= 10000 + 2 ;
execute stmt1 using @arg01, @arg01 ;
set @arg01= 10000 + 1 ;
execute stmt1 using @arg01, @arg01 ;
select * from t1 where a > 10000 order by a ;
a	b
10001	10001
10002	10002
10003	10003
10004	10004
10005	10005
10006	10006
10007	10007
10008	10008
10009	10009
10010	10010
delete from t1 where a > 10000 ;
set @arg00=81 ;
set @arg01='8-1' ;
set @arg02=82 ;
set @arg03='8-2' ;
prepare stmt1 from 'insert into t1 values(?,?),(?,?)';
execute stmt1 using @arg00, @arg01, @arg02, @arg03 ;
select a,b from t1 where a in (@arg00,@arg02) ;
a	b
81	8-1
82	8-2
set @arg00=9 ;
set @arg01='nine' ;
prepare stmt1 from 'insert into t1 set a=?, b=? ';
execute stmt1 using @arg00, @arg01 ;
select a,b from t1 where a = @arg00 ;
a	b
9	nine
set @arg00=6 ;
set @arg01=1 ;
prepare stmt1 from 'insert into t1 set a=?, b=''sechs''
                    on duplicate key update a=a + ?, b=concat(b,''modified'') ';
execute stmt1 using @arg00, @arg01;
select * from t1 order by a;
a	b
0	NULL
1	one
2	two
3	three
4	four
5	five
7	sixmodified
8	eight
9	nine
81	8-1
82	8-2
set @arg00=81 ;
set @arg01=1 ;
execute stmt1 using @arg00, @arg01;
ERROR 23000: Duplicate entry '82' for key 1
drop table if exists t2 ;
create table t2 (id int auto_increment primary key) 
ENGINE= 'MYISAM'  ;
prepare stmt1 from ' select last_insert_id() ' ;
insert into t2 values (NULL) ;
execute stmt1 ;
last_insert_id()
1
insert into t2 values (NULL) ;
execute stmt1 ;
last_insert_id()
2
drop table t2 ;
set @1000=1000 ;
set @x1000_2="x1000_2" ;
set @x1000_3="x1000_3" ;
set @x1000="x1000" ;
set @1100=1100 ;
set @x1100="x1100" ;
set @100=100 ;
set @updated="updated" ;
insert into t1 values(1000,'x1000_1') ;
insert into t1 values(@1000,@x1000_2),(@1000,@x1000_3)
on duplicate key update a = a + @100, b = concat(b,@updated) ;
select a,b from t1 where a >= 1000 order by a ;
a	b
1000	x1000_3
1100	x1000_1updated
delete from t1 where a >= 1000 ;
insert into t1 values(1000,'x1000_1') ;
prepare stmt1 from ' insert into t1 values(?,?),(?,?)
               on duplicate key update a = a + ?, b = concat(b,?) ';
execute stmt1 using @1000, @x1000_2, @1000, @x1000_3, @100, @updated ;
select a,b from t1 where a >= 1000 order by a ;
a	b
1000	x1000_3
1100	x1000_1updated
delete from t1 where a >= 1000 ;
insert into t1 values(1000,'x1000_1') ;
execute stmt1 using @1000, @x1000_2, @1100, @x1000_3, @100, @updated ;
select a,b from t1 where a >= 1000 order by a ;
a	b
1200	x1000_1updatedupdated
delete from t1 where a >= 1000 ;
prepare stmt1 from ' replace into t1 (a,b) select 100, ''hundred'' ';
execute stmt1;
execute stmt1;
execute stmt1;
test_sequence
------ multi table tests ------
delete from t1 ;
delete from t9 ;
insert into t1(a,b) values (1, 'one'), (2, 'two'), (3, 'three') ;
insert into t9 (c1,c21)
values (1, 'one'), (2, 'two'), (3, 'three') ;
prepare stmt_delete from " delete t1, t9 
  from t1, t9 where t1.a=t9.c1 and t1.b='updated' ";
prepare stmt_update from " update t1, t9 
  set t1.b='updated', t9.c21='updated'
  where t1.a=t9.c1 and t1.a=? ";
prepare stmt_select1 from " select a, b from t1 order by a" ;
prepare stmt_select2 from " select c1, c21 from t9 order by c1" ;
set @arg00= 1 ;
execute stmt_update using @arg00 ;
execute stmt_delete ;
execute stmt_select1 ;
a	b
2	two
3	three
execute stmt_select2 ;
c1	c21
2	two
3	three
set @arg00= @arg00 + 1 ;
execute stmt_update using @arg00 ;
execute stmt_delete ;
execute stmt_select1 ;
a	b
3	three
execute stmt_select2 ;
c1	c21
3	three
set @arg00= @arg00 + 1 ;
execute stmt_update using @arg00 ;
execute stmt_delete ;
execute stmt_select1 ;
a	b
execute stmt_select2 ;
c1	c21
set @arg00= @arg00 + 1 ;
drop table if exists t5 ;
set @arg01= 8;
set @arg02= 8.0;
set @arg03= 80.00000000000e-1;
set @arg04= 'abc' ;
set @arg05= CAST('abc' as binary) ;
set @arg06= '1991-08-05' ;
set @arg07= CAST('1991-08-05' as date);
set @arg08= '1991-08-05 01:01:01' ;
set @arg09= CAST('1991-08-05 01:01:01' as datetime) ;
set @arg10= unix_timestamp('1991-01-01 01:01:01');
set @arg11= YEAR('1991-01-01 01:01:01');
set @arg12= 8 ;
set @arg12= NULL ;
set @arg13= 8.0 ;
set @arg13= NULL ;
set @arg14= 'abc';
set @arg14= NULL ;
set @arg15= CAST('abc' as binary) ;
set @arg15= NULL ;
create table t5 as select
8                           as const01, @arg01 as param01,
8.0                         as const02, @arg02 as param02,
80.00000000000e-1           as const03, @arg03 as param03,
'abc'                       as const04, @arg04 as param04,
CAST('abc' as binary)       as const05, @arg05 as param05,
'1991-08-05'                as const06, @arg06 as param06,
CAST('1991-08-05' as date)  as const07, @arg07 as param07,
'1991-08-05 01:01:01'       as const08, @arg08 as param08,
CAST('1991-08-05 01:01:01'  as datetime) as const09, @arg09 as param09,
unix_timestamp('1991-01-01 01:01:01')    as const10, @arg10 as param10,
YEAR('1991-01-01 01:01:01') as const11, @arg11 as param11, 
NULL                        as const12, @arg12 as param12,
@arg13 as param13,
@arg14 as param14,
@arg15 as param15;
show create table t5 ;
Table	Create Table
t5	CREATE TABLE `t5` (
  `const01` bigint(1) NOT NULL default '0',
  `param01` bigint(20) default NULL,
  `const02` double(3,1) NOT NULL default '0.0',
  `param02` double default NULL,
  `const03` double NOT NULL default '0',
  `param03` double default NULL,
  `const04` char(3) NOT NULL default '',
  `param04` longtext,
  `const05` binary(3) NOT NULL default '',
  `param05` longblob,
  `const06` varchar(10) NOT NULL default '',
  `param06` longtext,
  `const07` date default NULL,
  `param07` longblob,
  `const08` varchar(19) NOT NULL default '',
  `param08` longtext,
  `const09` datetime default NULL,
  `param09` longblob,
  `const10` int(10) NOT NULL default '0',
  `param10` bigint(20) default NULL,
  `const11` int(4) default NULL,
  `param11` bigint(20) default NULL,
  `const12` binary(0) default NULL,
  `param12` bigint(20) default NULL,
  `param13` double default NULL,
  `param14` longtext,
  `param15` longblob
) ENGINE=MyISAM DEFAULT CHARSET=latin1
select * from t5 ;
Catalog	Database	Table	Table_alias	Column	Column_alias	Name	Type	Length	Max length	Is_null	Flags	Decimals	Charsetnr
def	test	t5	t5	const01	const01	8	1	1	N	32769	0	63
def	test	t5	t5	param01	param01	8	20	1	Y	32768	0	63
def	test	t5	t5	const02	const02	5	3	3	N	32769	1	63
def	test	t5	t5	param02	param02	5	20	1	Y	32768	31	63
def	test	t5	t5	const03	const03	5	23	1	N	32769	31	63
def	test	t5	t5	param03	param03	5	20	1	Y	32768	31	63
def	test	t5	t5	const04	const04	254	3	3	N	1	0	8
def	test	t5	t5	param04	param04	252	16777215	3	Y	16	0	8
def	test	t5	t5	const05	const05	254	3	3	N	129	0	63
def	test	t5	t5	param05	param05	252	16777215	3	Y	144	0	63
def	test	t5	t5	const06	const06	253	10	10	N	1	0	8
def	test	t5	t5	param06	param06	252	16777215	10	Y	16	0	8
def	test	t5	t5	const07	const07	10	10	10	Y	128	0	63
def	test	t5	t5	param07	param07	252	16777215	10	Y	144	0	63
def	test	t5	t5	const08	const08	253	19	19	N	1	0	8
def	test	t5	t5	param08	param08	252	16777215	19	Y	16	0	8
def	test	t5	t5	const09	const09	12	19	19	Y	128	0	63
def	test	t5	t5	param09	param09	252	16777215	19	Y	144	0	63
def	test	t5	t5	const10	const10	3	10	9	N	32769	0	63
def	test	t5	t5	param10	param10	8	20	9	Y	32768	0	63
def	test	t5	t5	const11	const11	3	4	4	Y	32768	0	63
def	test	t5	t5	param11	param11	8	20	4	Y	32768	0	63
def	test	t5	t5	const12	const12	254	0	0	Y	128	0	63
def	test	t5	t5	param12	param12	8	20	0	Y	32768	0	63
def	test	t5	t5	param13	param13	5	20	0	Y	32768	31	63
def	test	t5	t5	param14	param14	252	16777215	0	Y	16	0	8
def	test	t5	t5	param15	param15	252	16777215	0	Y	144	0	63
const01	8
param01	8
const02	8.0
param02	8
const03	8
param03	8
const04	abc
param04	abc
const05	abc
param05	abc
const06	1991-08-05
param06	1991-08-05
const07	1991-08-05
param07	1991-08-05
const08	1991-08-05 01:01:01
param08	1991-08-05 01:01:01
const09	1991-08-05 01:01:01
param09	1991-08-05 01:01:01
const10	662680861
param10	662680861
const11	1991
param11	1991
const12	NULL
param12	NULL
param13	NULL
param14	NULL
param15	NULL
drop table t5 ;
test_sequence
------ data type conversion tests ------
delete from t1 ;
insert into t1 values (1,'one');
insert into t1 values (2,'two');
insert into t1 values (3,'three');
insert into t1 values (4,'four');
commit ;
delete from t9 ;
insert into t9
set c1= 1, c2= 1, c3= 1, c4= 1, c5= 1, c6= 1, c7= 1, c8= 1, c9= 1,
c10= 1, c11= 1, c12 = 1,
c13= '2004-02-29', c14= '2004-02-29 11:11:11', c15= '2004-02-29 11:11:11',
c16= '11:11:11', c17= '2004',
c18= 1, c19=true, c20= 'a', c21= '123456789a', 
c22= '123456789a123456789b123456789c', c23= 'tinyblob', c24= 'tinytext',
c25= 'blob', c26= 'text', c27= 'mediumblob', c28= 'mediumtext',
c29= 'longblob', c30= 'longtext', c31='one', c32= 'monday';
insert into t9
set c1= 9, c2= 9, c3= 9, c4= 9, c5= 9, c6= 9, c7= 9, c8= 9, c9= 9,
c10= 9, c11= 9, c12 = 9,
c13= '2004-02-29', c14= '2004-02-29 11:11:11', c15= '2004-02-29 11:11:11',
c16= '11:11:11', c17= '2004',
c18= 1, c19=false, c20= 'a', c21= '123456789a', 
c22= '123456789a123456789b123456789c', c23= 'tinyblob', c24= 'tinytext',
c25= 'blob', c26= 'text', c27= 'mediumblob', c28= 'mediumtext',
c29= 'longblob', c30= 'longtext', c31='two', c32= 'tuesday';
commit ;
insert into t9 set c1= 0, c15= '1991-01-01 01:01:01' ;
select * from t9 order by c1 ;
c1	c2	c3	c4	c5	c6	c7	c8	c9	c10	c11	c12	c13	c14	c15	c16	c17	c18	c19	c20	c21	c22	c23	c24	c25	c26	c27	c28	c29	c30	c31	c32
0	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	1991-01-01 01:01:01	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
1	1	1	1	1	1	1	1	1	1	1.0000	1.0000	2004-02-29	2004-02-29 11:11:11	2004-02-29 11:11:11	11:11:11	2004	1	1	a	123456789a	123456789a123456789b123456789c	tinyblob	tinytext	blob	text	mediumblob	mediumtext	longblob	longtext	one	monday
9	9	9	9	9	9	9	9	9	9	9.0000	9.0000	2004-02-29	2004-02-29 11:11:11	2004-02-29 11:11:11	11:11:11	2004	1	0	a	123456789a	123456789a123456789b123456789c	tinyblob	tinytext	blob	text	mediumblob	mediumtext	longblob	longtext	two	tuesday
test_sequence
------ select @parameter:= column ------
prepare full_info from "select @arg01, @arg02, @arg03, @arg04,
       @arg05, @arg06, @arg07, @arg08,
       @arg09, @arg10, @arg11, @arg12,
       @arg13, @arg14, @arg15, @arg16,
       @arg17, @arg18, @arg19, @arg20,
       @arg21, @arg22, @arg23, @arg24,
       @arg25, @arg26, @arg27, @arg28,
       @arg29, @arg30, @arg31, @arg32" ;
select @arg01:=  c1, @arg02:=  c2, @arg03:=  c3, @arg04:=  c4,
@arg05:=  c5, @arg06:=  c6, @arg07:=  c7, @arg08:=  c8,
@arg09:=  c9, @arg10:= c10, @arg11:= c11, @arg12:= c12,
@arg13:= c13, @arg14:= c14, @arg15:= c15, @arg16:= c16,
@arg17:= c17, @arg18:= c18, @arg19:= c19, @arg20:= c20,
@arg21:= c21, @arg22:= c22, @arg23:= c23, @arg24:= c24,
@arg25:= c25, @arg26:= c26, @arg27:= c27, @arg28:= c28,
@arg29:= c29, @arg30:= c30, @arg31:= c31, @arg32:= c32
from t9 where c1= 1 ;
@arg01:=  c1	@arg02:=  c2	@arg03:=  c3	@arg04:=  c4	@arg05:=  c5	@arg06:=  c6	@arg07:=  c7	@arg08:=  c8	@arg09:=  c9	@arg10:= c10	@arg11:= c11	@arg12:= c12	@arg13:= c13	@arg14:= c14	@arg15:= c15	@arg16:= c16	@arg17:= c17	@arg18:= c18	@arg19:= c19	@arg20:= c20	@arg21:= c21	@arg22:= c22	@arg23:= c23	@arg24:= c24	@arg25:= c25	@arg26:= c26	@arg27:= c27	@arg28:= c28	@arg29:= c29	@arg30:= c30	@arg31:= c31	@arg32:= c32
1	1	1	1	1	1	1	1	1	1	1.0000	1.0000	2004-02-29	2004-02-29 11:11:11	2004-02-29 11:11:11	11:11:11	2004	1	1	a	123456789a	123456789a123456789b123456789c	tinyblob	tinytext	blob	text	mediumblob	mediumtext	longblob	longtext	one	monday
execute full_info ;
Catalog	Database	Table	Table_alias	Column	Column_alias	Name	Type	Length	Max length	Is_null	Flags	Decimals	Charsetnr
def					@arg01	254	20	1	Y	128	31	63
def					@arg02	254	20	1	Y	128	31	63
def					@arg03	254	20	1	Y	128	31	63
def					@arg04	254	20	1	Y	128	31	63
def					@arg05	254	20	1	Y	128	31	63
def					@arg06	254	20	1	Y	128	31	63
def					@arg07	254	20	1	Y	128	31	63
def					@arg08	254	20	1	Y	128	31	63
def					@arg09	254	20	1	Y	128	31	63
def					@arg10	254	20	1	Y	128	31	63
def					@arg11	254	20	1	Y	128	31	63
def					@arg12	254	20	1	Y	128	31	63
def					@arg13	254	8192	10	Y	128	31	63
def					@arg14	254	8192	19	Y	128	31	63
def					@arg15	254	8192	19	Y	128	31	63
def					@arg16	254	8192	8	Y	128	31	63
def					@arg17	254	20	4	Y	128	31	63
def					@arg18	254	20	1	Y	128	31	63
def					@arg19	254	20	1	Y	128	31	63
def					@arg20	254	8192	1	Y	0	31	8
def					@arg21	254	8192	10	Y	0	31	8
def					@arg22	254	8192	30	Y	0	31	8
def					@arg23	254	8192	8	Y	128	31	63
def					@arg24	254	8192	8	Y	0	31	8
def					@arg25	254	8192	4	Y	128	31	63
def					@arg26	254	8192	4	Y	0	31	8
def					@arg27	254	8192	10	Y	128	31	63
def					@arg28	254	8192	10	Y	0	31	8
def					@arg29	254	8192	8	Y	128	31	63
def					@arg30	254	8192	8	Y	0	31	8
def					@arg31	254	8192	3	Y	0	31	8
def					@arg32	254	8192	6	Y	128	31	63
@arg01	@arg02	@arg03	@arg04	@arg05	@arg06	@arg07	@arg08	@arg09	@arg10	@arg11	@arg12	@arg13	@arg14	@arg15	@arg16	@arg17	@arg18	@arg19	@arg20	@arg21	@arg22	@arg23	@arg24	@arg25	@arg26	@arg27	@arg28	@arg29	@arg30	@arg31	@arg32
1	1	1	1	1	1	1	1	1	1	1	1	2004-02-29	2004-02-29 11:11:11	2004-02-29 11:11:11	11:11:11	2004	1	1	a	123456789a	123456789a123456789b123456789c	tinyblob	tinytext	blob	text	mediumblob	mediumtext	longblob	longtext	one	monday
select @arg01:=  c1, @arg02:=  c2, @arg03:=  c3, @arg04:=  c4,
@arg05:=  c5, @arg06:=  c6, @arg07:=  c7, @arg08:=  c8,
@arg09:=  c9, @arg10:= c10, @arg11:= c11, @arg12:= c12,
@arg13:= c13, @arg14:= c14, @arg15:= c15, @arg16:= c16,
@arg17:= c17, @arg18:= c18, @arg19:= c19, @arg20:= c20,
@arg21:= c21, @arg22:= c22, @arg23:= c23, @arg24:= c24,
@arg25:= c25, @arg26:= c26, @arg27:= c27, @arg28:= c28,
@arg29:= c29, @arg30:= c30, @arg31:= c31, @arg32:= c32
from t9 where c1= 0 ;
@arg01:=  c1	@arg02:=  c2	@arg03:=  c3	@arg04:=  c4	@arg05:=  c5	@arg06:=  c6	@arg07:=  c7	@arg08:=  c8	@arg09:=  c9	@arg10:= c10	@arg11:= c11	@arg12:= c12	@arg13:= c13	@arg14:= c14	@arg15:= c15	@arg16:= c16	@arg17:= c17	@arg18:= c18	@arg19:= c19	@arg20:= c20	@arg21:= c21	@arg22:= c22	@arg23:= c23	@arg24:= c24	@arg25:= c25	@arg26:= c26	@arg27:= c27	@arg28:= c28	@arg29:= c29	@arg30:= c30	@arg31:= c31	@arg32:= c32
0	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	1991-01-01 01:01:01	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
execute full_info ;
Catalog	Database	Table	Table_alias	Column	Column_alias	Name	Type	Length	Max length	Is_null	Flags	Decimals	Charsetnr
def					@arg01	254	20	1	Y	128	31	63
def					@arg02	254	20	0	Y	128	31	63
def					@arg03	254	20	0	Y	128	31	63
def					@arg04	254	20	0	Y	128	31	63
def					@arg05	254	20	0	Y	128	31	63
def					@arg06	254	20	0	Y	128	31	63
def					@arg07	254	20	0	Y	128	31	63
def					@arg08	254	20	0	Y	128	31	63
def					@arg09	254	20	0	Y	128	31	63
def					@arg10	254	20	0	Y	128	31	63
def					@arg11	254	20	0	Y	128	31	63
def					@arg12	254	20	0	Y	128	31	63
def					@arg13	254	8192	0	Y	128	31	63
def					@arg14	254	8192	0	Y	128	31	63
def					@arg15	254	8192	19	Y	128	31	63
def					@arg16	254	8192	0	Y	128	31	63
def					@arg17	254	20	0	Y	128	31	63
def					@arg18	254	20	0	Y	128	31	63
def					@arg19	254	20	0	Y	128	31	63
def					@arg20	254	8192	0	Y	0	31	8
def					@arg21	254	8192	0	Y	0	31	8
def					@arg22	254	8192	0	Y	0	31	8
def					@arg23	254	8192	0	Y	128	31	63
def					@arg24	254	8192	0	Y	0	31	8
def					@arg25	254	8192	0	Y	128	31	63
def					@arg26	254	8192	0	Y	0	31	8
def					@arg27	254	8192	0	Y	128	31	63
def					@arg28	254	8192	0	Y	0	31	8
def					@arg29	254	8192	0	Y	128	31	63
def					@arg30	254	8192	0	Y	0	31	8
def					@arg31	254	8192	0	Y	0	31	8
def					@arg32	254	8192	0	Y	0	31	8
@arg01	@arg02	@arg03	@arg04	@arg05	@arg06	@arg07	@arg08	@arg09	@arg10	@arg11	@arg12	@arg13	@arg14	@arg15	@arg16	@arg17	@arg18	@arg19	@arg20	@arg21	@arg22	@arg23	@arg24	@arg25	@arg26	@arg27	@arg28	@arg29	@arg30	@arg31	@arg32
0	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	1991-01-01 01:01:01	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
prepare stmt1 from "select 
       @arg01:=  c1, @arg02:=  c2, @arg03:=  c3, @arg04:=  c4,
       @arg05:=  c5, @arg06:=  c6, @arg07:=  c7, @arg08:=  c8,
       @arg09:=  c9, @arg10:= c10, @arg11:= c11, @arg12:= c12,
       @arg13:= c13, @arg14:= c14, @arg15:= c15, @arg16:= c16,
       @arg17:= c17, @arg18:= c18, @arg19:= c19, @arg20:= c20,
       @arg21:= c21, @arg22:= c22, @arg23:= c23, @arg24:= c24,
       @arg25:= c25, @arg26:= c26, @arg27:= c27, @arg28:= c28,
       @arg29:= c29, @arg30:= c30, @arg31:= c31, @arg32:= c32
from t9 where c1= ?" ;
set @my_key= 1 ;
execute stmt1 using @my_key ;
@arg01:=  c1	@arg02:=  c2	@arg03:=  c3	@arg04:=  c4	@arg05:=  c5	@arg06:=  c6	@arg07:=  c7	@arg08:=  c8	@arg09:=  c9	@arg10:= c10	@arg11:= c11	@arg12:= c12	@arg13:= c13	@arg14:= c14	@arg15:= c15	@arg16:= c16	@arg17:= c17	@arg18:= c18	@arg19:= c19	@arg20:= c20	@arg21:= c21	@arg22:= c22	@arg23:= c23	@arg24:= c24	@arg25:= c25	@arg26:= c26	@arg27:= c27	@arg28:= c28	@arg29:= c29	@arg30:= c30	@arg31:= c31	@arg32:= c32
1	1	1	1	1	1	1	1	1	1	1.0000	1.0000	2004-02-29	2004-02-29 11:11:11	2004-02-29 11:11:11	11:11:11	2004	1	1	a	123456789a	123456789a123456789b123456789c	tinyblob	tinytext	blob	text	mediumblob	mediumtext	longblob	longtext	one	monday
execute full_info ;
Catalog	Database	Table	Table_alias	Column	Column_alias	Name	Type	Length	Max length	Is_null	Flags	Decimals	Charsetnr
def					@arg01	254	20	1	Y	128	31	63
def					@arg02	254	20	1	Y	128	31	63
def					@arg03	254	20	1	Y	128	31	63
def					@arg04	254	20	1	Y	128	31	63
def					@arg05	254	20	1	Y	128	31	63
def					@arg06	254	20	1	Y	128	31	63
def					@arg07	254	20	1	Y	128	31	63
def					@arg08	254	20	1	Y	128	31	63
def					@arg09	254	20	1	Y	128	31	63
def					@arg10	254	20	1	Y	128	31	63
def					@arg11	254	20	1	Y	128	31	63
def					@arg12	254	20	1	Y	128	31	63
def					@arg13	254	8192	10	Y	128	31	63
def					@arg14	254	8192	19	Y	128	31	63
def					@arg15	254	8192	19	Y	128	31	63
def					@arg16	254	8192	8	Y	128	31	63
def					@arg17	254	20	4	Y	128	31	63
def					@arg18	254	20	1	Y	128	31	63
def					@arg19	254	20	1	Y	128	31	63
def					@arg20	254	8192	1	Y	0	31	8
def					@arg21	254	8192	10	Y	0	31	8
def					@arg22	254	8192	30	Y	0	31	8
def					@arg23	254	8192	8	Y	128	31	63
def					@arg24	254	8192	8	Y	0	31	8
def					@arg25	254	8192	4	Y	128	31	63
def					@arg26	254	8192	4	Y	0	31	8
def					@arg27	254	8192	10	Y	128	31	63
def					@arg28	254	8192	10	Y	0	31	8
def					@arg29	254	8192	8	Y	128	31	63
def					@arg30	254	8192	8	Y	0	31	8
def					@arg31	254	8192	3	Y	0	31	8
def					@arg32	254	8192	6	Y	128	31	63
@arg01	@arg02	@arg03	@arg04	@arg05	@arg06	@arg07	@arg08	@arg09	@arg10	@arg11	@arg12	@arg13	@arg14	@arg15	@arg16	@arg17	@arg18	@arg19	@arg20	@arg21	@arg22	@arg23	@arg24	@arg25	@arg26	@arg27	@arg28	@arg29	@arg30	@arg31	@arg32
1	1	1	1	1	1	1	1	1	1	1	1	2004-02-29	2004-02-29 11:11:11	2004-02-29 11:11:11	11:11:11	2004	1	1	a	123456789a	123456789a123456789b123456789c	tinyblob	tinytext	blob	text	mediumblob	mediumtext	longblob	longtext	one	monday
set @my_key= 0 ;
execute stmt1 using @my_key ;
@arg01:=  c1	@arg02:=  c2	@arg03:=  c3	@arg04:=  c4	@arg05:=  c5	@arg06:=  c6	@arg07:=  c7	@arg08:=  c8	@arg09:=  c9	@arg10:= c10	@arg11:= c11	@arg12:= c12	@arg13:= c13	@arg14:= c14	@arg15:= c15	@arg16:= c16	@arg17:= c17	@arg18:= c18	@arg19:= c19	@arg20:= c20	@arg21:= c21	@arg22:= c22	@arg23:= c23	@arg24:= c24	@arg25:= c25	@arg26:= c26	@arg27:= c27	@arg28:= c28	@arg29:= c29	@arg30:= c30	@arg31:= c31	@arg32:= c32
0	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	1991-01-01 01:01:01	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
execute full_info ;
Catalog	Database	Table	Table_alias	Column	Column_alias	Name	Type	Length	Max length	Is_null	Flags	Decimals	Charsetnr
def					@arg01	254	20	1	Y	128	31	63
def					@arg02	254	20	0	Y	128	31	63
def					@arg03	254	20	0	Y	128	31	63
def					@arg04	254	20	0	Y	128	31	63
def					@arg05	254	20	0	Y	128	31	63
def					@arg06	254	20	0	Y	128	31	63
def					@arg07	254	20	0	Y	128	31	63
def					@arg08	254	20	0	Y	128	31	63
def					@arg09	254	20	0	Y	128	31	63
def					@arg10	254	20	0	Y	128	31	63
def					@arg11	254	20	0	Y	128	31	63
def					@arg12	254	20	0	Y	128	31	63
def					@arg13	254	8192	0	Y	128	31	63
def					@arg14	254	8192	0	Y	128	31	63
def					@arg15	254	8192	19	Y	128	31	63
def					@arg16	254	8192	0	Y	128	31	63
def					@arg17	254	20	0	Y	128	31	63
def					@arg18	254	20	0	Y	128	31	63
def					@arg19	254	20	0	Y	128	31	63
def					@arg20	254	8192	0	Y	0	31	8
def					@arg21	254	8192	0	Y	0	31	8
def					@arg22	254	8192	0	Y	0	31	8
def					@arg23	254	8192	0	Y	128	31	63
def					@arg24	254	8192	0	Y	0	31	8
def					@arg25	254	8192	0	Y	128	31	63
def					@arg26	254	8192	0	Y	0	31	8
def					@arg27	254	8192	0	Y	128	31	63
def					@arg28	254	8192	0	Y	0	31	8
def					@arg29	254	8192	0	Y	128	31	63
def					@arg30	254	8192	0	Y	0	31	8
def					@arg31	254	8192	0	Y	0	31	8
def					@arg32	254	8192	0	Y	0	31	8
@arg01	@arg02	@arg03	@arg04	@arg05	@arg06	@arg07	@arg08	@arg09	@arg10	@arg11	@arg12	@arg13	@arg14	@arg15	@arg16	@arg17	@arg18	@arg19	@arg20	@arg21	@arg22	@arg23	@arg24	@arg25	@arg26	@arg27	@arg28	@arg29	@arg30	@arg31	@arg32
0	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	1991-01-01 01:01:01	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
prepare stmt1 from "select ? := c1 from t9 where c1= 1" ;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near ':= c1 from t9 where c1= 1' at line 1
test_sequence
------ select column, .. into @parm,.. ------
select c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12,
c13, c14, c15, c16, c17, c18, c19, c20, c21, c22, c23, c24,
c25, c26, c27, c28, c29, c30, c31, c32
into @arg01, @arg02, @arg03, @arg04, @arg05, @arg06, @arg07, @arg08,
@arg09, @arg10, @arg11, @arg12, @arg13, @arg14, @arg15, @arg16,
@arg17, @arg18, @arg19, @arg20, @arg21, @arg22, @arg23, @arg24,
@arg25, @arg26, @arg27, @arg28, @arg29, @arg30, @arg31, @arg32
from t9 where c1= 1 ;
execute full_info ;
Catalog	Database	Table	Table_alias	Column	Column_alias	Name	Type	Length	Max length	Is_null	Flags	Decimals	Charsetnr
def					@arg01	254	20	1	Y	128	31	63
def					@arg02	254	20	1	Y	128	31	63
def					@arg03	254	20	1	Y	128	31	63
def					@arg04	254	20	1	Y	128	31	63
def					@arg05	254	20	1	Y	128	31	63
def					@arg06	254	20	1	Y	128	31	63
def					@arg07	254	20	1	Y	128	31	63
def					@arg08	254	20	1	Y	128	31	63
def					@arg09	254	20	1	Y	128	31	63
def					@arg10	254	20	1	Y	128	31	63
def					@arg11	254	20	1	Y	128	31	63
def					@arg12	254	20	1	Y	128	31	63
def					@arg13	254	8192	10	Y	128	31	63
def					@arg14	254	8192	19	Y	128	31	63
def					@arg15	254	8192	19	Y	128	31	63
def					@arg16	254	8192	8	Y	128	31	63
def					@arg17	254	20	4	Y	128	31	63
def					@arg18	254	20	1	Y	128	31	63
def					@arg19	254	20	1	Y	128	31	63
def					@arg20	254	8192	1	Y	0	31	8
def					@arg21	254	8192	10	Y	0	31	8
def					@arg22	254	8192	30	Y	0	31	8
def					@arg23	254	8192	8	Y	128	31	63
def					@arg24	254	8192	8	Y	0	31	8
def					@arg25	254	8192	4	Y	128	31	63
def					@arg26	254	8192	4	Y	0	31	8
def					@arg27	254	8192	10	Y	128	31	63
def					@arg28	254	8192	10	Y	0	31	8
def					@arg29	254	8192	8	Y	128	31	63
def					@arg30	254	8192	8	Y	0	31	8
def					@arg31	254	8192	3	Y	0	31	8
def					@arg32	254	8192	6	Y	128	31	63
@arg01	@arg02	@arg03	@arg04	@arg05	@arg06	@arg07	@arg08	@arg09	@arg10	@arg11	@arg12	@arg13	@arg14	@arg15	@arg16	@arg17	@arg18	@arg19	@arg20	@arg21	@arg22	@arg23	@arg24	@arg25	@arg26	@arg27	@arg28	@arg29	@arg30	@arg31	@arg32
1	1	1	1	1	1	1	1	1	1	1	1	2004-02-29	2004-02-29 11:11:11	2004-02-29 11:11:11	11:11:11	2004	1	1	a	123456789a	123456789a123456789b123456789c	tinyblob	tinytext	blob	text	mediumblob	mediumtext	longblob	longtext	one	monday
select c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12,
c13, c14, c15, c16, c17, c18, c19, c20, c21, c22, c23, c24,
c25, c26, c27, c28, c29, c30, c31, c32
into @arg01, @arg02, @arg03, @arg04, @arg05, @arg06, @arg07, @arg08,
@arg09, @arg10, @arg11, @arg12, @arg13, @arg14, @arg15, @arg16,
@arg17, @arg18, @arg19, @arg20, @arg21, @arg22, @arg23, @arg24,
@arg25, @arg26, @arg27, @arg28, @arg29, @arg30, @arg31, @arg32
from t9 where c1= 0 ;
execute full_info ;
Catalog	Database	Table	Table_alias	Column	Column_alias	Name	Type	Length	Max length	Is_null	Flags	Decimals	Charsetnr
def					@arg01	254	20	1	Y	128	31	63
def					@arg02	254	20	0	Y	128	31	63
def					@arg03	254	20	0	Y	128	31	63
def					@arg04	254	20	0	Y	128	31	63
def					@arg05	254	20	0	Y	128	31	63
def					@arg06	254	20	0	Y	128	31	63
def					@arg07	254	20	0	Y	128	31	63
def					@arg08	254	20	0	Y	128	31	63
def					@arg09	254	20	0	Y	128	31	63
def					@arg10	254	20	0	Y	128	31	63
def					@arg11	254	20	0	Y	128	31	63
def					@arg12	254	20	0	Y	128	31	63
def					@arg13	254	8192	0	Y	128	31	63
def					@arg14	254	8192	0	Y	128	31	63
def					@arg15	254	8192	19	Y	128	31	63
def					@arg16	254	8192	0	Y	128	31	63
def					@arg17	254	20	0	Y	128	31	63
def					@arg18	254	20	0	Y	128	31	63
def					@arg19	254	20	0	Y	128	31	63
def					@arg20	254	8192	0	Y	0	31	8
def					@arg21	254	8192	0	Y	0	31	8
def					@arg22	254	8192	0	Y	0	31	8
def					@arg23	254	8192	0	Y	128	31	63
def					@arg24	254	8192	0	Y	0	31	8
def					@arg25	254	8192	0	Y	128	31	63
def					@arg26	254	8192	0	Y	0	31	8
def					@arg27	254	8192	0	Y	128	31	63
def					@arg28	254	8192	0	Y	0	31	8
def					@arg29	254	8192	0	Y	128	31	63
def					@arg30	254	8192	0	Y	0	31	8
def					@arg31	254	8192	0	Y	0	31	8
def					@arg32	254	8192	0	Y	0	31	8
@arg01	@arg02	@arg03	@arg04	@arg05	@arg06	@arg07	@arg08	@arg09	@arg10	@arg11	@arg12	@arg13	@arg14	@arg15	@arg16	@arg17	@arg18	@arg19	@arg20	@arg21	@arg22	@arg23	@arg24	@arg25	@arg26	@arg27	@arg28	@arg29	@arg30	@arg31	@arg32
0	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	1991-01-01 01:01:01	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
prepare stmt1 from "select c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12,
       c13, c14, c15, c16, c17, c18, c19, c20, c21, c22, c23, c24,
       c25, c26, c27, c28, c29, c30, c31, c32
into @arg01, @arg02, @arg03, @arg04, @arg05, @arg06, @arg07, @arg08,
     @arg09, @arg10, @arg11, @arg12, @arg13, @arg14, @arg15, @arg16,
     @arg17, @arg18, @arg19, @arg20, @arg21, @arg22, @arg23, @arg24,
     @arg25, @arg26, @arg27, @arg28, @arg29, @arg30, @arg31, @arg32
from t9 where c1= ?" ;
set @my_key= 1 ;
execute stmt1 using @my_key ;
execute full_info ;
Catalog	Database	Table	Table_alias	Column	Column_alias	Name	Type	Length	Max length	Is_null	Flags	Decimals	Charsetnr
def					@arg01	254	20	1	Y	128	31	63
def					@arg02	254	20	1	Y	128	31	63
def					@arg03	254	20	1	Y	128	31	63
def					@arg04	254	20	1	Y	128	31	63
def					@arg05	254	20	1	Y	128	31	63
def					@arg06	254	20	1	Y	128	31	63
def					@arg07	254	20	1	Y	128	31	63
def					@arg08	254	20	1	Y	128	31	63
def					@arg09	254	20	1	Y	128	31	63
def					@arg10	254	20	1	Y	128	31	63
def					@arg11	254	20	1	Y	128	31	63
def					@arg12	254	20	1	Y	128	31	63
def					@arg13	254	8192	10	Y	128	31	63
def					@arg14	254	8192	19	Y	128	31	63
def					@arg15	254	8192	19	Y	128	31	63
def					@arg16	254	8192	8	Y	128	31	63
def					@arg17	254	20	4	Y	128	31	63
def					@arg18	254	20	1	Y	128	31	63
def					@arg19	254	20	1	Y	128	31	63
def					@arg20	254	8192	1	Y	0	31	8
def					@arg21	254	8192	10	Y	0	31	8
def					@arg22	254	8192	30	Y	0	31	8
def					@arg23	254	8192	8	Y	128	31	63
def					@arg24	254	8192	8	Y	0	31	8
def					@arg25	254	8192	4	Y	128	31	63
def					@arg26	254	8192	4	Y	0	31	8
def					@arg27	254	8192	10	Y	128	31	63
def					@arg28	254	8192	10	Y	0	31	8
def					@arg29	254	8192	8	Y	128	31	63
def					@arg30	254	8192	8	Y	0	31	8
def					@arg31	254	8192	3	Y	0	31	8
def					@arg32	254	8192	6	Y	128	31	63
@arg01	@arg02	@arg03	@arg04	@arg05	@arg06	@arg07	@arg08	@arg09	@arg10	@arg11	@arg12	@arg13	@arg14	@arg15	@arg16	@arg17	@arg18	@arg19	@arg20	@arg21	@arg22	@arg23	@arg24	@arg25	@arg26	@arg27	@arg28	@arg29	@arg30	@arg31	@arg32
1	1	1	1	1	1	1	1	1	1	1	1	2004-02-29	2004-02-29 11:11:11	2004-02-29 11:11:11	11:11:11	2004	1	1	a	123456789a	123456789a123456789b123456789c	tinyblob	tinytext	blob	text	mediumblob	mediumtext	longblob	longtext	one	monday
set @my_key= 0 ;
execute stmt1 using @my_key ;
execute full_info ;
Catalog	Database	Table	Table_alias	Column	Column_alias	Name	Type	Length	Max length	Is_null	Flags	Decimals	Charsetnr
def					@arg01	254	20	1	Y	128	31	63
def					@arg02	254	20	0	Y	128	31	63
def					@arg03	254	20	0	Y	128	31	63
def					@arg04	254	20	0	Y	128	31	63
def					@arg05	254	20	0	Y	128	31	63
def					@arg06	254	20	0	Y	128	31	63
def					@arg07	254	20	0	Y	128	31	63
def					@arg08	254	20	0	Y	128	31	63
def					@arg09	254	20	0	Y	128	31	63
def					@arg10	254	20	0	Y	128	31	63
def					@arg11	254	20	0	Y	128	31	63
def					@arg12	254	20	0	Y	128	31	63
def					@arg13	254	8192	0	Y	128	31	63
def					@arg14	254	8192	0	Y	128	31	63
def					@arg15	254	8192	19	Y	128	31	63
def					@arg16	254	8192	0	Y	128	31	63
def					@arg17	254	20	0	Y	128	31	63
def					@arg18	254	20	0	Y	128	31	63
def					@arg19	254	20	0	Y	128	31	63
def					@arg20	254	8192	0	Y	0	31	8
def					@arg21	254	8192	0	Y	0	31	8
def					@arg22	254	8192	0	Y	0	31	8
def					@arg23	254	8192	0	Y	128	31	63
def					@arg24	254	8192	0	Y	0	31	8
def					@arg25	254	8192	0	Y	128	31	63
def					@arg26	254	8192	0	Y	0	31	8
def					@arg27	254	8192	0	Y	128	31	63
def					@arg28	254	8192	0	Y	0	31	8
def					@arg29	254	8192	0	Y	128	31	63
def					@arg30	254	8192	0	Y	0	31	8
def					@arg31	254	8192	0	Y	0	31	8
def					@arg32	254	8192	0	Y	0	31	8
@arg01	@arg02	@arg03	@arg04	@arg05	@arg06	@arg07	@arg08	@arg09	@arg10	@arg11	@arg12	@arg13	@arg14	@arg15	@arg16	@arg17	@arg18	@arg19	@arg20	@arg21	@arg22	@arg23	@arg24	@arg25	@arg26	@arg27	@arg28	@arg29	@arg30	@arg31	@arg32
0	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	1991-01-01 01:01:01	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
prepare stmt1 from "select c1 into ? from t9 where c1= 1" ;
ERROR 42000: You have an error in your SQL syntax; check the manual that corresponds to your MySQL server version for the right syntax to use near '? from t9 where c1= 1' at line 1
test_sequence
-- insert into numeric columns --
insert into t9 
( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
( 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20 ) ;
set @arg00= 21 ;
insert into t9 
( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
( @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00, @arg00 ) ;
prepare stmt1 from "insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22 )" ;
execute stmt1 ;
set @arg00= 23;
prepare stmt2 from "insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values 
  (  ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00 ;
insert into t9 
( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
( 30.0, 30.0, 30.0, 30.0, 30.0, 30.0, 30.0, 30.0,
30.0, 30.0, 30.0 ) ;
set @arg00= 31.0 ;
insert into t9 
( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
( @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00, @arg00 ) ;
prepare stmt1 from "insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( 32.0, 32.0, 32.0, 32.0, 32.0, 32.0, 32.0, 32.0,
    32.0, 32.0, 32.0 )" ;
execute stmt1 ;
set @arg00= 33.0;
prepare stmt2 from "insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values 
  (  ?,  ?,  ?,  ?,  ?,  ?,  ?,  ?,  ?,   ?,   ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00 ;
insert into t9 
( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
( '40', '40', '40', '40', '40', '40', '40', '40',
'40', '40', '40' ) ;
set @arg00= '41' ;
insert into t9 
( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
( @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00, @arg00 ) ;
prepare stmt1 from "insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( '42', '42', '42', '42', '42', '42', '42', '42',
    '42', '42', '42' )" ;
execute stmt1 ;
set @arg00= '43';
prepare stmt2 from "insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values 
  ( ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00 ;
insert into t9 
( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
( CAST('50' as binary), CAST('50' as binary), 
CAST('50' as binary), CAST('50' as binary), CAST('50' as binary), 
CAST('50' as binary), CAST('50' as binary), CAST('50' as binary),
CAST('50' as binary), CAST('50' as binary), CAST('50' as binary) ) ;
set @arg00= CAST('51' as binary) ;
insert into t9 
( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
( @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00, @arg00 ) ;
prepare stmt1 from "insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( CAST('52' as binary), CAST('52' as binary),
  CAST('52' as binary), CAST('52' as binary), CAST('52' as binary), 
  CAST('52' as binary), CAST('52' as binary), CAST('52' as binary),
  CAST('52' as binary), CAST('52' as binary), CAST('52' as binary) )" ;
execute stmt1 ;
set @arg00= CAST('53' as binary) ;
prepare stmt2 from "insert into t9 
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values 
  ( ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00 ;
set @arg00= 2 ;
set @arg00= NULL ;
insert into t9
( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
( 60, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
NULL, NULL, NULL ) ;
insert into t9
( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
( 61, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00, @arg00 ) ;
prepare stmt1 from "insert into t9
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( 62, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL )" ;
execute stmt1 ;
prepare stmt2 from "insert into t9
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( 63, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00 ;
set @arg00= 8.0 ;
set @arg00= NULL ;
insert into t9
( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
( 71, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00, @arg00 ) ;
prepare stmt2 from "insert into t9
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( 73, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00 ;
set @arg00= 'abc' ;
set @arg00= NULL ;
insert into t9
( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
( 81, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00, @arg00 ) ;
prepare stmt2 from "insert into t9
  ( c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values
  ( 83, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? )" ;
execute stmt2 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00 ;
select c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12
from t9 where c1 >= 20
order by c1 ;
c1	c2	c3	c4	c5	c6	c7	c8	c9	c10	c12
20	20	20	20	20	20	20	20	20	20	20.0000
21	21	21	21	21	21	21	21	21	21	21.0000
22	22	22	22	22	22	22	22	22	22	22.0000
23	23	23	23	23	23	23	23	23	23	23.0000
30	30	30	30	30	30	30	30	30	30	30.0000
31	31	31	31	31	31	31	31	31	31	31.0000
32	32	32	32	32	32	32	32	32	32	32.0000
33	33	33	33	33	33	33	33	33	33	33.0000
40	40	40	40	40	40	40	40	40	40	40.0000
41	41	41	41	41	41	41	41	41	41	41.0000
42	42	42	42	42	42	42	42	42	42	42.0000
43	43	43	43	43	43	43	43	43	43	43.0000
50	50	50	50	50	50	50	50	50	50	50.0000
51	51	51	51	51	51	51	51	51	51	51.0000
52	52	52	52	52	52	52	52	52	52	52.0000
53	53	53	53	53	53	53	53	53	53	53.0000
60	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
61	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
62	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
63	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
71	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
73	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
81	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
83	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
test_sequence
-- select .. where numeric column = .. --
set @arg00= 20;
select 'true' as found from t9 
where c1= 20 and c2= 20 and c3= 20 and c4= 20 and c5= 20 and c6= 20 and c7= 20
and c8= 20 and c9= 20 and c10= 20 and c12= 20;
found
true
select 'true' as found from t9 
where c1= @arg00 and c2= @arg00 and c3= @arg00 and c4= @arg00 and c5= @arg00 
and c6= @arg00 and c7= @arg00 and c8= @arg00 and c9= @arg00 and c10= @arg00
and c12= @arg00;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and c2= 20 and c3= 20 and c4= 20 and c5= 20 and c6= 20 and c7= 20
  and c8= 20 and c9= 20 and c10= 20 and c12= 20 ";
execute stmt1 ;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= ? and c2= ? and c3= ? and c4= ? and c5= ? 
  and c6= ? and c7= ? and c8= ? and c9= ? and c10= ?
  and c12= ? ";
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00 ;
found
true
set @arg00= 20.0;
select 'true' as found from t9 
where c1= 20.0 and c2= 20.0 and c3= 20.0 and c4= 20.0 and c5= 20.0 and c6= 20.0
and c7= 20.0 and c8= 20.0 and c9= 20.0 and c10= 20.0 and c12= 20.0;
found
true
select 'true' as found from t9 
where c1= @arg00 and c2= @arg00 and c3= @arg00 and c4= @arg00 and c5= @arg00 
and c6= @arg00 and c7= @arg00 and c8= @arg00 and c9= @arg00 and c10= @arg00
and c12= @arg00;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20.0 and c2= 20.0 and c3= 20.0 and c4= 20.0 and c5= 20.0 and c6= 20.0
  and c7= 20.0 and c8= 20.0 and c9= 20.0 and c10= 20.0 and c12= 20.0 ";
execute stmt1 ;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= ? and c2= ? and c3= ? and c4= ? and c5= ? 
  and c6= ? and c7= ? and c8= ? and c9= ? and c10= ?
  and c12= ? ";
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00 ;
found
true
select 'true' as found from t9 
where c1= '20' and c2= '20' and c3= '20' and c4= '20' and c5= '20' and c6= '20'
  and c7= '20' and c8= '20' and c9= '20' and c10= '20' and c12= '20';
found
true
prepare stmt1 from "select 'true' as found from t9
where c1= '20' and c2= '20' and c3= '20' and c4= '20' and c5= '20' and c6= '20'
  and c7= '20' and c8= '20' and c9= '20' and c10= '20' and c12= '20' ";
execute stmt1 ;
found
true
set @arg00= '20';
select 'true' as found from t9 
where c1= @arg00 and c2= @arg00 and c3= @arg00 and c4= @arg00 and c5= @arg00 
and c6= @arg00 and c7= @arg00 and c8= @arg00 and c9= @arg00 and c10= @arg00
and c12= @arg00;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= ? and c2= ? and c3= ? and c4= ? and c5= ? 
  and c6= ? and c7= ? and c8= ? and c9= ? and c10= ?
  and c12= ? ";
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00 ;
found
true
select 'true' as found from t9 
where c1= CAST('20' as binary) and c2= CAST('20' as binary) and 
c3= CAST('20' as binary) and c4= CAST('20' as binary) and 
c5= CAST('20' as binary) and c6= CAST('20' as binary) and 
c7= CAST('20' as binary) and c8= CAST('20' as binary) and 
c9= CAST('20' as binary) and c10= CAST('20' as binary) and 
c12= CAST('20' as binary);
found
true
prepare stmt1 from "select 'true' as found from t9
where c1= CAST('20' as binary) and c2= CAST('20' as binary) and 
      c3= CAST('20' as binary) and c4= CAST('20' as binary) and 
      c5= CAST('20' as binary) and c6= CAST('20' as binary) and 
      c7= CAST('20' as binary) and c8= CAST('20' as binary) and 
      c9= CAST('20' as binary) and c10= CAST('20' as binary) and 
      c12= CAST('20' as binary) ";
execute stmt1 ;
found
true
set @arg00= CAST('20' as binary) ;
select 'true' as found from t9 
where c1= @arg00 and c2= @arg00 and c3= @arg00 and c4= @arg00 and c5= @arg00 
and c6= @arg00 and c7= @arg00 and c8= @arg00 and c9= @arg00 and c10= @arg00
and c12= @arg00;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= ? and c2= ? and c3= ? and c4= ? and c5= ? 
  and c6= ? and c7= ? and c8= ? and c9= ? and c10= ?
  and c12= ? ";
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00 ;
found
true
delete from t9 ;
test_sequence
-- some numeric overflow experiments --
prepare my_insert from "insert into t9 
   ( c21, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12 )
values 
   ( 'O',  ?,  ?,  ?,  ?,  ?,  ?,  ?,  ?,  ?,   ?,   ? )" ;
prepare my_select from "select c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c12
from t9 where c21 = 'O' ";
prepare my_delete from "delete from t9 where c21 = 'O' ";
set @arg00= 9223372036854775807 ;
execute my_insert using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00, @arg00 ;
Warnings:
Warning	1264	Data truncated; out of range for column 'c1' at row 1
Warning	1264	Data truncated; out of range for column 'c2' at row 1
Warning	1264	Data truncated; out of range for column 'c3' at row 1
Warning	1264	Data truncated; out of range for column 'c4' at row 1
Warning	1264	Data truncated; out of range for column 'c5' at row 1
Warning	1264	Data truncated; out of range for column 'c12' at row 1
execute my_select ;
c1	127
c2	32767
c3	8388607
c4	2147483647
c5	2147483647
c6	9223372036854775807
c7	9.22337e+18
c8	9.22337203685478e+18
c9	9.22337203685478e+18
c10	9.22337203685478e+18
c12	99999.9999
execute my_delete ;
set @arg00= '9223372036854775807' ;
execute my_insert using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00, @arg00 ;
Warnings:
Warning	1264	Data truncated; out of range for column 'c1' at row 1
Warning	1264	Data truncated; out of range for column 'c2' at row 1
Warning	1264	Data truncated; out of range for column 'c3' at row 1
Warning	1265	Data truncated for column 'c4' at row 1
Warning	1265	Data truncated for column 'c5' at row 1
Warning	1264	Data truncated; out of range for column 'c12' at row 1
execute my_select ;
c1	127
c2	32767
c3	8388607
c4	2147483647
c5	2147483647
c6	9223372036854775807
c7	9.22337e+18
c8	9.22337203685478e+18
c9	9.22337203685478e+18
c10	9.22337203685478e+18
c12	99999.9999
execute my_delete ;
set @arg00= -9223372036854775808 ;
execute my_insert using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00, @arg00 ;
Warnings:
Warning	1264	Data truncated; out of range for column 'c1' at row 1
Warning	1264	Data truncated; out of range for column 'c2' at row 1
Warning	1264	Data truncated; out of range for column 'c3' at row 1
Warning	1264	Data truncated; out of range for column 'c4' at row 1
Warning	1264	Data truncated; out of range for column 'c5' at row 1
Warning	1264	Data truncated; out of range for column 'c12' at row 1
execute my_select ;
c1	-128
c2	-32768
c3	-8388608
c4	-2147483648
c5	-2147483648
c6	-9223372036854775808
c7	-9.22337e+18
c8	-9.22337203685478e+18
c9	-9.22337203685478e+18
c10	-9.22337203685478e+18
c12	-9999.9999
execute my_delete ;
set @arg00= '-9223372036854775808' ;
execute my_insert using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00, @arg00 ;
Warnings:
Warning	1264	Data truncated; out of range for column 'c1' at row 1
Warning	1264	Data truncated; out of range for column 'c2' at row 1
Warning	1264	Data truncated; out of range for column 'c3' at row 1
Warning	1265	Data truncated for column 'c4' at row 1
Warning	1265	Data truncated for column 'c5' at row 1
Warning	1264	Data truncated; out of range for column 'c12' at row 1
execute my_select ;
c1	-128
c2	-32768
c3	-8388608
c4	-2147483648
c5	-2147483648
c6	-9223372036854775808
c7	-9.22337e+18
c8	-9.22337203685478e+18
c9	-9.22337203685478e+18
c10	-9.22337203685478e+18
c12	-9999.9999
execute my_delete ;
set @arg00= 1.11111111111111111111e+50 ;
execute my_insert using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00, @arg00 ;
Warnings:
Warning	1264	Data truncated; out of range for column 'c1' at row 1
Warning	1264	Data truncated; out of range for column 'c2' at row 1
Warning	1264	Data truncated; out of range for column 'c3' at row 1
Warning	1264	Data truncated; out of range for column 'c4' at row 1
Warning	1264	Data truncated; out of range for column 'c5' at row 1
Warning	1264	Data truncated; out of range for column 'c6' at row 1
Warning	1264	Data truncated; out of range for column 'c7' at row 1
Warning	1264	Data truncated; out of range for column 'c12' at row 1
execute my_select ;
c1	127
c2	32767
c3	8388607
c4	2147483647
c5	2147483647
c6	9223372036854775807
c7	3.40282e+38
c8	1.11111111111111e+50
c9	1.11111111111111e+50
c10	1.11111111111111e+50
c12	99999.9999
execute my_delete ;
set @arg00= '1.11111111111111111111e+50' ;
execute my_insert using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00, @arg00 ;
Warnings:
Warning	1265	Data truncated for column 'c1' at row 1
Warning	1265	Data truncated for column 'c2' at row 1
Warning	1265	Data truncated for column 'c3' at row 1
Warning	1265	Data truncated for column 'c4' at row 1
Warning	1265	Data truncated for column 'c5' at row 1
Warning	1265	Data truncated for column 'c6' at row 1
Warning	1264	Data truncated; out of range for column 'c7' at row 1
Warning	1264	Data truncated; out of range for column 'c12' at row 1
execute my_select ;
c1	1
c2	1
c3	1
c4	1
c5	1
c6	1
c7	3.40282e+38
c8	1.11111111111111e+50
c9	1.11111111111111e+50
c10	1.11111111111111e+50
c12	99999.9999
execute my_delete ;
set @arg00= -1.11111111111111111111e+50 ;
execute my_insert using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00, @arg00 ;
Warnings:
Warning	1264	Data truncated; out of range for column 'c1' at row 1
Warning	1264	Data truncated; out of range for column 'c2' at row 1
Warning	1264	Data truncated; out of range for column 'c3' at row 1
Warning	1264	Data truncated; out of range for column 'c4' at row 1
Warning	1264	Data truncated; out of range for column 'c5' at row 1
Warning	1264	Data truncated; out of range for column 'c6' at row 1
Warning	1264	Data truncated; out of range for column 'c7' at row 1
Warning	1264	Data truncated; out of range for column 'c12' at row 1
execute my_select ;
c1	-128
c2	-32768
c3	-8388608
c4	-2147483648
c5	-2147483648
c6	-9223372036854775808
c7	-3.40282e+38
c8	-1.11111111111111e+50
c9	-1.11111111111111e+50
c10	-1.11111111111111e+50
c12	-9999.9999
execute my_delete ;
set @arg00= '-1.11111111111111111111e+50' ;
execute my_insert using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00,
@arg00, @arg00, @arg00, @arg00, @arg00 ;
Warnings:
Warning	1265	Data truncated for column 'c1' at row 1
Warning	1265	Data truncated for column 'c2' at row 1
Warning	1265	Data truncated for column 'c3' at row 1
Warning	1265	Data truncated for column 'c4' at row 1
Warning	1265	Data truncated for column 'c5' at row 1
Warning	1265	Data truncated for column 'c6' at row 1
Warning	1264	Data truncated; out of range for column 'c7' at row 1
Warning	1264	Data truncated; out of range for column 'c12' at row 1
execute my_select ;
c1	-1
c2	-1
c3	-1
c4	-1
c5	-1
c6	-1
c7	-3.40282e+38
c8	-1.11111111111111e+50
c9	-1.11111111111111e+50
c10	-1.11111111111111e+50
c12	-9999.9999
execute my_delete ;
test_sequence
-- insert into string columns --
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
Warnings:
Warning	1265	Data truncated for column 'c20' at row 1
select c1, c20, c21, c22, c23, c24, c25, c26, c27, c28, c29, c30
from t9 where c1 >= 20
order by c1 ;
c1	c20	c21	c22	c23	c24	c25	c26	c27	c28	c29	c30
20	2	20	20	20	20	20	20	20	20	20	20
21	2	21	21	21	21	21	21	21	21	21	21
22	2	22	22	22	22	22	22	22	22	22	22
23	2	23	23	23	23	23	23	23	23	23	23
30	3	30	30	30	30	30	30	30	30	30	30
31	3	31	31	31	31	31	31	31	31	31	31
32	3	32	32	32	32	32	32	32	32	32	32
33	3	33	33	33	33	33	33	33	33	33	33
40	4	40	40	40	40	40	40	40	40	40	40
41	4	41	41	41	41	41	41	41	41	41	41
42	4	42	42	42	42	42	42	42	42	42	42
43	4	43	43	43	43	43	43	43	43	43	43
50	5	50	50	50.00	50.00	50.00	50.00	50.00	50.00	50.00	50.00
51	5	51	51	51	51	51	51	51	51	51	51
52	5	52	52	52.00	52.00	52.00	52.00	52.00	52.00	52.00	52.00
53	5	53	53	53.00	53.00	53.00	53.00	53.00	53.00	53.00	53.00
54	5	54	54	54.00	54.00	54.00	54.00	54.00	54.00	54.00	54.00
55	5	55	55	55	55	55	55	55	55	55	55
56	6	56	56	56.00	56.00	56.00	56.00	56.00	56.00	56.00	56.00
57	6	57	57	57.00	57.00	57.00	57.00	57.00	57.00	57.00	57.00
60	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
61	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
62	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
63	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
71	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
73	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
81	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
83	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL	NULL
test_sequence
-- select .. where string column = .. --
set @arg00= '20';
select 'true' as found from t9 
where c1= 20 and concat(c20,substr('20',1+length(c20)))= '20' and c21= '20' and
c22= '20' and c23= '20' and c24= '20' and c25= '20' and c26= '20' and
c27= '20' and c28= '20' and c29= '20' and c30= '20' ;
found
true
select 'true' as found from t9 
where c1= 20 and concat(c20,substr(@arg00,1+length(c20)))= @arg00 and
c21= @arg00 and c22= @arg00 and c23= @arg00 and c25= @arg00 and
c26= @arg00 and c27= @arg00 and c28= @arg00 and c29= @arg00 and c30= @arg00;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and concat(c20,substr('20',1+length(c20)))= '20' and c21= '20' and
  c22= '20' and c23= '20' and c24= '20' and c25= '20' and c26= '20' and
  c27= '20' and c28= '20' and c29= '20' and c30= '20'" ;
execute stmt1 ;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and concat(c20,substr(?,1+length(c20)))= ? and
  c21= ? and c22= ? and c23= ? and c25= ? and
  c26= ? and c27= ? and c28= ? and c29= ? and c30= ?" ;
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00, @arg00 ;
found
true
set @arg00= CAST('20' as binary);
select 'true' as found from t9 
where c1= 20 and concat(c20,substr(CAST('20' as binary),1+length(c20)))
= CAST('20' as binary) and c21= CAST('20' as binary)
and c22= CAST('20' as binary) and c23= CAST('20' as binary) and
c24= CAST('20' as binary) and c25= CAST('20' as binary) and
c26= CAST('20' as binary) and c27= CAST('20' as binary) and
c28= CAST('20' as binary) and c29= CAST('20' as binary) and
c30= CAST('20' as binary) ;
found
true
select 'true' as found from t9 
where c1= 20 and concat(c20,substr(@arg00,1+length(c20))) = @arg00 and
c21= @arg00 and c22= @arg00 and c23= @arg00 and c25= @arg00 and
c26= @arg00 and c27= @arg00 and c28= @arg00 and c29= @arg00 and
c30= @arg00;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and concat(c20,substr(CAST('20' as binary),1+length(c20)))
                 = CAST('20' as binary) and c21= CAST('20' as binary)
  and c22= CAST('20' as binary) and c23= CAST('20' as binary) and
  c24= CAST('20' as binary) and c25= CAST('20' as binary) and
  c26= CAST('20' as binary) and c27= CAST('20' as binary) and
  c28= CAST('20' as binary) and c29= CAST('20' as binary) and
  c30= CAST('20' as binary)" ;
execute stmt1 ;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and concat(c20,substr(?,1+length(c20))) = ? and c21= ? and
  c22= ? and c23= ? and c25= ? and c26= ? and c27= ? and c28= ? and
  c29= ? and c30= ?";
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00, @arg00 ;
found
true
set @arg00= 20;
select 'true' as found from t9 
where c1= 20 and concat(c20,substr(20,1+length(c20)))= 20 and c21= 20 and
c22= 20 and c23= 20 and c24= 20 and c25= 20 and c26= 20 and
c27= 20 and c28= 20 and c29= 20 and c30= 20 ;
found
true
select 'true' as found from t9 
where c1= 20 and concat(c20,substr(@arg00,1+length(c20)))= @arg00 and
c21= @arg00 and c22= @arg00 and c23= @arg00 and c25= @arg00 and
c26= @arg00 and c27= @arg00 and c28= @arg00 and c29= @arg00 and c30= @arg00;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and concat(c20,substr(20,1+length(c20)))= 20 and c21= 20 and
  c22= 20 and c23= 20 and c24= 20 and c25= 20 and c26= 20 and
  c27= 20 and c28= 20 and c29= 20 and c30= 20" ;
execute stmt1 ;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and concat(c20,substr(?,1+length(c20)))= ? and
  c21= ? and c22= ? and c23= ? and c25= ? and
  c26= ? and c27= ? and c28= ? and c29= ? and c30= ?" ;
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00, @arg00 ;
found
true
set @arg00= 20.0;
select 'true' as found from t9 
where c1= 20 and concat(c20,substr(20.0,1+length(c20)))= 20.0 and c21= 20.0 and
c22= 20.0 and c23= 20.0 and c24= 20.0 and c25= 20.0 and c26= 20.0 and
c27= 20.0 and c28= 20.0 and c29= 20.0 and c30= 20.0 ;
found
true
select 'true' as found from t9 
where c1= 20 and concat(c20,substr(@arg00,1+length(c20)))= @arg00 and
c21= @arg00 and c22= @arg00 and c23= @arg00 and c25= @arg00 and
c26= @arg00 and c27= @arg00 and c28= @arg00 and c29= @arg00 and c30= @arg00;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and concat(c20,substr(20.0,1+length(c20)))= 20.0 and c21= 20.0 and
  c22= 20.0 and c23= 20.0 and c24= 20.0 and c25= 20.0 and c26= 20.0 and
  c27= 20.0 and c28= 20.0 and c29= 20.0 and c30= 20.0" ;
execute stmt1 ;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and concat(c20,substr(?,1+length(c20)))= ? and
  c21= ? and c22= ? and c23= ? and c25= ? and
  c26= ? and c27= ? and c28= ? and c29= ? and c30= ?" ;
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00, @arg00, 
@arg00, @arg00, @arg00, @arg00, @arg00 ;
found
true
delete from t9 ;
test_sequence
-- insert into date/time columns --
Warnings:
Warning	1265	Data truncated for column 'c17' at row 1
Warnings:
Warning	1265	Data truncated for column 'c17' at row 1
Warnings:
Warning	1265	Data truncated for column 'c17' at row 1
Warnings:
Warning	1265	Data truncated for column 'c17' at row 1
Warnings:
Warning	1265	Data truncated for column 'c17' at row 1
Warnings:
Warning	1265	Data truncated for column 'c17' at row 1
Warnings:
Warning	1265	Data truncated for column 'c17' at row 1
Warnings:
Warning	1265	Data truncated for column 'c17' at row 1
Warnings:
Warning	1264	Data truncated; out of range for column 'c13' at row 1
Warning	1265	Data truncated for column 'c14' at row 1
Warning	1265	Data truncated for column 'c15' at row 1
Warning	1264	Data truncated; out of range for column 'c16' at row 1
Warning	1264	Data truncated; out of range for column 'c17' at row 1
Warnings:
Warning	1264	Data truncated; out of range for column 'c13' at row 1
Warning	1265	Data truncated for column 'c14' at row 1
Warning	1265	Data truncated for column 'c15' at row 1
Warning	1264	Data truncated; out of range for column 'c16' at row 1
Warning	1264	Data truncated; out of range for column 'c17' at row 1
Warnings:
Warning	1264	Data truncated; out of range for column 'c13' at row 1
Warning	1265	Data truncated for column 'c14' at row 1
Warning	1265	Data truncated for column 'c15' at row 1
Warning	1264	Data truncated; out of range for column 'c16' at row 1
Warning	1264	Data truncated; out of range for column 'c17' at row 1
Warnings:
Warning	1264	Data truncated; out of range for column 'c13' at row 1
Warning	1265	Data truncated for column 'c14' at row 1
Warning	1265	Data truncated for column 'c15' at row 1
Warning	1264	Data truncated; out of range for column 'c16' at row 1
Warning	1264	Data truncated; out of range for column 'c17' at row 1
Warnings:
Warning	1265	Data truncated for column 'c15' at row 1
Warning	1264	Data truncated; out of range for column 'c16' at row 1
Warning	1264	Data truncated; out of range for column 'c17' at row 1
Warnings:
Warning	1265	Data truncated for column 'c15' at row 1
Warning	1264	Data truncated; out of range for column 'c16' at row 1
Warning	1264	Data truncated; out of range for column 'c17' at row 1
Warnings:
Warning	1265	Data truncated for column 'c15' at row 1
Warning	1264	Data truncated; out of range for column 'c16' at row 1
Warning	1264	Data truncated; out of range for column 'c17' at row 1
Warnings:
Warning	1265	Data truncated for column 'c15' at row 1
Warning	1264	Data truncated; out of range for column 'c16' at row 1
Warning	1264	Data truncated; out of range for column 'c17' at row 1
select c1, c13, c14, c15, c16, c17 from t9 order by c1 ;
c1	c13	c14	c15	c16	c17
20	1991-01-01	1991-01-01 01:01:01	1991-01-01 01:01:01	01:01:01	1991
21	1991-01-01	1991-01-01 01:01:01	1991-01-01 01:01:01	01:01:01	1991
22	1991-01-01	1991-01-01 01:01:01	1991-01-01 01:01:01	01:01:01	1991
23	1991-01-01	1991-01-01 01:01:01	1991-01-01 01:01:01	01:01:01	1991
30	1991-01-01	1991-01-01 01:01:01	1991-01-01 01:01:01	01:01:01	1991
31	1991-01-01	1991-01-01 01:01:01	1991-01-01 01:01:01	01:01:01	1991
32	1991-01-01	1991-01-01 01:01:01	1991-01-01 01:01:01	01:01:01	1991
33	1991-01-01	1991-01-01 01:01:01	1991-01-01 01:01:01	01:01:01	1991
40	0000-00-00	0000-00-00 00:00:00	0000-00-00 00:00:00	838:59:59	0000
41	0000-00-00	0000-00-00 00:00:00	0000-00-00 00:00:00	838:59:59	0000
42	0000-00-00	0000-00-00 00:00:00	0000-00-00 00:00:00	838:59:59	0000
43	0000-00-00	0000-00-00 00:00:00	0000-00-00 00:00:00	838:59:59	0000
50	2001-00-00	2001-00-00 00:00:00	0000-00-00 00:00:00	838:59:59	0000
51	0010-00-00	0010-00-00 00:00:00	0000-00-00 00:00:00	838:59:59	0000
52	2001-00-00	2001-00-00 00:00:00	0000-00-00 00:00:00	838:59:59	0000
53	2001-00-00	2001-00-00 00:00:00	0000-00-00 00:00:00	838:59:59	0000
60	NULL	NULL	1991-01-01 01:01:01	NULL	NULL
61	NULL	NULL	1991-01-01 01:01:01	NULL	NULL
62	NULL	NULL	1991-01-01 01:01:01	NULL	NULL
63	NULL	NULL	1991-01-01 01:01:01	NULL	NULL
71	NULL	NULL	1991-01-01 01:01:01	NULL	NULL
73	NULL	NULL	1991-01-01 01:01:01	NULL	NULL
81	NULL	NULL	1991-01-01 01:01:01	NULL	NULL
83	NULL	NULL	1991-01-01 01:01:01	NULL	NULL
test_sequence
-- select .. where date/time column = .. --
set @arg00= '1991-01-01 01:01:01' ;
select 'true' as found from t9 
where c1= 20 and c13= '1991-01-01 01:01:01' and c14= '1991-01-01 01:01:01' and
c15= '1991-01-01 01:01:01' and c16= '1991-01-01 01:01:01' and
c17= '1991-01-01 01:01:01' ;
found
true
select 'true' as found from t9 
where c1= 20 and c13= @arg00 and c14= @arg00 and c15= @arg00 and c16= @arg00
and c17= @arg00 ;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and c13= '1991-01-01 01:01:01' and c14= '1991-01-01 01:01:01' and
  c15= '1991-01-01 01:01:01' and c16= '1991-01-01 01:01:01' and
  c17= '1991-01-01 01:01:01'" ;
execute stmt1 ;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and c13= ? and c14= ? and c15= ? and c16= ? and c17= ?" ;
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00 ;
found
true
set @arg00= CAST('1991-01-01 01:01:01' as datetime) ;
select 'true' as found from t9 
where c1= 20 and c13= CAST('1991-01-01 01:01:01' as datetime) and
c14= CAST('1991-01-01 01:01:01' as datetime) and
c15= CAST('1991-01-01 01:01:01' as datetime) and
c16= CAST('1991-01-01 01:01:01' as datetime) and
c17= CAST('1991-01-01 01:01:01' as datetime) ;
found
true
select 'true' as found from t9 
where c1= 20 and c13= @arg00 and c14= @arg00 and c15= @arg00 and c16= @arg00
and c17= @arg00 ;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and c13= CAST('1991-01-01 01:01:01' as datetime) and
  c14= CAST('1991-01-01 01:01:01' as datetime) and
  c15= CAST('1991-01-01 01:01:01' as datetime) and
  c16= CAST('1991-01-01 01:01:01' as datetime) and
  c17= CAST('1991-01-01 01:01:01' as datetime)" ;
execute stmt1 ;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and c13= ? and c14= ? and c15= ? and c16= ? and c17= ?" ;
execute stmt1 using @arg00, @arg00, @arg00, @arg00, @arg00 ;
found
true
set @arg00= 1991 ;
select 'true' as found from t9 
where c1= 20 and c17= 1991 ;
found
true
select 'true' as found from t9 
where c1= 20 and c17= @arg00 ;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and c17= 1991" ;
execute stmt1 ;
found
true
prepare stmt1 from "select 'true' as found from t9
where c1= 20 and c17= ?" ;
execute stmt1 using @arg00 ;
found
true
set @arg00= 1.991e+3 ;
select 'true' as found from t9 
where c1= 20 and abs(c17 - 1.991e+3) < 0.01 ;
found
true
select 'true' as found from t9 
where c1= 20 and abs(c17 - @arg00) < 0.01 ;
found
true
prepare stmt1 from "select 'true' as found from t9 
where c1= 20 and abs(c17 - 1.991e+3) < 0.01" ;
execute stmt1 ;
found
true
prepare stmt1 from "select 'true' as found from t9
where c1= 20 and abs(c17 - ?) < 0.01" ;
execute stmt1 using @arg00 ;
found
true
drop table t1, t1_1, t1_2, 
t9_1, t9_2, t9;
