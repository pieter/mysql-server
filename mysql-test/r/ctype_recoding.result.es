SET CHARACTER SET koi8r;
DROP TABLE IF EXISTS �������, t1, t2;
SET CHARACTER SET koi8r;
CREATE TABLE t1 (a CHAR(10) CHARACTER SET cp1251) SELECT _koi8r'�����' AS a;
CREATE TABLE t2 (a CHAR(10) CHARACTER SET utf8);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` char(10) character set cp1251 default NULL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
SELECT a FROM t1;
a
�����
SELECT HEX(a) FROM t1;
HEX(a)
EFF0EEE1E0
INSERT t2 SELECT * FROM t1;
SELECT HEX(a) FROM t2;
HEX(a)
D0BFD180D0BED0B1D0B0
DROP TABLE t1, t2;
CREATE TABLE t1 (description text character set cp1250 NOT NULL);
INSERT INTO t1 (description) VALUES (_latin2'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaasssssssssssaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbcccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddde');
SELECT description FROM t1;
description
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaasssssssssssaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbcccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddde
DROP TABLE t1;
CREATE TABLE t1 (a TEXT CHARACTER SET cp1251) SELECT _koi8r'�����' AS a;
CREATE TABLE t2 (a TEXT CHARACTER SET utf8);
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `a` text character set cp1251
) ENGINE=MyISAM DEFAULT CHARSET=latin1
SELECT HEX(a) FROM t1;
HEX(a)
EFF0EEE1E0
INSERT t2 SELECT * FROM t1;
SELECT HEX(a) FROM t2;
HEX(a)
D0BFD180D0BED0B1D0B0
DROP TABLE t1, t2;
CREATE TABLE `�������`
(
���� CHAR(32) CHARACTER SET koi8r NOT NULL COMMENT "����������� ����"
) COMMENT "����������� �������";
SHOW TABLES;
Tables_in_test
�������
SHOW CREATE TABLE �������;
Table	Create Table
�������	CREATE TABLE `�������` (
  `����` char(32) character set koi8r NOT NULL default '' COMMENT '����������� ����'
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COMMENT='����������� �������'
SHOW FIELDS FROM �������;
Field	Type	Null	Key	Default	Extra
����	char(32)				
SET CHARACTER SET cp1251;
SHOW TABLES;
Tables_in_test
�������
SHOW CREATE TABLE �������;
Table	Create Table
�������	CREATE TABLE `�������` (
  `����` char(32) character set koi8r NOT NULL default '' COMMENT '����������� ����'
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COMMENT='����������� �������'
SHOW FIELDS FROM �������;
Field	Type	Null	Key	Default	Extra
����	char(32)				
SET CHARACTER SET utf8;
SHOW TABLES;
Tables_in_test
таблица
SHOW CREATE TABLE таблица;
Table	Create Table
таблица	CREATE TABLE `таблица` (
  `поле` char(32) character set koi8r NOT NULL default '' COMMENT 'комментарий поля'
) ENGINE=MyISAM DEFAULT CHARSET=latin1 COMMENT='комментарий таблицы'
SHOW FIELDS FROM таблица;
Field	Type	Null	Key	Default	Extra
поле	char(32)				
SET CHARACTER SET koi8r;
DROP TABLE �������;
SET CHARACTER SET default;
SET NAMES UTF8;
CREATE TABLE t1 (t text) DEFAULT CHARSET UTF8;
INSERT INTO t1 (t) VALUES ('x');
SELECT 1 FROM t1 WHERE CONCAT(_latin1'x') = t;
1
1
DROP TABLE t1;
SET CHARACTER SET koi8r;
CREATE DATABASE ����;
USE ����;
SHOW TABLES;
Tables_in_тест
SHOW TABLES IN ����;
Tables_in_тест
SET CHARACTER SET cp1251;
SHOW TABLES;
Tables_in_тест
SHOW TABLES IN ����;
Tables_in_тест
SET CHARACTER SET koi8r;
DROP DATABASE ����;
SET NAMES koi8r;
SELECT hex('����');
hex('тест')
D4C5D3D4
SET character_set_connection=cp1251;
SELECT hex('����');
hex('тест')
F2E5F1F2
USE test;
SET NAMES binary;
CREATE TABLE `тест` (`тест` int);
SHOW CREATE TABLE `тест`;
Table	Create Table
тест	CREATE TABLE `тест` (
  `тест` int(11) default NULL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
SET NAMES utf8;
SHOW CREATE TABLE `тест`;
Table	Create Table
тест	CREATE TABLE `тест` (
  `тест` int(11) default NULL
) ENGINE=MyISAM DEFAULT CHARSET=latin1
DROP TABLE `тест`;
SET NAMES binary;
SET character_set_connection=utf8;
SELECT 'тест' as s;
s
тест
SET NAMES utf8;
SET character_set_connection=binary;
SELECT 'тест' as s;
s
тест
SET NAMES latin1;
CREATE TABLE t1 (`�` CHAR(128) DEFAULT '�', `�1` ENUM('�1','�2') DEFAULT '�2');
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `�` char(128) default '�',
  `�1` enum('�1','�2') default '�2'
) ENGINE=MyISAM DEFAULT CHARSET=latin1
SHOW COLUMNS FROM t1;
Field	Type	Null	Key	Default	Extra
�	char(128)	YES		�	
�1	enum('�1','�2')	YES		�2	
SET NAMES binary;
SHOW CREATE TABLE t1;
Table	Create Table
t1	CREATE TABLE `t1` (
  `ä` char(128) default 'ä',
  `ä1` enum('ä1','ä2') default 'ä2'
) ENGINE=MyISAM DEFAULT CHARSET=latin1
SHOW COLUMNS FROM t1;
Field	Type	Null	Key	Default	Extra
ä	char(128)	YES		ä	
ä1	enum('ä1','ä2')	YES		ä2	
DROP TABLE t1;
SET NAMES binary;
CREATE TABLE `good�����` (a int);
ERROR HY000: Invalid utf8 character string: '�����'
SET NAMES utf8;
CREATE TABLE `good�����` (a int);
ERROR HY000: Invalid utf8 character string: '�����` (a int)'
set names latin1;
create table t1 (a char(10) character set koi8r, b text character set koi8r);
insert into t1 values ('test','test');
insert into t1 values ('����','����');
Warnings:
Warning	1265	Data truncated for column 'a' at row 1
Warning	1265	Data truncated for column 'b' at row 1
drop table t1;
set names koi8r;
create table t1 (a char(10) character set cp1251);
insert into t1 values (_koi8r'����');
select * from t1 where a=_koi8r'����';
a
����
select * from t1 where a=concat(_koi8r'����');
ERROR HY000: Illegal mix of collations (cp1251_general_ci,IMPLICIT) and (koi8r_general_ci,COERCIBLE) for operation '='
select * from t1 where a=_latin1'����';
ERROR HY000: Illegal mix of collations (cp1251_general_ci,IMPLICIT) and (latin1_swedish_ci,COERCIBLE) for operation '='
drop table t1;
set names latin1;
set names koi8r;
create table t1 (c1 char(10) character set cp1251);
insert into t1 values ('�');
select c1 from t1 where c1 between '�' and '�';
c1
�
select ifnull(c1,'�'), ifnull(null,c1) from t1;
ifnull(c1,'ъ')	ifnull(null,c1)
�	�
select if(1,c1,'�'), if(0,c1,'�') from t1;
if(1,c1,'Ж')	if(0,c1,'Ж')
�	�
select coalesce('�',c1), coalesce(null,c1) from t1;
coalesce('Ж',c1)	coalesce(null,c1)
�	�
select least(c1,'�'), greatest(c1,'�') from t1;
least(c1,'Ж')	greatest(c1,'Ж')
�	�
select locate(c1,'�'), locate('�',c1) from t1;
locate(c1,'ъ')	locate('ъ',c1)
1	1
select field(c1,'�'),field('�',c1) from t1;
field(c1,'ъ')	field('ъ',c1)
1	1
select concat(c1,'�'), concat('�',c1) from t1;
concat(c1,'Ж')	concat('Ж',c1)
��	��
select concat_ws(c1,'�','�'), concat_ws('�',c1,'�') from t1;
concat_ws(c1,'Ж','ъ')	concat_ws('Ж',c1,'ъ')
���	���
select replace(c1,'�','�'), replace('�',c1,'�') from t1;
replace(c1,'ъ','Ж')	replace('ъ',c1,'Ж')
�	�
select substring_index(c1,'����',2) from t1;
substring_index(c1,'ЖЖъъ',2)
�
select elt(1,c1,'�'),elt(1,'�',c1) from t1;
elt(1,c1,'Ж')	elt(1,'Ж',c1)
�	�
select make_set(3,c1,'�'), make_set(3,'�',c1) from t1;
make_set(3,c1,'Ж')	make_set(3,'Ж',c1)
�,�	�,�
select insert(c1,1,2,'�'),insert('�',1,2,c1) from t1;
insert(c1,1,2,'Ж')	insert('Ж',1,2,c1)
�	�
select trim(c1 from '�'),trim('�' from c1) from t1;
trim(c1 from 'ъ')	trim('ъ' from c1)
	
select lpad(c1,3,'�'), lpad('�',3,c1) from t1;
lpad(c1,3,'Ж')	lpad('Ж',3,c1)
���	���
select rpad(c1,3,'�'), rpad('�',3,c1) from t1;
rpad(c1,3,'Ж')	rpad('Ж',3,c1)
���	���
