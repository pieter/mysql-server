-- This script converts any old privilege tables to privilege tables suitable
-- for MySQL 4.0.

-- You can safely ignore all 'Duplicate column' and 'Unknown column' errors"
-- because these just mean that your tables are already up to date.
-- This script is safe to run even if your tables are already up to date!

-- On unix, you should use the mysql_fix_privilege_tables script to execute
-- this sql script.
-- On windows you should do 'mysql --force mysql < mysql_fix_privilege_tables.sql'

-- Convert all tables to UTF-8 with binary collation
-- and reset all char columns to correct width
ALTER TABLE user
  MODIFY Host char(60) NOT NULL default '',
  MODIFY User char(16) NOT NULL default '',
  MODIFY Password char(41) NOT NULL default '',
  ENGINE=MyISAM, CONVERT TO CHARACTER SET utf8 COLLATE utf8_bin;
ALTER TABLE db
  MODIFY Host char(60) NOT NULL default '',
  MODIFY Db char(64) NOT NULL default '',
  MODIFY User char(16) NOT NULL default '',
  ENGINE=MyISAM, CONVERT TO CHARACTER SET utf8 COLLATE utf8_bin;
ALTER TABLE host
  MODIFY Host char(60) NOT NULL default '',
  MODIFY Db char(64) NOT NULL default '',
  ENGINE=MyISAM, CONVERT TO CHARACTER SET utf8 COLLATE utf8_bin;
ALTER TABLE func
  ENGINE=MyISAM, CONVERT TO CHARACTER SET utf8 COLLATE utf8_bin;
ALTER TABLE columns_priv
  MODIFY Host char(60) NOT NULL default '',
  MODIFY Db char(64) NOT NULL default '',
  MODIFY User char(16) NOT NULL default '',
  MODIFY Table_name char(64) NOT NULL default '',
  MODIFY Column_name char(64) NOT NULL default '',
  ENGINE=MyISAM, CONVERT TO CHARACTER SET utf8 COLLATE utf8_bin;
ALTER TABLE tables_priv
  MODIFY Host char(60) NOT NULL default '',
  MODIFY Db char(64) NOT NULL default '',
  MODIFY User char(16) NOT NULL default '',
  MODIFY Table_name char(64) NOT NULL default '',
  MODIFY Grantor char(77) NOT NULL default '',
  ENGINE=MyISAM, CONVERT TO CHARACTER SET utf8 COLLATE utf8_bin;
ALTER TABLE procs_priv type=MyISAM, CONVERT TO CHARACTER SET utf8 COLLATE utf8_bin;
ALTER TABLE user add File_priv enum('N','Y') NOT NULL;
CREATE TABLE IF NOT EXISTS func (
  name char(64) binary DEFAULT '' NOT NULL,
  ret tinyint(1) DEFAULT '0' NOT NULL,
  dl char(128) DEFAULT '' NOT NULL,
  type enum ('function','aggregate') NOT NULL,
  PRIMARY KEY (name)
) CHARACTER SET utf8 COLLATE utf8_bin;

-- Detect whether or not we had the Grant_priv column
SET @hadGrantPriv:=0;
SELECT @hadGrantPriv:=1 FROM user WHERE Grant_priv LIKE '%';

ALTER TABLE user add Grant_priv enum('N','Y') NOT NULL,add References_priv enum('N','Y') NOT NULL,add Index_priv enum('N','Y') NOT NULL,add Alter_priv enum('N','Y') NOT NULL;
ALTER TABLE host add Grant_priv enum('N','Y') NOT NULL,add References_priv enum('N','Y') NOT NULL,add Index_priv enum('N','Y') NOT NULL,add Alter_priv enum('N','Y') NOT NULL;
ALTER TABLE db add Grant_priv enum('N','Y') NOT NULL,add References_priv enum('N','Y') NOT NULL,add Index_priv enum('N','Y') NOT NULL,add Alter_priv enum('N','Y') NOT NULL;

--- Fix privileges for old tables
UPDATE user SET Grant_priv=File_priv,References_priv=Create_priv,Index_priv=Create_priv,Alter_priv=Create_priv WHERE @hadGrantPriv = 0;
UPDATE db SET References_priv=Create_priv,Index_priv=Create_priv,Alter_priv=Create_priv WHERE @hadGrantPriv = 0;
UPDATE host SET References_priv=Create_priv,Index_priv=Create_priv,Alter_priv=Create_priv WHERE @hadGrantPriv = 0;

--
-- The second alter changes ssl_type to new 4.0.2 format
-- Adding columns needed by GRANT .. REQUIRE (openssl)"

ALTER TABLE user
ADD ssl_type enum('','ANY','X509', 'SPECIFIED') NOT NULL,
ADD ssl_cipher BLOB NOT NULL,
ADD x509_issuer BLOB NOT NULL,
ADD x509_subject BLOB NOT NULL;
ALTER TABLE user MODIFY ssl_type enum('','ANY','X509', 'SPECIFIED') NOT NULL;

--
--  Create tables_priv and columns_priv if they don't exists
--

CREATE TABLE IF NOT EXISTS tables_priv (
  Host char(60) binary DEFAULT '' NOT NULL,
  Db char(64) binary DEFAULT '' NOT NULL,
  User char(16) binary DEFAULT '' NOT NULL,
  Table_name char(64) binary DEFAULT '' NOT NULL,
  Grantor char(77) DEFAULT '' NOT NULL,
  Timestamp timestamp(14),
  Table_priv set('Select','Insert','Update','Delete','Create','Drop','Grant','References','Index','Alter') DEFAULT '' NOT NULL,
  Column_priv set('Select','Insert','Update','References') DEFAULT '' NOT NULL,
  PRIMARY KEY (Host,Db,User,Table_name)
) CHARACTER SET utf8 COLLATE utf8_bin;

CREATE TABLE IF NOT EXISTS columns_priv (
  Host char(60) DEFAULT '' NOT NULL,
  Db char(64) DEFAULT '' NOT NULL,
  User char(16) DEFAULT '' NOT NULL,
  Table_name char(64) DEFAULT '' NOT NULL,
  Column_name char(64) DEFAULT '' NOT NULL,
  Timestamp timestamp(14),
  Column_priv set('Select','Insert','Update','References') DEFAULT '' NOT NULL,
  PRIMARY KEY (Host,Db,User,Table_name,Column_name)
) CHARACTER SET utf8 COLLATE utf8_bin;


--
-- Name change of Type -> Column_priv from MySQL 3.22.12
--

ALTER TABLE columns_priv change Type Column_priv set('Select','Insert','Update','References') DEFAULT '' NOT NULL;

--
--  Add the new 'type' column to the func table.
--

ALTER TABLE func add type enum ('function','aggregate') NOT NULL;

--
--  Change the user,db and host tables to MySQL 4.0 format
--

# Detect whether we had Show_db_priv
SET @hadShowDbPriv:=0;
SELECT @hadShowDbPriv:=1 FROM user WHERE Show_db_priv LIKE '%';

ALTER TABLE user
ADD Show_db_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Alter_priv,
ADD Super_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Show_db_priv,
ADD Create_tmp_table_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Super_priv,
ADD Lock_tables_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Create_tmp_table_priv,
ADD Execute_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Lock_tables_priv,
ADD Repl_slave_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Execute_priv,
ADD Repl_client_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Repl_slave_priv;

-- Convert privileges so that users have similar privileges as before

UPDATE user SET Show_db_priv= Select_priv, Super_priv=Process_priv, Execute_priv=Process_priv, Create_tmp_table_priv='Y', Lock_tables_priv='Y', Repl_slave_priv=file_priv, Repl_client_priv=File_priv where user<>"" AND @hadShowDbPriv = 0;


--  Add fields that can be used to limit number of questions and connections
--  for some users.

ALTER TABLE user
ADD max_questions int(11) NOT NULL DEFAULT 0 AFTER x509_subject,
ADD max_updates   int(11) unsigned NOT NULL DEFAULT 0 AFTER max_questions,
ADD max_connections int(11) unsigned NOT NULL DEFAULT 0 AFTER max_updates;


--
--  Add Create_tmp_table_priv and Lock_tables_priv to db and host
--

ALTER TABLE db
ADD Create_tmp_table_priv enum('N','Y') DEFAULT 'N' NOT NULL,
ADD Lock_tables_priv enum('N','Y') DEFAULT 'N' NOT NULL;
ALTER TABLE host
ADD Create_tmp_table_priv enum('N','Y') DEFAULT 'N' NOT NULL,
ADD Lock_tables_priv enum('N','Y') DEFAULT 'N' NOT NULL;

alter table user change max_questions max_questions int(11) unsigned DEFAULT 0  NOT NULL;
alter table tables_priv add KEY Grantor (Grantor);

alter table db comment='Database privileges';
alter table host comment='Host privileges;  Merged with database privileges';
alter table user comment='Users and global privileges';
alter table func comment='User defined functions';
alter table tables_priv comment='Table privileges';
alter table columns_priv comment='Column privileges';

#
# Detect whether we had Create_view_priv
# 
SET @hadCreateViewPriv:=0;
SELECT @hadCreateViewPriv:=1 FROM user WHERE Create_view_priv LIKE '%';

#
# Create VIEWs privileges (v5.0)
#
ALTER TABLE db ADD Create_view_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Lock_tables_priv;
ALTER TABLE host ADD Create_view_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Lock_tables_priv;
ALTER TABLE user ADD Create_view_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Repl_client_priv;

#
# Show VIEWs privileges (v5.0)
#
ALTER TABLE db ADD Show_view_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Create_view_priv;
ALTER TABLE host ADD Show_view_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Create_view_priv;
ALTER TABLE user ADD Show_view_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Create_view_priv;

#
# Assign create/show view privileges to people who have create provileges
#
UPDATE user SET Create_view_priv=Create_priv, Show_view_priv=Create_priv where user<>"" AND @hadCreateViewPriv = 0;

#
#
#
SET @hadCreateRoutinePriv:=0;
SELECT @hadCreateRoutinePriv:=1 FROM user WHERE Create_routine_priv LIKE '%';

#
# Create PROCEDUREs privileges (v5.0)
#
ALTER TABLE db ADD Create_routine_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Show_view_priv;
ALTER TABLE user ADD Create_routine_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Show_view_priv;

#
# Alter PROCEDUREs privileges (v5.0)
#
ALTER TABLE db ADD Alter_routine_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Create_routine_priv;
ALTER TABLE user ADD Alter_routine_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Create_routine_priv;

ALTER TABLE db ADD Execute_priv enum('N','Y') DEFAULT 'N' NOT NULL AFTER Alter_routine_priv;

#
# Assign create/alter routine privileges to people who have create privileges
#
UPDATE user SET Create_routine_priv=Create_priv, Alter_routine_priv=Alter_priv where user<>"" AND @hadCreateRoutinePriv = 0;
UPDATE db SET Create_routine_priv=Create_priv, Alter_routine_priv=Alter_priv, Execute_priv=Select_priv where user<>"" AND @hadCreateRoutinePriv = 0;

#
# Add max_user_connections resource limit 
#
ALTER TABLE user ADD max_user_connections int(11) unsigned DEFAULT '0' NOT NULL AFTER max_connections;

#
# Create some possible missing tables
#
CREATE TABLE IF NOT EXISTS procs_priv (
Host char(60) binary DEFAULT '' NOT NULL,
Db char(64) binary DEFAULT '' NOT NULL,
User char(16) binary DEFAULT '' NOT NULL,
Routine_name char(64) binary DEFAULT '' NOT NULL,
Grantor char(77) DEFAULT '' NOT NULL,
Timestamp timestamp(14),
Proc_priv set('Execute','Alter Routine','Grant') DEFAULT '' NOT NULL,
PRIMARY KEY (Host,Db,User,Routine_name),
KEY Grantor (Grantor)
) CHARACTER SET utf8 COLLATE utf8_bin comment='Procedure privileges';

CREATE TABLE IF NOT EXISTS help_topic (
help_topic_id int unsigned not null,
name varchar(64) not null,
help_category_id smallint unsigned not null,
description text not null,
example text not null,
url varchar(128) not null,
primary key (help_topic_id), unique index (name)
) CHARACTER SET utf8 comment='help topics';

CREATE TABLE IF NOT EXISTS help_category (
help_category_id smallint unsigned not null,
name varchar(64) not null,
parent_category_id smallint unsigned null,
url varchar(128) not null,
primary key (help_category_id),
unique index (name)
) CHARACTER SET utf8 comment='help categories';

CREATE TABLE IF NOT EXISTS help_relation (
help_topic_id int unsigned not null references help_topic,
help_keyword_id  int unsigned not null references help_keyword,
primary key (help_keyword_id, help_topic_id)
) CHARACTER SET utf8 comment='keyword-topic relation';

CREATE TABLE IF NOT EXISTS help_keyword (
help_keyword_id int unsigned not null,
name varchar(64) not null,
primary key (help_keyword_id),
unique index (name)
) CHARACTER SET utf8 comment='help keywords';

