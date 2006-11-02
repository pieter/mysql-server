/* Copyright (C) 2000-2002 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/* This file includes all reserved words and functions */

#include "lex_symbol.h"

SYM_GROUP sym_group_common= {"", ""};
SYM_GROUP sym_group_geom= {"Spatial extentions", "HAVE_SPATIAL"};
SYM_GROUP sym_group_rtree= {"RTree keys", "HAVE_RTREE_KEYS"};

/* We don't want to include sql_yacc.h into gen_lex_hash */
#ifdef NO_YACC_SYMBOLS
#define SYM_OR_NULL(A) 0
#else
#define SYM_OR_NULL(A) A
#endif

#define SYM(A) SYM_OR_NULL(A),0,&sym_group_common

/*
  Symbols are broken into separated arrays to allow field names with
  same name as functions.
  These are kept sorted for human lookup (the symbols are hashed).
*/

static SYMBOL symbols[] = {
  { "&&",		SYM(AND_AND_SYM)},
  { "<",		SYM(LT)},
  { "<=",		SYM(LE)},
  { "<>",		SYM(NE)},
  { "!=",		SYM(NE)},
  { "=",		SYM(EQ)},
  { ">",		SYM(GT_SYM)},
  { ">=",		SYM(GE)},
  { "<<",		SYM(SHIFT_LEFT)},
  { ">>",		SYM(SHIFT_RIGHT)},
  { "<=>",		SYM(EQUAL_SYM)},
  { "ACCESSIBLE",	SYM(ACCESSIBLE_SYM)},
  { "ACTION",		SYM(ACTION)},
  { "ADD",		SYM(ADD)},
  { "AFTER",		SYM(AFTER_SYM)},
  { "AGAINST",          SYM(AGAINST)},
  { "AGGREGATE",	SYM(AGGREGATE_SYM)},
  { "ALL",		SYM(ALL)},
  { "ALGORITHM",	SYM(ALGORITHM_SYM)},
  { "ALTER",		SYM(ALTER)},
  { "ANALYZE",		SYM(ANALYZE_SYM)},
  { "AND",		SYM(AND_SYM)},
  { "ANY",              SYM(ANY_SYM)},
  { "AS",		SYM(AS)},
  { "ASC",		SYM(ASC)},
  { "ASCII",		SYM(ASCII_SYM)},
  { "ASENSITIVE",       SYM(ASENSITIVE_SYM)},
  { "AT",		SYM(AT_SYM)},
  { "AUTHORS",	        SYM(AUTHORS_SYM)},
  { "AUTO_INCREMENT",	SYM(AUTO_INC)},
  { "AUTOEXTEND_SIZE",	SYM(AUTOEXTEND_SIZE_SYM)},
  { "AVG",		SYM(AVG_SYM)},
  { "AVG_ROW_LENGTH",	SYM(AVG_ROW_LENGTH)},
  { "BACKUP",	        SYM(BACKUP_SYM)},
  { "BEFORE",	        SYM(BEFORE_SYM)},
  { "BEGIN",	        SYM(BEGIN_SYM)},
  { "BETWEEN",		SYM(BETWEEN_SYM)},
  { "BIGINT",		SYM(BIGINT)},
  { "BINARY",		SYM(BINARY)},
  { "BINLOG",		SYM(BINLOG_SYM)},
  { "BIT",		SYM(BIT_SYM)},
  { "BLOB",		SYM(BLOB_SYM)},
  { "BOOL",		SYM(BOOL_SYM)},
  { "BOOLEAN",		SYM(BOOLEAN_SYM)},
  { "BOTH",		SYM(BOTH)},
  { "BTREE",		SYM(BTREE_SYM)},
  { "BY",		SYM(BY)},
  { "BYTE",		SYM(BYTE_SYM)},
  { "CACHE",		SYM(CACHE_SYM)},
  { "CALL",             SYM(CALL_SYM)},
  { "CASCADE",		SYM(CASCADE)},
  { "CASCADED",         SYM(CASCADED)},
  { "CASE",		SYM(CASE_SYM)},
  { "CHAIN",		SYM(CHAIN_SYM)},
  { "CHANGE",		SYM(CHANGE)},
  { "CHANGED",		SYM(CHANGED)},
  { "CHAR",		SYM(CHAR_SYM)},
  { "CHARACTER",	SYM(CHAR_SYM)},
  { "CHARSET",		SYM(CHARSET)},
  { "CHECK",		SYM(CHECK_SYM)},
  { "CHECKSUM",		SYM(CHECKSUM_SYM)},
  { "CIPHER",		SYM(CIPHER_SYM)},
  { "CLIENT",		SYM(CLIENT_SYM)},
  { "CLOSE",		SYM(CLOSE_SYM)},
  { "COALESCE",		SYM(COALESCE)},
  { "CODE",             SYM(CODE_SYM)},
  { "COLLATE",		SYM(COLLATE_SYM)},
  { "COLLATION",	SYM(COLLATION_SYM)},
  { "COLUMN",		SYM(COLUMN_SYM)},
  { "COLUMNS",		SYM(COLUMNS)},
  { "COMMENT",		SYM(COMMENT_SYM)},
  { "COMMIT",		SYM(COMMIT_SYM)},
  { "COMMITTED",	SYM(COMMITTED_SYM)},
  { "COMPACT",		SYM(COMPACT_SYM)},
  { "COMPLETION",	SYM(COMPLETION_SYM)},
  { "COMPRESSED",	SYM(COMPRESSED_SYM)},
  { "CONCURRENT",	SYM(CONCURRENT)},
  { "CONDITION",        SYM(CONDITION_SYM)},
  { "CONNECTION",       SYM(CONNECTION_SYM)},
  { "CONSISTENT",	SYM(CONSISTENT_SYM)},
  { "CONSTRAINT",	SYM(CONSTRAINT)},
  { "CONTAINS",         SYM(CONTAINS_SYM)},
  { "CONTINUE",         SYM(CONTINUE_SYM)},
  { "CONTRIBUTORS",     SYM(CONTRIBUTORS_SYM)},
  { "CONVERT",		SYM(CONVERT_SYM)},
  { "CREATE",		SYM(CREATE)},
  { "CROSS",		SYM(CROSS)},
  { "CUBE",		SYM(CUBE_SYM)},
  { "CURRENT_DATE",	SYM(CURDATE)},
  { "CURRENT_TIME",	SYM(CURTIME)},
  { "CURRENT_TIMESTAMP", SYM(NOW_SYM)},
  { "CURRENT_USER",	SYM(CURRENT_USER)},
  { "CURSOR",           SYM(CURSOR_SYM)},
  { "DATA",		SYM(DATA_SYM)},
  { "DATABASE",		SYM(DATABASE)},
  { "DATABASES",	SYM(DATABASES)},
  { "DATAFILE", 	SYM(DATAFILE_SYM)},
  { "DATE",		SYM(DATE_SYM)},
  { "DATETIME",		SYM(DATETIME)},
  { "DAY",		SYM(DAY_SYM)},
  { "DAY_HOUR",		SYM(DAY_HOUR_SYM)},
  { "DAY_MICROSECOND",	SYM(DAY_MICROSECOND_SYM)},
  { "DAY_MINUTE",	SYM(DAY_MINUTE_SYM)},
  { "DAY_SECOND",	SYM(DAY_SECOND_SYM)},
  { "DEALLOCATE",       SYM(DEALLOCATE_SYM)},     
  { "DEC",		SYM(DECIMAL_SYM)},
  { "DECIMAL",		SYM(DECIMAL_SYM)},
  { "DECLARE",          SYM(DECLARE_SYM)},
  { "DEFAULT",		SYM(DEFAULT)},
  { "DEFINER",          SYM(DEFINER_SYM)},
  { "DELAYED",		SYM(DELAYED_SYM)},
  { "DELAY_KEY_WRITE",	SYM(DELAY_KEY_WRITE_SYM)},
  { "DELETE",		SYM(DELETE_SYM)},
  { "DESC",		SYM(DESC)},
  { "DESCRIBE",		SYM(DESCRIBE)},
  { "DES_KEY_FILE",	SYM(DES_KEY_FILE)},
  { "DETERMINISTIC",    SYM(DETERMINISTIC_SYM)},
  { "DIRECTORY",	SYM(DIRECTORY_SYM)},
  { "DISABLE",		SYM(DISABLE_SYM)},
  { "DISCARD",		SYM(DISCARD)},
  { "DISK",		SYM(DISK_SYM)},
  { "DISTINCT",		SYM(DISTINCT)},
  { "DISTINCTROW",	SYM(DISTINCT)},	/* Access likes this */
  { "DIV",		SYM(DIV_SYM)},
  { "DO",		SYM(DO_SYM)},
  { "DOUBLE",		SYM(DOUBLE_SYM)},
  { "DROP",		SYM(DROP)},
  { "DUAL",		SYM(DUAL_SYM)},
  { "DUMPFILE",		SYM(DUMPFILE)},
  { "DUPLICATE",	SYM(DUPLICATE_SYM)},
  { "DYNAMIC",		SYM(DYNAMIC_SYM)},
  { "EACH",             SYM(EACH_SYM)},
  { "ELSE",             SYM(ELSE)},
  { "ELSEIF",           SYM(ELSEIF_SYM)},
  { "ENABLE",		SYM(ENABLE_SYM)},
  { "ENCLOSED",		SYM(ENCLOSED)},
  { "END",		SYM(END)},
  { "ENDS",		SYM(ENDS_SYM)},
  { "ENGINE",		SYM(ENGINE_SYM)},
  { "ENGINES",		SYM(ENGINES_SYM)},
  { "ENUM",		SYM(ENUM)},
  { "ERRORS",		SYM(ERRORS)},
  { "ESCAPE",		SYM(ESCAPE_SYM)},
  { "ESCAPED",		SYM(ESCAPED)},
  { "EVENT",		SYM(EVENT_SYM)},
  { "EVENTS",		SYM(EVENTS_SYM)},
  { "EVERY",		SYM(EVERY_SYM)},
  { "EXECUTE",		SYM(EXECUTE_SYM)},
  { "EXISTS",		SYM(EXISTS)},
  { "EXIT",             SYM(EXIT_SYM)},
  { "EXPANSION",	SYM(EXPANSION_SYM)},
  { "EXPLAIN",		SYM(DESCRIBE)},
  { "EXTENDED",		SYM(EXTENDED_SYM)},
  { "EXTENT_SIZE",	SYM(EXTENT_SIZE_SYM)},
  { "FALSE",		SYM(FALSE_SYM)},
  { "FAST",		SYM(FAST_SYM)},
  { "FETCH",            SYM(FETCH_SYM)},
  { "FIELDS",		SYM(COLUMNS)},
  { "FILE",		SYM(FILE_SYM)},
  { "FIRST",		SYM(FIRST_SYM)},
  { "FIXED",		SYM(FIXED_SYM)},
  { "FLOAT",		SYM(FLOAT_SYM)},
  { "FLOAT4",		SYM(FLOAT_SYM)},
  { "FLOAT8",		SYM(DOUBLE_SYM)},
  { "FLUSH",		SYM(FLUSH_SYM)},
  { "FOR",		SYM(FOR_SYM)},
  { "FORCE",		SYM(FORCE_SYM)},
  { "FOREIGN",		SYM(FOREIGN)},
  { "FOUND",            SYM(FOUND_SYM)},
  { "FRAC_SECOND",      SYM(FRAC_SECOND_SYM)},
  { "FROM",		SYM(FROM)},
  { "FULL",		SYM(FULL)},
  { "FULLTEXT",		SYM(FULLTEXT_SYM)},
  { "FUNCTION",		SYM(FUNCTION_SYM)},
  { "GEOMETRY",		SYM(GEOMETRY_SYM)},
  { "GEOMETRYCOLLECTION",SYM(GEOMETRYCOLLECTION)},
  { "GET_FORMAT",       SYM(GET_FORMAT)},
  { "GLOBAL",		SYM(GLOBAL_SYM)},
  { "GRANT",		SYM(GRANT)},
  { "GRANTS",	        SYM(GRANTS)},
  { "GROUP",		SYM(GROUP)},
  { "HANDLER",		SYM(HANDLER_SYM)},
  { "HASH",		SYM(HASH_SYM)},
  { "HAVING",		SYM(HAVING)},
  { "HELP",		SYM(HELP_SYM)},
  { "HIGH_PRIORITY",	SYM(HIGH_PRIORITY)},
  { "HOSTS",		SYM(HOSTS_SYM)},
  { "HOUR",		SYM(HOUR_SYM)},
  { "HOUR_MICROSECOND",	SYM(HOUR_MICROSECOND_SYM)},
  { "HOUR_MINUTE",	SYM(HOUR_MINUTE_SYM)},
  { "HOUR_SECOND",	SYM(HOUR_SECOND_SYM)},
  { "IDENTIFIED",	SYM(IDENTIFIED_SYM)},
  { "IF",		SYM(IF)},
  { "IGNORE",		SYM(IGNORE_SYM)},
  { "IMPORT",		SYM(IMPORT)},
  { "IN",		SYM(IN_SYM)},
  { "INDEX",		SYM(INDEX_SYM)},
  { "INDEXES",		SYM(INDEXES)},
  { "INFILE",		SYM(INFILE)},
  { "INITIAL_SIZE",	SYM(INITIAL_SIZE_SYM)},
  { "INNER",		SYM(INNER_SYM)},
  { "INNOBASE",		SYM(INNOBASE_SYM)},
  { "INNODB",		SYM(INNOBASE_SYM)},
  { "INOUT",            SYM(INOUT_SYM)},
  { "INSENSITIVE",      SYM(INSENSITIVE_SYM)},
  { "INSERT",		SYM(INSERT)},
  { "INSERT_METHOD",    SYM(INSERT_METHOD)},
  { "INSTALL",          SYM(INSTALL_SYM)},
  { "INT",		SYM(INT_SYM)},
  { "INT1",		SYM(TINYINT)},
  { "INT2",		SYM(SMALLINT)},
  { "INT3",		SYM(MEDIUMINT)},
  { "INT4",		SYM(INT_SYM)},
  { "INT8",		SYM(BIGINT)},
  { "INTEGER",		SYM(INT_SYM)},
  { "INTERVAL",		SYM(INTERVAL_SYM)},
  { "INTO",		SYM(INTO)},
  { "IO_THREAD",        SYM(RELAY_THREAD)},
  { "IS",		SYM(IS)},
  { "ISOLATION",	SYM(ISOLATION)},
  { "ISSUER",		SYM(ISSUER_SYM)},
  { "ITERATE",          SYM(ITERATE_SYM)},
  { "INVOKER",          SYM(INVOKER_SYM)},
  { "JOIN",		SYM(JOIN_SYM)},
  { "KEY",		SYM(KEY_SYM)},
  { "KEYS",		SYM(KEYS)},
  { "KEY_BLOCK_SIZE",	SYM(KEY_BLOCK_SIZE)},
  { "KILL",		SYM(KILL_SYM)},
  { "LANGUAGE",         SYM(LANGUAGE_SYM)},
  { "LAST",		SYM(LAST_SYM)},
  { "LEADING",		SYM(LEADING)},
  { "LEAVE",            SYM(LEAVE_SYM)},
  { "LEAVES",		SYM(LEAVES)},
  { "LEFT",		SYM(LEFT)},
  { "LESS",             SYM(LESS_SYM)},
  { "LEVEL",		SYM(LEVEL_SYM)},
  { "LIKE",		SYM(LIKE)},
  { "LIMIT",		SYM(LIMIT)},
  { "LINEAR",		SYM(LINEAR_SYM)},
  { "LINES",		SYM(LINES)},
  { "LINESTRING",	SYM(LINESTRING)},
  { "LIST",             SYM(LIST_SYM)},
  { "LOAD",		SYM(LOAD)},
  { "LOCAL",		SYM(LOCAL_SYM)},
  { "LOCALTIME",	SYM(NOW_SYM)},
  { "LOCALTIMESTAMP",	SYM(NOW_SYM)},
  { "LOCK",		SYM(LOCK_SYM)},
  { "LOCKS",		SYM(LOCKS_SYM)},
  { "LOGFILE",		SYM(LOGFILE_SYM)},
  { "LOGS",		SYM(LOGS_SYM)},
  { "LONG",		SYM(LONG_SYM)},
  { "LONGBLOB",		SYM(LONGBLOB)},
  { "LONGTEXT",		SYM(LONGTEXT)},
  { "LOOP",             SYM(LOOP_SYM)},
  { "LOW_PRIORITY",	SYM(LOW_PRIORITY)},
  { "MASTER",           SYM(MASTER_SYM)},
  { "MASTER_CONNECT_RETRY",           SYM(MASTER_CONNECT_RETRY_SYM)},
  { "MASTER_HOST",           SYM(MASTER_HOST_SYM)},
  { "MASTER_LOG_FILE",           SYM(MASTER_LOG_FILE_SYM)},
  { "MASTER_LOG_POS",           SYM(MASTER_LOG_POS_SYM)},
  { "MASTER_PASSWORD",           SYM(MASTER_PASSWORD_SYM)},
  { "MASTER_PORT",           SYM(MASTER_PORT_SYM)},
  { "MASTER_SERVER_ID",           SYM(MASTER_SERVER_ID_SYM)},
  { "MASTER_SSL",       SYM(MASTER_SSL_SYM)},
  { "MASTER_SSL_CA",    SYM(MASTER_SSL_CA_SYM)},
  { "MASTER_SSL_CAPATH",SYM(MASTER_SSL_CAPATH_SYM)},
  { "MASTER_SSL_CERT",  SYM(MASTER_SSL_CERT_SYM)},
  { "MASTER_SSL_CIPHER",SYM(MASTER_SSL_CIPHER_SYM)},
  { "MASTER_SSL_KEY",   SYM(MASTER_SSL_KEY_SYM)},
  { "MASTER_USER",           SYM(MASTER_USER_SYM)},
  { "MATCH",		SYM(MATCH)},
  { "MAX_CONNECTIONS_PER_HOUR", SYM(MAX_CONNECTIONS_PER_HOUR)},
  { "MAX_QUERIES_PER_HOUR", SYM(MAX_QUERIES_PER_HOUR)},
  { "MAX_ROWS",		SYM(MAX_ROWS)},
  { "MAX_SIZE",		SYM(MAX_SIZE_SYM)},
  { "MAX_UPDATES_PER_HOUR", SYM(MAX_UPDATES_PER_HOUR)},
  { "MAX_USER_CONNECTIONS", SYM(MAX_USER_CONNECTIONS_SYM)},
  { "MAXVALUE",         SYM(MAX_VALUE_SYM)},
  { "MEDIUM",		SYM(MEDIUM_SYM)},
  { "MEDIUMBLOB",	SYM(MEDIUMBLOB)},
  { "MEDIUMINT",	SYM(MEDIUMINT)},
  { "MEDIUMTEXT",	SYM(MEDIUMTEXT)},
  { "MEMORY",		SYM(MEMORY_SYM)},
  { "MERGE",		SYM(MERGE_SYM)},
  { "MICROSECOND",	SYM(MICROSECOND_SYM)},
  { "MIDDLEINT",	SYM(MEDIUMINT)},	/* For powerbuilder */
  { "MIGRATE",          SYM(MIGRATE_SYM)},
  { "MINUTE",		SYM(MINUTE_SYM)},
  { "MINUTE_MICROSECOND", SYM(MINUTE_MICROSECOND_SYM)},
  { "MINUTE_SECOND",	SYM(MINUTE_SECOND_SYM)},
  { "MIN_ROWS",		SYM(MIN_ROWS)},
  { "MOD",		SYM(MOD_SYM)},
  { "MODE",		SYM(MODE_SYM)},
  { "MODIFIES",		SYM(MODIFIES_SYM)},
  { "MODIFY",		SYM(MODIFY_SYM)},
  { "MONTH",		SYM(MONTH_SYM)},
  { "MULTILINESTRING",	SYM(MULTILINESTRING)},
  { "MULTIPOINT",	SYM(MULTIPOINT)},
  { "MULTIPOLYGON",	SYM(MULTIPOLYGON)},
  { "MUTEX",  SYM(MUTEX_SYM)},
  { "NAME",             SYM(NAME_SYM)},
  { "NAMES",		SYM(NAMES_SYM)},
  { "NATIONAL",		SYM(NATIONAL_SYM)},
  { "NATURAL",		SYM(NATURAL)},
  { "NDB",              SYM(NDBCLUSTER_SYM)},
  { "NDBCLUSTER",       SYM(NDBCLUSTER_SYM)},
  { "NCHAR",		SYM(NCHAR_SYM)},
  { "NEW",              SYM(NEW_SYM)},
  { "NEXT",		SYM(NEXT_SYM)},
  { "NO",		SYM(NO_SYM)},
  { "NO_WAIT",		SYM(NO_WAIT_SYM)},
  { "NODEGROUP",	SYM(NODEGROUP_SYM)},
  { "NONE",		SYM(NONE_SYM)},
  { "NOT",		SYM(NOT_SYM)},
  { "NO_WRITE_TO_BINLOG",  SYM(NO_WRITE_TO_BINLOG)},
  { "NULL",		SYM(NULL_SYM)},
  { "NUMERIC",		SYM(NUMERIC_SYM)},
  { "NVARCHAR",		SYM(NVARCHAR_SYM)},
  { "OFFSET",		SYM(OFFSET_SYM)},
  { "OLD_PASSWORD",	SYM(OLD_PASSWORD)},
  { "ON",		SYM(ON)},
  { "ONE",              SYM(ONE_SYM)},
  { "ONE_SHOT",		SYM(ONE_SHOT_SYM)},
  { "OPEN",		SYM(OPEN_SYM)},
  { "OPTIMIZE",		SYM(OPTIMIZE)},
  { "OPTION",		SYM(OPTION)},
  { "OPTIONALLY",	SYM(OPTIONALLY)},
  { "OR",		SYM(OR_SYM)},
  { "ORDER",		SYM(ORDER_SYM)},
  { "OUT",              SYM(OUT_SYM)},
  { "OUTER",		SYM(OUTER)},
  { "OUTFILE",		SYM(OUTFILE)},
  { "PACK_KEYS",	SYM(PACK_KEYS_SYM)},
  { "PARSER",           SYM(PARSER_SYM)},
  { "PARTIAL",		SYM(PARTIAL)},
#ifdef WITH_PARTITION_STORAGE_ENGINE
  { "PARTITION",        SYM(PARTITION_SYM)},
  { "PARTITIONING",     SYM(PARTITIONING_SYM)},
  { "PARTITIONS",       SYM(PARTITIONS_SYM)},
#endif
  { "PASSWORD",		SYM(PASSWORD)},
  { "PHASE",            SYM(PHASE_SYM)},
  { "PLUGIN",           SYM(PLUGIN_SYM)},
  { "PLUGINS",          SYM(PLUGINS_SYM)},
  { "POINT",		SYM(POINT_SYM)},
  { "POLYGON",		SYM(POLYGON)},
  { "PRECISION",	SYM(PRECISION)},
  { "PREPARE",          SYM(PREPARE_SYM)},
  { "PRESERVE",		SYM(PRESERVE_SYM)},
  { "PREV",		SYM(PREV_SYM)},
  { "PRIMARY",		SYM(PRIMARY_SYM)},
  { "PRIVILEGES",	SYM(PRIVILEGES)},
  { "PROCEDURE",	SYM(PROCEDURE)},
  { "PROCESS"	,	SYM(PROCESS)},
  { "PROCESSLIST",	SYM(PROCESSLIST_SYM)},
  { "PURGE",		SYM(PURGE)},
  { "QUARTER",          SYM(QUARTER_SYM)},
  { "QUERY",		SYM(QUERY_SYM)},
  { "QUICK",	        SYM(QUICK)},
  { "RANGE",            SYM(RANGE_SYM)},
  { "READ",		SYM(READ_SYM)},
  { "READ_ONLY",	SYM(READ_ONLY_SYM)},
  { "READ_WRITE",	SYM(READ_WRITE_SYM)},
  { "READS",		SYM(READS_SYM)},
  { "REAL",		SYM(REAL)},
  { "REBUILD",		SYM(REBUILD_SYM)},
  { "RECOVER",          SYM(RECOVER_SYM)},
  { "REDO_BUFFER_SIZE",	SYM(REDO_BUFFER_SIZE_SYM)},
  { "REDOFILE",         SYM(REDOFILE_SYM)},
  { "REDUNDANT",	SYM(REDUNDANT_SYM)},
  { "REFERENCES",	SYM(REFERENCES)},
  { "REGEXP",		SYM(REGEXP)},
  { "RELAY_LOG_FILE",   SYM(RELAY_LOG_FILE_SYM)},
  { "RELAY_LOG_POS",    SYM(RELAY_LOG_POS_SYM)},
  { "RELAY_THREAD",     SYM(RELAY_THREAD)},
  { "RELEASE",		SYM(RELEASE_SYM)},
  { "RELOAD",		SYM(RELOAD)},
  { "REMOVE",		SYM(REMOVE_SYM)},
  { "RENAME",		SYM(RENAME)},
  { "REORGANIZE",	SYM(REORGANIZE_SYM)},
  { "REPAIR",		SYM(REPAIR)},
  { "REPEATABLE",	SYM(REPEATABLE_SYM)},
  { "REPLACE",		SYM(REPLACE)},
  { "REPLICATION",	SYM(REPLICATION)},
  { "REPEAT",           SYM(REPEAT_SYM)},
  { "REQUIRE",	        SYM(REQUIRE_SYM)},
  { "RESET",		SYM(RESET_SYM)},
  { "RESTORE",		SYM(RESTORE_SYM)},
  { "RESTRICT",		SYM(RESTRICT)},
  { "RESUME",           SYM(RESUME_SYM)},
  { "RETURN",           SYM(RETURN_SYM)},
  { "RETURNS",		SYM(RETURNS_SYM)},
  { "REVOKE",		SYM(REVOKE)},
  { "RIGHT",		SYM(RIGHT)},
  { "RLIKE",		SYM(REGEXP)},	/* Like in mSQL2 */
  { "ROLLBACK",		SYM(ROLLBACK_SYM)},
  { "ROLLUP",		SYM(ROLLUP_SYM)},
  { "ROUTINE",		SYM(ROUTINE_SYM)},
  { "ROW",		SYM(ROW_SYM)},
  { "ROWS",		SYM(ROWS_SYM)},
  { "ROW_FORMAT",	SYM(ROW_FORMAT_SYM)},
  { "RTREE",		SYM(RTREE_SYM)},
  { "SAVEPOINT",	SYM(SAVEPOINT_SYM)},
  { "SCHEDULE",		SYM(SCHEDULE_SYM)},
  { "SCHEMA",		SYM(DATABASE)},
  { "SCHEMAS",          SYM(DATABASES)},
  { "SECOND",		SYM(SECOND_SYM)},
  { "SECOND_MICROSECOND", SYM(SECOND_MICROSECOND_SYM)},
  { "SECURITY",         SYM(SECURITY_SYM)},
  { "SELECT",		SYM(SELECT_SYM)},
  { "SENSITIVE",        SYM(SENSITIVE_SYM)},
  { "SEPARATOR",	SYM(SEPARATOR_SYM)},
  { "SERIAL",		SYM(SERIAL_SYM)},
  { "SERIALIZABLE",	SYM(SERIALIZABLE_SYM)},
  { "SESSION",		SYM(SESSION_SYM)},
  { "SET",		SYM(SET)},
  { "SHARE",		SYM(SHARE_SYM)},
  { "SHOW",		SYM(SHOW)},
  { "SHUTDOWN",		SYM(SHUTDOWN)},
  { "SIGNED",		SYM(SIGNED_SYM)},
  { "SIMPLE",		SYM(SIMPLE_SYM)},
  { "SLAVE",            SYM(SLAVE)},
  { "SNAPSHOT",         SYM(SNAPSHOT_SYM)},
  { "SMALLINT",		SYM(SMALLINT)},
  { "SOME",             SYM(ANY_SYM)},
  { "SONAME",		SYM(SONAME_SYM)},
  { "SOUNDS",		SYM(SOUNDS_SYM)},
  { "SPATIAL",		SYM(SPATIAL_SYM)},
  { "SPECIFIC",         SYM(SPECIFIC_SYM)},
  { "SQL",              SYM(SQL_SYM)},
  { "SQLEXCEPTION",     SYM(SQLEXCEPTION_SYM)},
  { "SQLSTATE",         SYM(SQLSTATE_SYM)},
  { "SQLWARNING",       SYM(SQLWARNING_SYM)},
  { "SQL_BIG_RESULT",	SYM(SQL_BIG_RESULT)},
  { "SQL_BUFFER_RESULT", SYM(SQL_BUFFER_RESULT)},
  { "SQL_CACHE",        SYM(SQL_CACHE_SYM)},
  { "SQL_CALC_FOUND_ROWS", SYM(SQL_CALC_FOUND_ROWS)},
  { "SQL_NO_CACHE",	SYM(SQL_NO_CACHE_SYM)},
  { "SQL_SMALL_RESULT", SYM(SQL_SMALL_RESULT)},
  { "SQL_THREAD",	SYM(SQL_THREAD)},
  { "SQL_TSI_FRAC_SECOND", SYM(FRAC_SECOND_SYM)},
  { "SQL_TSI_SECOND",   SYM(SECOND_SYM)},
  { "SQL_TSI_MINUTE",   SYM(MINUTE_SYM)},
  { "SQL_TSI_HOUR",     SYM(HOUR_SYM)},
  { "SQL_TSI_DAY",      SYM(DAY_SYM)},
  { "SQL_TSI_WEEK",     SYM(WEEK_SYM)},
  { "SQL_TSI_MONTH",    SYM(MONTH_SYM)},
  { "SQL_TSI_QUARTER",  SYM(QUARTER_SYM)},
  { "SQL_TSI_YEAR",     SYM(YEAR_SYM)},
  { "SSL",		SYM(SSL_SYM)},
  { "START",		SYM(START_SYM)},
  { "STARTING",		SYM(STARTING)},
  { "STARTS",		SYM(STARTS_SYM)},
  { "STATUS",		SYM(STATUS_SYM)},
  { "STOP",		SYM(STOP_SYM)},
  { "STORAGE",		SYM(STORAGE_SYM)},
  { "STRAIGHT_JOIN",	SYM(STRAIGHT_JOIN)},
  { "STRING",		SYM(STRING_SYM)},
  { "SUBJECT",		SYM(SUBJECT_SYM)},
  { "SUBPARTITION",     SYM(SUBPARTITION_SYM)},
  { "SUBPARTITIONS",    SYM(SUBPARTITIONS_SYM)},
  { "SUPER",		SYM(SUPER_SYM)},
  { "SUSPEND",          SYM(SUSPEND_SYM)},
  { "TABLE",		SYM(TABLE_SYM)},
  { "TABLES",		SYM(TABLES)},
  { "TABLESPACE",	SYM(TABLESPACE)},
  { "TEMPORARY",	SYM(TEMPORARY)},
  { "TEMPTABLE",	SYM(TEMPTABLE_SYM)},
  { "TERMINATED",	SYM(TERMINATED)},
  { "TEXT",		SYM(TEXT_SYM)},
  { "THAN",             SYM(THAN_SYM)},
  { "THEN",		SYM(THEN_SYM)},
  { "TIME",		SYM(TIME_SYM)},
  { "TIMESTAMP",	SYM(TIMESTAMP)},
  { "TIMESTAMPADD",     SYM(TIMESTAMP_ADD)},
  { "TIMESTAMPDIFF",    SYM(TIMESTAMP_DIFF)},
  { "TINYBLOB",		SYM(TINYBLOB)},
  { "TINYINT",		SYM(TINYINT)},
  { "TINYTEXT",		SYM(TINYTEXT)},
  { "TO",		SYM(TO_SYM)},
  { "TRAILING",		SYM(TRAILING)},
  { "TRANSACTION",	SYM(TRANSACTION_SYM)},
  { "TRIGGER",          SYM(TRIGGER_SYM)},
  { "TRIGGERS",         SYM(TRIGGERS_SYM)},
  { "TRUE",		SYM(TRUE_SYM)},
  { "TRUNCATE",		SYM(TRUNCATE_SYM)},
  { "TYPE",		SYM(TYPE_SYM)},
  { "TYPES",		SYM(TYPES_SYM)},
  { "UNCOMMITTED",	SYM(UNCOMMITTED_SYM)},
  { "UNDEFINED",	SYM(UNDEFINED_SYM)},
  { "UNDO_BUFFER_SIZE",	SYM(UNDO_BUFFER_SIZE_SYM)},
  { "UNDOFILE", 	SYM(UNDOFILE_SYM)},
  { "UNDO",             SYM(UNDO_SYM)},
  { "UNICODE",	        SYM(UNICODE_SYM)},
  { "UNION",	        SYM(UNION_SYM)},
  { "UNIQUE",		SYM(UNIQUE_SYM)},
  { "UNKNOWN",		SYM(UNKNOWN_SYM)},
  { "UNLOCK",		SYM(UNLOCK_SYM)},
  { "UNINSTALL",        SYM(UNINSTALL_SYM)},
  { "UNSIGNED",		SYM(UNSIGNED)},
  { "UNTIL",		SYM(UNTIL_SYM)},
  { "UPDATE",		SYM(UPDATE_SYM)},
  { "UPGRADE",          SYM(UPGRADE_SYM)},
  { "USAGE",		SYM(USAGE)},
  { "USE",		SYM(USE_SYM)},
  { "USER",		SYM(USER)},
  { "USER_RESOURCES",	SYM(RESOURCES)},
  { "USE_FRM",		SYM(USE_FRM)},
  { "USING",		SYM(USING)},
  { "UTC_DATE",         SYM(UTC_DATE_SYM)},
  { "UTC_TIME",         SYM(UTC_TIME_SYM)},
  { "UTC_TIMESTAMP",    SYM(UTC_TIMESTAMP_SYM)},
  { "VALUE",		SYM(VALUE_SYM)},
  { "VALUES",		SYM(VALUES)},
  { "VARBINARY",	SYM(VARBINARY)},
  { "VARCHAR",		SYM(VARCHAR)},
  { "VARCHARACTER",	SYM(VARCHAR)},
  { "VARIABLES",	SYM(VARIABLES)},
  { "VARYING",		SYM(VARYING)},
  { "WAIT",		SYM(WAIT_SYM)},
  { "WARNINGS",		SYM(WARNINGS)},
  { "WEEK",		SYM(WEEK_SYM)},
  { "WHEN",		SYM(WHEN_SYM)},
  { "WHERE",		SYM(WHERE)},
  { "WHILE",            SYM(WHILE_SYM)},
  { "VIEW",		SYM(VIEW_SYM)},
  { "WITH",		SYM(WITH)},
  { "WORK",		SYM(WORK_SYM)},
  { "WRITE",		SYM(WRITE_SYM)},
  { "X509",		SYM(X509_SYM)},
  { "XOR",		SYM(XOR)},
  { "XA",               SYM(XA_SYM)},
  { "YEAR",		SYM(YEAR_SYM)},
  { "YEAR_MONTH",	SYM(YEAR_MONTH_SYM)},
  { "ZEROFILL",		SYM(ZEROFILL)},
  { "||",		SYM(OR_OR_SYM)}
};


