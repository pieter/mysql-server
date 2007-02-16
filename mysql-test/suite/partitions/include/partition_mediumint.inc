--disable_query_log
# DATA DIRECTORY
eval SET @data_dir = 'DATA DIRECTORY =
''/tmp''';
let $data_directory = `select @data_dir`;

#INDEX DIRECTORY
eval SET @indx_dir = 'INDEX DIRECTORY =
''/tmp''';
let $index_directory = `select @indx_dir`;
--enable_query_log

eval create table t1 (a mediumint unsigned not null, primary key(a)) engine=$engine 
partition by key (a) (
partition pa1 $data_directory $index_directory max_rows=20 min_rows=2,
partition pa2 $data_directory $index_directory max_rows=30 min_rows=3,
partition pa3 $data_directory $index_directory max_rows=30 min_rows=4,
partition pa4 $data_directory $index_directory max_rows=40 min_rows=2);
show create table t1;
insert into t1 values (16777215), (16777214), (16777213), (16777212), (1), (2), (65535);
select * from t1;
select * from t1 where a=16777213;
delete from t1 where a=16777213;
select * from t1;
drop table t1;

eval create table t2 (a mediumint unsigned not null, primary key(a)) engine=$engine 
partition by key (a) partitions 8;
show create table t2;
insert into t2 values (16777215), (16777214), (16777213), (16777212);
select * from t2;
select * from t2 where a=16777213;
delete from t2 where a=16777213;
select * from t2;
delete from t2;
let $count=$maxrows;
--echo $maxrows inserts;
--disable_query_log
while ($count)
{
eval insert into t2 values ($count);
dec $count;
}
--enable_query_log
select count(*) from t2;
drop table t2;

eval create table t3 (a mediumint not null, primary key(a)) engine=$engine 
partition by key (a) partitions 10;
show create table t3;
insert into t3 values (8388607), (8388606), (8388605), (8388604), (-8388608), (-8388607), (1), (-1), (0);
select * from t3;
select * from t3 where a=8388605;
delete from t3 where a=8388605;
select * from t3;
drop table t3;