#
# Create missing time zone related tables
#

CREATE TABLE IF NOT EXISTS time_zone_name (
Name char(64) NOT NULL,   
Time_zone_id int  unsigned NOT NULL,
PRIMARY KEY Name (Name) 
) CHARACTER SET utf8 comment='Time zone names';

CREATE TABLE IF NOT EXISTS time_zone (
Time_zone_id int unsigned NOT NULL auto_increment,
Use_leap_seconds  enum('Y','N') DEFAULT 'N' NOT NULL,
PRIMARY KEY TzId (Time_zone_id) 
) CHARACTER SET utf8 comment='Time zones';

CREATE TABLE IF NOT EXISTS time_zone_transition (
Time_zone_id int unsigned NOT NULL,
Transition_time bigint signed NOT NULL,   
Transition_type_id int unsigned NOT NULL,
PRIMARY KEY TzIdTranTime (Time_zone_id, Transition_time) 
) CHARACTER SET utf8 comment='Time zone transitions';

CREATE TABLE IF NOT EXISTS time_zone_transition_type (
Time_zone_id int unsigned NOT NULL,
Transition_type_id int unsigned NOT NULL,
Offset int signed DEFAULT 0 NOT NULL,
Is_DST tinyint unsigned DEFAULT 0 NOT NULL,
Abbreviation char(8) DEFAULT '' NOT NULL,
PRIMARY KEY TzIdTrTId (Time_zone_id, Transition_type_id) 
) CHARACTER SET utf8 comment='Time zone transition types';

CREATE TABLE IF NOT EXISTS time_zone_leap_second (
Transition_time bigint signed NOT NULL,
Correction int signed NOT NULL,   
PRIMARY KEY TranTime (Transition_time) 
) CHARACTER SET utf8 comment='Leap seconds information for time zones';


#
# Create proc table if it doesn't exists
#

CREATE TABLE IF NOT EXISTS proc (
  db                char(64) binary DEFAULT '' NOT NULL,
  name              char(64) DEFAULT '' NOT NULL,
  type              enum('FUNCTION','PROCEDURE') NOT NULL,
  specific_name     char(64) DEFAULT '' NOT NULL,
  language          enum('SQL') DEFAULT 'SQL' NOT NULL,
  sql_data_access   enum('CONTAINS_SQL',
			 'NO_SQL',
			 'READS_SQL_DATA',
			 'MODIFIES_SQL_DATA'
		    ) DEFAULT 'CONTAINS_SQL' NOT NULL,
  is_deterministic  enum('YES','NO') DEFAULT 'NO' NOT NULL,
  security_type     enum('INVOKER','DEFINER') DEFAULT 'DEFINER' NOT NULL,
  param_list        blob DEFAULT '' NOT NULL,
  returns           char(64) DEFAULT '' NOT NULL,
  body              blob DEFAULT '' NOT NULL,
  definer           char(77) binary DEFAULT '' NOT NULL,
  created           timestamp,
  modified          timestamp,
  sql_mode          set(
                        'REAL_AS_FLOAT',
                        'PIPES_AS_CONCAT',
                        'ANSI_QUOTES',
                        'IGNORE_SPACE',
                        'NOT_USED',
                        'ONLY_FULL_GROUP_BY',
                        'NO_UNSIGNED_SUBTRACTION',
                        'NO_DIR_IN_CREATE',
                        'POSTGRESQL',
                        'ORACLE',
                        'MSSQL',
                        'DB2',
                        'MAXDB',
                        'NO_KEY_OPTIONS',
                        'NO_TABLE_OPTIONS',
                        'NO_FIELD_OPTIONS',
                        'MYSQL323',
                        'MYSQL40',
                        'ANSI',
                        'NO_AUTO_VALUE_ON_ZERO'
                    ) DEFAULT 0 NOT NULL,
  comment           char(64) binary DEFAULT '' NOT NULL,
  PRIMARY KEY (db,name,type)
) comment='Stored Procedures';

# Correct the name fields to not binary, and expand sql_data_access
ALTER TABLE proc MODIFY name char(64) DEFAULT '' NOT NULL,
                 MODIFY specific_name char(64) DEFAULT '' NOT NULL,
		 MODIFY sql_data_access
			enum('CONTAINS_SQL',
			     'NO_SQL',
			     'READS_SQL_DATA',
			     'MODIFIES_SQL_DATA'
			    ) DEFAULT 'CONTAINS_SQL' NOT NULL;