static SYMBOL sql_functions[] = {
  { "ADDDATE",		SYM(ADDDATE_SYM)},
  { "BIT_AND",		SYM(BIT_AND)},
  { "BIT_OR",		SYM(BIT_OR)},
  { "BIT_XOR",		SYM(BIT_XOR)},
  { "CAST",		SYM(CAST_SYM)},
  { "COUNT",		SYM(COUNT_SYM)},
  { "CURDATE",		SYM(CURDATE)},
  { "CURTIME",		SYM(CURTIME)},
  { "DATE_ADD",		SYM(DATE_ADD_INTERVAL)},
  { "DATE_SUB",		SYM(DATE_SUB_INTERVAL)},
  { "EXTRACT",		SYM(EXTRACT_SYM)},
  { "GROUP_CONCAT",	SYM(GROUP_CONCAT_SYM)},
  { "GROUP_UNIQUE_USERS",	SYM(GROUP_UNIQUE_USERS)},
  { "MAX",		SYM(MAX_SYM)},
  { "MID",		SYM(SUBSTRING)},	/* unireg function */
  { "MIN",		SYM(MIN_SYM)},
  { "NOW",		SYM(NOW_SYM)},
  { "POSITION",		SYM(POSITION_SYM)},
  { "SESSION_USER",     SYM(USER)},
  { "STD",		SYM(STD_SYM)},
  { "STDDEV",		SYM(STD_SYM)},
  { "STDDEV_POP",	SYM(STD_SYM)},
  { "STDDEV_SAMP",	SYM(STDDEV_SAMP_SYM)},
  { "SUBDATE",		SYM(SUBDATE_SYM)},
  { "SUBSTR",		SYM(SUBSTRING)},
  { "SUBSTRING",	SYM(SUBSTRING)},
  { "SUM",		SYM(SUM_SYM)},
  { "SYSDATE",		SYM(SYSDATE)},
  { "SYSTEM_USER",      SYM(USER)},
  { "TRIM",		SYM(TRIM)},
  { "UNIQUE_USERS",	SYM(UNIQUE_USERS)},
  { "VARIANCE",		SYM(VARIANCE_SYM)},
  { "VAR_POP",		SYM(VARIANCE_SYM)},
  { "VAR_SAMP",		SYM(VAR_SAMP_SYM)},
};
