drop table if exists t1,t2;
create table t1 (bandID MEDIUMINT UNSIGNED NOT NULL PRIMARY KEY, payoutID SMALLINT UNSIGNED NOT NULL);
insert into t1 (bandID,payoutID) VALUES (1,6),(2,6),(3,4),(4,9),(5,10),(6,1),(7,12),(8,12);
create table t2 (payoutID SMALLINT UNSIGNED NOT NULL PRIMARY KEY);
insert into t2 (payoutID) SELECT DISTINCT payoutID FROM t1;
insert into t2 (payoutID) SELECT payoutID+10 FROM t1;
ERROR 23000: Duplicate entry '16' for key 1
insert ignore into t2 (payoutID) SELECT payoutID+10 FROM t1;
select * from t2;
payoutID
1
4
6
9
10
11
12
14
16
19
20
22
drop table t1,t2;
CREATE TABLE `t1` (
`numeropost` bigint(20) unsigned NOT NULL default '0',
`icone` tinyint(4) unsigned NOT NULL default '0',
`numreponse` bigint(20) unsigned NOT NULL auto_increment,
`contenu` text NOT NULL,
`pseudo` varchar(50) NOT NULL default '',
`date` datetime NOT NULL default '0000-00-00 00:00:00',
`ip` bigint(11) NOT NULL default '0',
`signature` tinyint(1) unsigned NOT NULL default '0',
PRIMARY KEY  (`numeropost`,`numreponse`)
,KEY `ip` (`ip`),
KEY `date` (`date`),
KEY `pseudo` (`pseudo`),
KEY `numreponse` (`numreponse`)
) ENGINE=MyISAM;
CREATE TABLE `t2` (
`numeropost` bigint(20) unsigned NOT NULL default '0',
`icone` tinyint(4) unsigned NOT NULL default '0',
`numreponse` bigint(20) unsigned NOT NULL auto_increment,
`contenu` text NOT NULL,
`pseudo` varchar(50) NOT NULL default '',
`date` datetime NOT NULL default '0000-00-00 00:00:00',
`ip` bigint(11) NOT NULL default '0',
`signature` tinyint(1) unsigned NOT NULL default '0',
PRIMARY KEY  (`numeropost`,`numreponse`),
KEY `ip` (`ip`),
KEY `date` (`date`),
KEY `pseudo` (`pseudo`),
KEY `numreponse` (`numreponse`)
) ENGINE=MyISAM;
INSERT INTO t2
(numeropost,icone,numreponse,contenu,pseudo,date,ip,signature) VALUES
(9,1,56,'test','joce','2001-07-25 13:50:53'
,3649052399,0);
INSERT INTO t1 (numeropost,icone,contenu,pseudo,date,signature,ip)
SELECT 1618,icone,contenu,pseudo,date,signature,ip FROM t2
WHERE numeropost=9 ORDER BY numreponse ASC;
show variables like '%bulk%';
Variable_name	Value
bulk_insert_buffer_size	8388608
INSERT INTO t1 (numeropost,icone,contenu,pseudo,date,signature,ip)
SELECT 1718,icone,contenu,pseudo,date,signature,ip FROM t2
WHERE numeropost=9 ORDER BY numreponse ASC;
DROP TABLE t1,t2;
create table t1(a int, unique(a));
insert into t1 values(2);
create table t2(a int);
insert into t2 values(1),(2);
reset master;
insert into t1 select * from t2;
ERROR 23000: Duplicate entry '2' for key 1
show binlog events;
select * from t1;
a
1
2
drop table t1, t2;
create table t1 (a int not null);
create table t2 (a int not null);
insert into t1 values (1);
insert into t1 values (a+2);
insert into t1 values (a+3);
insert into t1 values (4),(a+5);
insert into t1 select * from t1;
select * from t1;
a
1
2
3
4
5
1
2
3
4
5
insert into t1 select * from t1 as t2;
select * from t1;
a
1
2
3
4
5
1
2
3
4
5
1
2
3
4
5
1
2
3
4
5
insert into t2 select * from t1 as t2;
select * from t1;
a
1
2
3
4
5
1
2
3
4
5
1
2
3
4
5
1
2
3
4
5
insert into t1 select t2.a from t1,t2;
select * from t1;
a
1
2
3
4
5
1
2
3
4
5
1
2
3
4
5
1
2
3
4
5
1
1
1
1
1
1
1
1
1
1
1
1
1
1
1
1
1
1
1
1
2
2
2
2
2
2
2
2
2
2
2
2
2
2
2
2
2
2
2
2
3
3
3
3
3
3
3
3
3
3
3
3
3
3
3
3
3
3
3
3
4
4
4
4
4
4
4
4
4
4
4
4
4
4
4
4
4
4
4
4
5
5
5
5
5
5
5
5
5
5
5
5
5
5
5
5
5
5
5
5
1
1
1
1
1
1
1
1
1
1
1
1
1
1
1
1
1
1
1
1
2
2
2
2
2
2
2
2
2
2
2
2
2
2
2
2
2
2
2
2
3
3
3
3
3
3
3
3
3
3
3
3
3
3
3
3
3
3
3
3
4
4
4
4
4
4
4
4
4
4
4
4
4
4
4
4
4
4
4
4
5
5
5
5
5
5
5
5
5
5
5
5
5
5
5
5
5
5
5
5
1
1
1
1
1
1
1
1
1
1
1
1
1
1
1
1
1
1
1
1
2
2
2
2
2
2
2
2
2
2
2
2
2
2
2
2
2
2
2
2
3
3
3
3
3
3
3
3
3
3
3
3
3
3
3
3
3
3
3
3
4
4
4
4
4
4
4
4
4
4
4
4
4
4
4
4
4
4
4
4
5
5
5
5
5
5
5
5
5
5
5
5
5
5
5
5
5
5
5
5
1
1
1
1
1
1
1
1
1
1
1
1
1
1
1
1
1
1
1
1
2
2
2
2
2
2
2
2
2
2
2
2
2
2
2
2
2
2
2
2
3
3
3
3
3
3
3
3
3
3
3
3
3
3
3
3
3
3
3
3
4
4
4
4
4
4
4
4
4
4
4
4
4
4
4
4
4
4
4
4
5
5
5
5
5
5
5
5
5
5
5
5
5
5
5
5
5
5
5
5
insert into t1 select * from t1,t1;
ERROR 42000: Not unique table/alias: 't1'
drop table t1,t2;
create table t1 (a int not null primary key, b char(10));
create table t2 (a int not null, b char(10));
insert into t1 values (1,"t1:1"),(3,"t1:3");
insert into t2 values (2,"t2:2"), (3,"t2:3");
insert into t1 select * from t2;
ERROR 23000: Duplicate entry '3' for key 1
select * from t1;
a	b
1	t1:1
3	t1:3
2	t2:2
replace into t1 select * from t2;
select * from t1;
a	b
1	t1:1
3	t2:3
2	t2:2
drop table t1,t2;
CREATE TABLE t1 ( USID INTEGER UNSIGNED, ServerID TINYINT UNSIGNED, State ENUM ('unknown', 'Access-Granted', 'Session-Active', 'Session-Closed' ) NOT NULL DEFAULT 'unknown', SessionID CHAR(32), User CHAR(32) NOT NULL DEFAULT '<UNKNOWN>', NASAddr INTEGER UNSIGNED, NASPort INTEGER UNSIGNED, NASPortType INTEGER UNSIGNED, ConnectSpeed INTEGER UNSIGNED, CarrierType CHAR(32), CallingStationID CHAR(32), CalledStationID CHAR(32), AssignedAddr INTEGER UNSIGNED, SessionTime INTEGER UNSIGNED, PacketsIn INTEGER UNSIGNED, OctetsIn INTEGER UNSIGNED, PacketsOut INTEGER UNSIGNED, OctetsOut INTEGER UNSIGNED, TerminateCause INTEGER UNSIGNED, UnauthTime TINYINT UNSIGNED, AccessRequestTime DATETIME, AcctStartTime DATETIME, AcctLastTime DATETIME, LastModification TIMESTAMP NOT NULL);
CREATE TABLE t2 ( USID INTEGER UNSIGNED AUTO_INCREMENT, ServerID TINYINT UNSIGNED, State ENUM ('unknown', 'Access-Granted', 'Session-Active', 'Session-Closed' ) NOT NULL DEFAULT 'unknown', SessionID CHAR(32), User TEXT NOT NULL, NASAddr INTEGER UNSIGNED, NASPort INTEGER UNSIGNED, NASPortType INTEGER UNSIGNED, ConnectSpeed INTEGER UNSIGNED, CarrierType CHAR(32), CallingStationID CHAR(32), CalledStationID CHAR(32), AssignedAddr INTEGER UNSIGNED, SessionTime INTEGER UNSIGNED, PacketsIn INTEGER UNSIGNED, OctetsIn INTEGER UNSIGNED, PacketsOut INTEGER UNSIGNED, OctetsOut INTEGER UNSIGNED, TerminateCause INTEGER UNSIGNED, UnauthTime TINYINT UNSIGNED, AccessRequestTime DATETIME, AcctStartTime DATETIME, AcctLastTime DATETIME, LastModification TIMESTAMP NOT NULL, INDEX(USID,ServerID,NASAddr,SessionID), INDEX(AssignedAddr));
INSERT INTO t1 VALUES (39,42,'Access-Granted','46','491721000045',2130706433,17690,NULL,NULL,'Localnet','491721000045','49172200000',754974766,NULL,NULL,NULL,NULL,NULL,NULL,NULL,'2003-07-18 00:11:21',NULL,NULL,20030718001121);
INSERT INTO t2 SELECT USID, ServerID, State, SessionID, User, NASAddr, NASPort, NASPortType, ConnectSpeed, CarrierType, CallingStationID, CalledStationID, AssignedAddr, SessionTime, PacketsIn, OctetsIn, PacketsOut, OctetsOut, TerminateCause, UnauthTime, AccessRequestTime, AcctStartTime, AcctLastTime, LastModification from t1 LIMIT 1;
drop table t1,t2;
CREATE TABLE t1(
Month date NOT NULL,
Type tinyint(3) unsigned NOT NULL auto_increment,
Field int(10) unsigned NOT NULL,
Count int(10) unsigned NOT NULL,
UNIQUE KEY Month (Month,Type,Field)
);
insert into t1 Values
(20030901, 1, 1, 100),
(20030901, 1, 2, 100),
(20030901, 2, 1, 100),
(20030901, 2, 2, 100),
(20030901, 3, 1, 100);
select * from t1;
Month	Type	Field	Count
2003-09-01	1	1	100
2003-09-01	1	2	100
2003-09-01	2	1	100
2003-09-01	2	2	100
2003-09-01	3	1	100
Select null, Field, Count From t1 Where Month=20030901 and Type=2;
NULL	Field	Count
NULL	1	100
NULL	2	100
create table t2(No int not null, Field int not null, Count int not null);
insert into t2 Select null, Field, Count From t1 Where Month=20030901 and Type=2;
Warnings:
Warning	1263	Data truncated; NULL supplied to NOT NULL column 'No' at row 1
Warning	1263	Data truncated; NULL supplied to NOT NULL column 'No' at row 2
select * from t2;
No	Field	Count
0	1	100
0	2	100
drop table t1, t2;