-- This script converts any old privilege tables to privilege tables suitable
-- for MySQL 4.0.

-- You can safely ignore all 'Duplicate column' and 'Unknown column' errors"
-- as this just means that your tables where already up to date.
-- This script is safe to run even if your tables are already up to date!

-- On unix, you should use the mysql_fix_privilege_tables script to execute
-- this sql script.
-- On windows you should do 'mysql --force mysql < mysql_fix_privilege_tables.sql'

USE mysql;
ALTER TABLE user type=MyISAM;
ALTER TABLE db type=MyISAM;
ALTER TABLE host type=MyISAM;
ALTER TABLE func type=MyISAM;
ALTER TABLE columns_priv type=MyISAM;
ALTER TABLE tables_priv type=MyISAM;
ALTER TABLE user change Password Password char(41) not null;
ALTER TABLE user add File_priv enum('N','Y') NOT NULL;
CREATE TABLE IF NOT EXISTS func (
  name char(64) binary DEFAULT '' NOT NULL,
  ret tinyint(1) DEFAULT '0' NOT NULL,
  dl char(128) DEFAULT '' NOT NULL,
  type enum ('function','aggregate') NOT NULL,
  PRIMARY KEY (name)
);

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
);

CREATE TABLE IF NOT EXISTS columns_priv (
  Host char(60) DEFAULT '' NOT NULL,
  Db char(60) DEFAULT '' NOT NULL,
  User char(16) DEFAULT '' NOT NULL,
  Table_name char(60) DEFAULT '' NOT NULL,
  Column_name char(59) DEFAULT '' NOT NULL,
  Timestamp timestamp(14),
  Column_priv set('Select','Insert','Update','References') DEFAULT '' NOT NULL,
  PRIMARY KEY (Host,Db,User,Table_name,Column_name)
);


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
ADD max_questions int(11) NOT NULL AFTER x509_subject,
ADD max_updates   int(11) unsigned NOT NULL AFTER max_questions,
ADD max_connections int(11) unsigned NOT NULL AFTER max_updates;


--
--  Add Create_tmp_table_priv and Lock_tables_priv to db and host
--

ALTER TABLE db
ADD Create_tmp_table_priv enum('N','Y') DEFAULT 'N' NOT NULL,
ADD Lock_tables_priv enum('N','Y') DEFAULT 'N' NOT NULL;
ALTER TABLE host
ADD Create_tmp_table_priv enum('N','Y') DEFAULT 'N' NOT NULL,
ADD Lock_tables_priv enum('N','Y') DEFAULT 'N' NOT NULL;

alter table db change Db Db char(64) binary DEFAULT '' NOT NULL;
alter table host change Db Db char(64) binary DEFAULT '' NOT NULL;
alter table user change password Password char(16) binary NOT NULL, change max_questions max_questions int(11) unsigned DEFAULT 0  NOT NULL;
alter table tables_priv change Db Db char(64) binary DEFAULT '' NOT NULL, change Host Host char(60) binary DEFAULT '' NOT NULL, change User User char(16) binary DEFAULT '' NOT NULL, change Table_name Table_name char(64) binary DEFAULT '' NOT NULL;
alter table tables_priv add KEY Grantor (Grantor);
alter table columns_priv change Db Db char(64) binary DEFAULT '' NOT NULL, change Host Host char(60) binary DEFAULT '' NOT NULL, change User User char(16) binary DEFAULT '' NOT NULL, change Table_name Table_name char(64) binary DEFAULT '' NOT NULL, change Column_name Column_name char(64) binary DEFAULT '' NOT NULL;

alter table db comment='Database privileges';
alter table host comment='Host privileges;  Merged with database privileges';
alter table user comment='Users and global privileges';
alter table func comment='User defined functions';
alter table tables_priv comment='Table privileges';
alter table columns_priv comment='Column privileges';
