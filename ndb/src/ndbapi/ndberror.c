/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include <ndb_global.h>
#include <ndberror.h>
#include <m_string.h>

typedef struct ErrorBundle {
  int code;
  ndberror_classification classification;
  const char * message;
} ErrorBundle;

/**
 * Shorter names in table below
 */

#define ST_S ndberror_st_success
#define ST_P ndberror_st_permanent
#define ST_T ndberror_st_temporary
#define ST_U ndberror_st_unknown

#define NE ndberror_cl_none
#define AE ndberror_cl_application
#define CE ndberror_cl_configuration
#define ND ndberror_cl_no_data_found
#define CV ndberror_cl_constraint_violation
#define SE ndberror_cl_schema_error
#define UD ndberror_cl_user_defined

#define IS ndberror_cl_insufficient_space
#define TR ndberror_cl_temporary_resource
#define NR ndberror_cl_node_recovery
#define OL ndberror_cl_overload
#define TO ndberror_cl_timeout_expired
#define NS ndberror_cl_node_shutdown

#define UR ndberror_cl_unknown_result

#define IE ndberror_cl_internal_error
#define NI ndberror_cl_function_not_implemented
#define UE ndberror_cl_unknown_error_code

#define OE ndberror_cl_schema_object_already_exists

static const char* empty_string = "";

/*
 * Error code ranges are reserved for respective block
 *
 *  200 - TC
 *  300 - DIH
 *  400 - LQH
 *  600 - ACC
 *  700 - DICT
 *  800 - TUP
 *  900 - TUX
 * 1200 - LQH
 * 1300 - BACKUP
 * 4000 - API
 * 4100 - ""
 * 4200 - ""
 * 4300 - ""
 * 4400 - ""
 * 4500 - ""
 * 4600 - ""
 * 4700 - "" Event
 * 5000 - Management server
 */

static
const 
ErrorBundle ErrorCodes[] = {
  /**
   * No error
   */
  { 0,    NE, "No error" },
  
  /**
   * NoDataFound
   */
  { 626,  ND, "Tuple did not exist" },

  /**
   * ConstraintViolation 
   */
  { 630,  CV, "Tuple already existed when attempting to insert" },
  { 840,  CV, "Trying to set a NOT NULL attribute to NULL" },
  { 893,  CV, "Constraint violation e.g. duplicate value in unique index" },

  /**
   * Node recovery errors
   */
  {  286, NR, "Node failure caused abort of transaction" }, 
  {  250, NR, "Node where lock was held crashed, restart scan transaction" },
  {  499, NR, "Scan take over error, restart scan transaction" },  
  { 1204, NR, "Temporary failure, distribution changed" },
  { 4002, NR, "Send to NDB failed" },
  { 4010, NR, "Node failure caused abort of transaction" }, 
  { 4025, NR, "Node failure caused abort of transaction" }, 
  { 4027, NR, "Node failure caused abort of transaction" },
  { 4028, NR, "Node failure caused abort of transaction" },
  { 4029, NR, "Node failure caused abort of transaction" },
  { 4031, NR, "Node failure caused abort of transaction" },
  { 4033, NR, "Send to NDB failed" },
  { 4115, NR, 
    "Transaction was committed but all read information was not "
    "received due to node crash" },
  { 4119, NR, "Simple/dirty read failed due to node failure" },
  
  /**
   * Node shutdown
   */
  {  280, NS, "Transaction aborted due to node shutdown" },
  /* This scan trans had an active fragment scan in a LQH which have crashed */
  {  270, NS, "Transaction aborted due to node shutdown" }, 
  { 1223, NS, "Read operation aborted due to node shutdown" },
  { 4023, NS, "Transaction aborted due to node shutdown" },
  { 4030, NS, "Transaction aborted due to node shutdown" },
  { 4034, NS, "Transaction aborted due to node shutdown" },


  
  /**
   * Unknown result
   */
  { 4007, UR, "Send to ndbd node failed" },
  { 4008, UR, "Receive from NDB failed" },
  { 4009, UR, "Cluster Failure" },
  { 4012, UR, 
    "Request ndbd time-out, maybe due to high load or communication problems"}, 
  { 4013, UR, "Request timed out in waiting for node failure"}, 
  { 4024, UR, 
    "Time-out, most likely caused by simple read or cluster failure" }, 
  
  /**
   * TemporaryResourceError
   */
  { 217,  TR, "217" },
  { 218,  TR, "218" },
  { 219,  TR, "219" },
  { 233,  TR,
    "Out of operation records in transaction coordinator (increase MaxNoOfConcurrentOperations)" },
  { 275,  TR, "275" },
  { 279,  TR, "Out of transaction markers in transaction coordinator" },
  { 414,  TR, "414" },
  { 418,  TR, "Out of transaction buffers in LQH" },
  { 419,  TR, "419" },
  { 245,  TR, "Too many active scans" },
  { 488,  TR, "Too many active scans" },
  { 490,  TR, "Too many active scans" },
  { 805,  TR, "Out of attrinfo records in tuple manager" },
  { 830,  TR, "Out of add fragment operation records" },
  { 873,  TR, "Out of attrinfo records for scan in tuple manager" },
  { 1217, TR, "Out of operation records in local data manager (increase MaxNoOfLocalOperations)" },
  { 1220, TR, "REDO log files overloaded, consult online manual (decrease TimeBetweenLocalCheckpoints, and|or increase NoOfFragmentLogFiles)" },
  { 1222, TR, "Out of transaction markers in LQH" },
  { 1224, TR, "Out of Send Buffer space in LQH" },
  { 4021, TR, "Out of Send Buffer space in NDB API" },
  { 4022, TR, "Out of Send Buffer space in NDB API" },
  { 4032, TR, "Out of Send Buffer space in NDB API" },
  {  288, TR, "Out of index operations in transaction coordinator (increase MaxNoOfConcurrentIndexOperations)" },
  {  289, TR, "Out of transaction buffer memory in TC (increase TransactionBufferMemory)" },

  /**
   * InsufficientSpace
   */
  { 623,  IS, "623" },
  { 624,  IS, "624" },
  { 625,  IS, "Out of memory in Ndb Kernel, hash index part (increase IndexMemory)" },
  { 640,  IS, "Too many hash indexes (should not happen)" },
  { 826,  IS, "Too many tables and attributes (increase MaxNoOfAttributes or MaxNoOfTables)" },
  { 827,  IS, "Out of memory in Ndb Kernel, table data (increase DataMemory)" },
  { 902,  IS, "Out of memory in Ndb Kernel, ordered index data (increase DataMemory)" },
  { 903,  IS, "Too many ordered indexes (increase MaxNoOfOrderedIndexes)" },
  { 904,  IS, "Out of fragment records (increase MaxNoOfOrderedIndexes)" },
  { 905,  IS, "Out of attribute records (increase MaxNoOfAttributes)" },

  /**
   * TimeoutExpired 
   */
  { 266,  TO, "Time-out in NDB, probably caused by deadlock" },
  { 274,  TO, "Time-out in NDB, probably caused by deadlock" }, /* Scan trans timeout */
  { 296,  TO, "Time-out in NDB, probably caused by deadlock" }, /* Scan trans timeout */
  { 297,  TO, "Time-out in NDB, probably caused by deadlock" }, /* Scan trans timeout, temporary!! */
  { 237,  TO, "Transaction had timed out when trying to commit it" },
  
  /**
   * OverloadError
   */
  { 410,  OL, "REDO log files overloaded, consult online manual (decrease TimeBetweenLocalCheckpoints, and|or increase NoOfFragmentLogFiles)" },
  { 677,  OL, "Index UNDO buffers overloaded (increase UndoIndexBuffer)" },
  { 891,  OL, "Data UNDO buffers overloaded (increase UndoDataBuffer)" },
  { 1221, OL, "REDO buffers overloaded, consult online manual (increase RedoBuffer)" },
  { 4006, OL, "Connect failure - out of connection objects (increase MaxNoOfConcurrentTransactions)" }, 


  
  /**
   * Internal errors
   */
  { 896,  IE, "Tuple corrupted - wrong checksum or column data in invalid format" },
  { 901,  IE, "Inconsistent ordered index. The index needs to be dropped and recreated" },
  { 202,  IE, "202" },
  { 203,  IE, "203" },
  { 207,  IE, "207" },
  { 208,  IE, "208" },
  { 209,  IE, "Communication problem, signal error" },
  { 220,  IE, "220" },
  { 230,  IE, "230" },
  { 232,  IE, "232" },
  { 238,  IE, "238" },
  { 271,  IE, "Simple Read transaction without any attributes to read" },
  { 272,  IE, "Update operation without any attributes to update" },
  { 276,  IE, "276" },
  { 277,  IE, "277" },
  { 278,  IE, "278" },
  { 287,  IE, "Index corrupted" },
  { 290,  IE, "Corrupt key in TC, unable to xfrm" },
  { 631,  IE, "631" },
  { 632,  IE, "632" },
  { 702,  IE, "Request to non-master" },
  { 706,  IE, "Inconsistency during table creation" },
  { 809,  IE, "809" },
  { 812,  IE, "812" },
  { 829,  IE, "829" },
  { 833,  IE, "833" },
  { 839,  IE, "Illegal null attribute" },
  { 871,  IE, "871" },
  { 882,  IE, "882" },
  { 883,  IE, "883" },
  { 887,  IE, "887" },
  { 888,  IE, "888" },
  { 890,  IE, "890" },
  { 4000, IE, "MEMORY ALLOCATION ERROR" },
  { 4001, IE, "Signal Definition Error" },
  { 4005, IE, "Internal Error in NdbApi" },
  { 4011, IE, "Internal Error in NdbApi" }, 
  { 4107, IE, "Simple Transaction and Not Start" },
  { 4108, IE, "Faulty operation type" },
  { 4109, IE, "Faulty primary key attribute length" },
  { 4110, IE, "Faulty length in ATTRINFO signal" },
  { 4111, IE, "Status Error in NdbConnection" },
  { 4113, IE, "Too many operations received" },
  { 4320, IE, "Cannot use the same object twice to create table" },
  { 4321, IE, "Trying to start two schema transactions" },
  { 4344, IE, "Only DBDICT and TRIX can send requests to TRIX" },
  { 4345, IE, "TRIX block is not available yet, probably due to node failure" },
  { 4346, IE, "Internal error at index create/build" },
  { 4347, IE, "Bad state at alter index" },
  { 4348, IE, "Inconsistency detected at alter index" },
  { 4349, IE, "Inconsistency detected at index usage" },
  { 4350, IE, "Transaction already aborted" },

  /**
   * Application error
   */
  { 299,  AE, "Operation not allowed or aborted due to single user mode" },
  { 763,  AE, "Alter table requires cluster nodes to have exact same version" },
  { 823,  AE, "Too much attrinfo from application in tuple manager" },
  { 831,  AE, "Too many nullable/bitfields in table definition" },
  { 876,  AE, "876" },
  { 877,  AE, "877" },
  { 878,  AE, "878" },
  { 879,  AE, "879" },
  { 880,  AE, "Tried to read too much - too many getValue calls" },
  { 884,  AE, "Stack overflow in interpreter" },
  { 885,  AE, "Stack underflow in interpreter" },
  { 886,  AE, "More than 65535 instructions executed in interpreter" },
  { 897,  AE, "Update attempt of primary key via ndbcluster internal api (if this occurs via the MySQL server it is a bug, please report)" },
  { 892,  AE, "Unsupported type in scan filter" },
  { 4256, AE, "Must call Ndb::init() before this function" },
  { 4257, AE, "Tried to read too much - too many getValue calls" },
  
  /** 
   * Scan application errors
   */
  { 242,  AE, "Zero concurrency in scan"},
  { 244,  AE, "Too high concurrency in scan"},
  { 269,  AE, "No condition and attributes to read in scan"},
  { 874,  AE, "Too much attrinfo (e.g. scan filter) for scan in tuple manager" },
  { 4600, AE, "Transaction is already started"},
  { 4601, AE, "Transaction is not started"},
  { 4602, AE, "You must call getNdbOperation before executeScan" },
  { 4603, AE, "There can only be ONE operation in a scan transaction" },
  { 4604, AE, "takeOverScanOp, to take over a scanned row one must explicitly request keyinfo in readTuples call" },
  { 4605, AE, "You may only call openScanRead or openScanExclusive once for each operation"},
  { 4607, AE, "There may only be one operation in a scan transaction"},
  { 4608, AE, "You can not takeOverScan unless you have used openScanExclusive"},
  { 4609, AE, "You must call nextScanResult before trying to takeOverScan"},
  { 4232, AE, "Parallelism can only be between 1 and 240" },

  /** 
   * Event schema errors
   */

  { 4713,  SE, "Column defined in event does not exist in table"},
  
  /** 
   * Event application errors
   */

  { 4707,  AE, "Too many event have been defined"},
  { 4708,  AE, "Event name is too long"},
  { 4709,  AE, "Can't accept more subscribers"},
  {  746,  OE, "Event name already exists"},
  { 4710,  AE, "Event not found"},
  { 4711,  AE, "Creation of event failed"},
  { 4712,  AE, "Stopped event operation does not exist. Already stopped?"},

  /** 
   * Event internal errors
   */

  { 4731,  IE, "Event not found"},

  /**
   * SchemaError
   */
  { 701,  SE, "System busy with other schema operation" },
  { 711,  SE, "System busy with node restart, schema operations not allowed" },
  { 703,  SE, "Invalid table format" },
  { 704,  SE, "Attribute name too long" },
  { 705,  SE, "Table name too long" },
  { 707,  SE, "No more table metadata records (increase MaxNoOfTables)" },  
  { 708,  SE, "No more attribute metadata records (increase MaxNoOfAttributes)" },
  { 709,  SE, "No such table existed" },
  { 721,  OE, "Table or index with given name already exists" },
  { 723,  SE, "No such table existed" },
  { 736,  SE, "Unsupported array size" },
  { 737,  SE, "Attribute array size too big" },
  { 738,  SE, "Record too big" },
  { 739,  SE, "Unsupported primary key length" },
  { 740,  SE, "Nullable primary key not supported" },
  { 741,  SE, "Unsupported alter table" },
  { 743,  SE, "Unsupported character set in table or index" },
  { 744,  SE, "Character string is invalid for given character set" },
  { 745,  SE, "Distribution key not supported for char attribute (use binary attribute)" },
  { 761,  SE, "Unable to drop table as backup is in progress" },
  { 762,  SE, "Unable to alter table as backup is in progress" },
  { 241,  SE, "Invalid schema object version" },
  { 283,  SE, "Table is being dropped" },
  { 284,  SE, "Table not defined in transaction coordinator" },
  { 285,  SE, "Unknown table error in transaction coordinator" },
  { 881,  SE, "Unable to create table, out of data pages (increase DataMemory) " },
  { 906,  SE, "Unsupported attribute type in index" },
  { 907,  SE, "Unsupported character set in table or index" },
  { 908,  IS, "Invalid ordered index tree node size" },
  { 1225, SE, "Table not defined in local query handler" },
  { 1226, SE, "Table is being dropped" },
  { 1228, SE, "Cannot use drop table for drop index" },
  { 1229, SE, "Too long frm data supplied" },
  { 1231, SE, "Invalid table or index to scan" },
  { 1232, SE, "Invalid table or index to scan" },

  /**
   * FunctionNotImplemented
   */
  { 4003, NI, "Function not implemented yet" },

  /**
   * Backup error codes
   */ 

  { 1300, IE, "Undefined error" },
  { 1301, IE, "Backup issued to not master (reissue command to master)" },
  { 1302, IE, "Out of backup record" },
  { 1303, IS, "Out of resources" },
  { 1304, IE, "Sequence failure" },
  { 1305, IE, "Backup definition not implemented" },
  { 1306, AE, "Backup not supported in diskless mode (change Diskless)" },

  { 1321, UD, "Backup aborted by user request" },
  { 1322, IE, "Backup already completed" },
  { 1323, IE, "1323" },
  { 1324, IE, "Backup log buffer full" },
  { 1325, IE, "File or scan error" },
  { 1326, IE, "Backup abortet due to node failure" },
  { 1327, IE, "1327" },
  
  { 1340, IE, "Backup undefined error" },
  { 1342, AE, "Backup failed to allocate buffers (check configuration)" },
  { 1343, AE, "Backup failed to setup fs buffers (check configuration)" },
  { 1344, AE, "Backup failed to allocate tables (check configuration)" },
  { 1345, AE, "Backup failed to insert file header (check configuration)" },
  { 1346, AE, "Backup failed to insert table list (check configuration)" },
  { 1347, AE, "Backup failed to allocate table memory (check configuration)" },
  { 1348, AE, "Backup failed to allocate file record (check configuration)" },
  { 1349, AE, "Backup failed to allocate attribute record (check configuration)" },
  { 1329, AE, "Backup during software upgrade not supported" },
  
  /**
   * Still uncategorized
   */
  { 720,  AE, "Attribute name reused in table definition" },
  { 4004, AE, "Attribute name or id not found in the table" },
  
  { 4100, AE, "Status Error in NDB" },
  { 4101, AE, "No connections to NDB available and connect failed" },
  { 4102, AE, "Type in NdbTamper not correct" },
  { 4103, AE, "No schema connections to NDB available and connect failed" },
  { 4104, AE, "Ndb Init in wrong state, destroy Ndb object and create a new" },
  { 4105, AE, "Too many Ndb objects" },
  { 4106, AE, "All Not NULL attribute have not been defined" },
  { 4114, AE, "Transaction is already completed" },
  { 4116, AE, "Operation was not defined correctly, probably missing a key" },
  { 4117, AE, "Could not start transporter, configuration error"}, 
  { 4118, AE, "Parameter error in API call" },
  { 4300, AE, "Tuple Key Type not correct" },
  { 4301, AE, "Fragment Type not correct" },
  { 4302, AE, "Minimum Load Factor not correct" },
  { 4303, AE, "Maximum Load Factor not correct" },
  { 4304, AE, "Maximum Load Factor smaller than Minimum" },
  { 4305, AE, "K value must currently be set to 6" },
  { 4306, AE, "Memory Type not correct" },
  { 4307, AE, "Invalid table name" },
  { 4308, AE, "Attribute Size not correct" },
  { 4309, AE, "Fixed array too large, maximum 64000 bytes" },
  { 4310, AE, "Attribute Type not correct" },
  { 4311, AE, "Storage Mode not correct" },
  { 4312, AE, "Null Attribute Type not correct" },
  { 4313, AE, "Index only storage for non-key attribute" },
  { 4314, AE, "Storage Type of attribute not correct" },
  { 4315, AE, "No more key attributes allowed after defining variable length key attribute" },
  { 4316, AE, "Key attributes are not allowed to be NULL attributes" },
  { 4317, AE, "Too many primary keys defined in table" },
  { 4318, AE, "Invalid attribute name" },
  { 4319, AE, "createAttribute called at erroneus place" },
  { 4322, AE, "Attempt to define distribution key when not prepared to" },
  { 4323, AE, "Distribution Key set on table but not defined on first attribute" },
  { 4324, AE, "Attempt to define distribution group when not prepared to" },
  { 4325, AE, "Distribution Group set on table but not defined on first attribute" },
  { 4326, AE, "Distribution Group with erroneus number of bits" },
  { 4327, AE, "Distribution Group with 1 byte attribute is not allowed" },
  { 4328, AE, "Disk memory attributes not yet supported" },
  { 4329, AE, "Variable stored attributes not yet supported" },

  { 4400, AE, "Status Error in NdbSchemaCon" },
  { 4401, AE, "Only one schema operation per schema transaction" },
  { 4402, AE, "No schema operation defined before calling execute" },

  { 4501, AE, "Insert in hash table failed when getting table information from Ndb" },
  { 4502, AE, "GetValue not allowed in Update operation" },
  { 4503, AE, "GetValue not allowed in Insert operation" },
  { 4504, AE, "SetValue not allowed in Read operation" },
  { 4505, AE, "NULL value not allowed in primary key search" },
  { 4506, AE, "Missing getValue/setValue when calling execute" },
  { 4507, AE, "Missing operation request when calling execute" },

  { 4200, AE, "Status Error when defining an operation" },
  { 4201, AE, "Variable Arrays not yet supported" },
  { 4202, AE, "Set value on tuple key attribute is not allowed" },
  { 4203, AE, "Trying to set a NOT NULL attribute to NULL" },
  { 4204, AE, "Set value and Read/Delete Tuple is incompatible" },
  { 4205, AE, "No Key attribute used to define tuple" },
  { 4206, AE, "Not allowed to equal key attribute twice" },
  { 4207, AE, "Key size is limited to 4092 bytes" },
  { 4208, AE, "Trying to read a non-stored attribute" },
  { 4209, AE, "Length parameter in equal/setValue is incorrect" },
  { 4210, AE, "Ndb sent more info than the length he specified" },
  { 4211, AE, "Inconsistency in list of NdbRecAttr-objects" },
  { 4212, AE, "Ndb reports NULL value on Not NULL attribute" },
  { 4213, AE, "Not all data of an attribute has been received" },
  { 4214, AE, "Not all attributes have been received" },
  { 4215, AE, "More data received than reported in TCKEYCONF message" },
  { 4216, AE, "More than 8052 bytes in setValue cannot be handled" },
  { 4217, AE, "It is not allowed to increment any other than unsigned ints" },
  { 4218, AE, "Currently not allowed to increment NULL-able attributes" },
  { 4219, AE, "Maximum size of interpretative attributes are 64 bits" },
  { 4220, AE, "Maximum size of interpretative attributes are 64 bits" },
  { 4221, AE, "Trying to jump to a non-defined label" },
  { 4222, AE, "Label was not found, internal error" },
  { 4223, AE, "Not allowed to create jumps to yourself" },
  { 4224, AE, "Not allowed to jump to a label in a different subroutine" },
  { 4225, AE, "All primary keys defined, call setValue/getValue"},
  { 4226, AE, "Bad number when defining a label" },
  { 4227, AE, "Bad number when defining a subroutine" },
  { 4228, AE, "Illegal interpreter function in scan definition" },
  { 4229, AE, "Illegal register in interpreter function definition" },
  { 4230, AE, "Illegal state when calling getValue, probably not a read" },
  { 4231, AE, "Illegal state when calling interpreter routine" },
  { 4233, AE, "Calling execute (synchronous) when already prepared asynchronous transaction exists" },
  { 4234, AE, "Illegal to call setValue in this state" },
  { 4235, AE, "No callback from execute" },
  { 4236, AE, "Trigger name too long" },
  { 4237, AE, "Too many triggers" },
  { 4238, AE, "Trigger not found" },
  { 4239, AE, "Trigger with given name already exists"},
  { 4240, AE, "Unsupported trigger type"},
  { 4241, AE, "Index name too long" },
  { 4242, AE, "Too many indexes" },
  { 4243, AE, "Index not found" },
  { 4244, OE, "Index or table with given name already exists" },
  { 4247, AE, "Illegal index/trigger create/drop/alter request" },
  { 4248, AE, "Trigger/index name invalid" },
  { 4249, AE, "Invalid table" },
  { 4250, AE, "Invalid index type or index logging option" },
  { 4251, AE, "Cannot create unique index, duplicate keys found" },
  { 4252, AE, "Failed to allocate space for index" },
  { 4253, AE, "Failed to create index table" },
  { 4254, AE, "Table not an index table" },
  { 4255, AE, "Hash index attributes must be specified in same order as table attributes" },
  { 4258, AE, "Cannot create unique index, duplicate attributes found in definition" },
  { 4259, AE, "Invalid set of range scan bounds" },
  { 4260, UD, "NdbScanFilter: Operator is not defined in NdbScanFilter::Group"},
  { 4261, UD, "NdbScanFilter: Column is NULL"},
  { 4262, UD, "NdbScanFilter: Condition is out of bounds"},
  { 4263, IE, "Invalid blob attributes or invalid blob parts table" },
  { 4264, AE, "Invalid usage of blob attribute" },
  { 4265, AE, "The blob method is not valid in current blob state" },
  { 4266, AE, "Invalid blob seek position" },
  { 4267, IE, "Corrupted blob value" },
  { 4268, IE, "Error in blob head update forced rollback of transaction" },
  { 4269, IE, "No connection to ndb management server" },
  { 4270, IE, "Unknown blob error" },
  { 4335, AE, "Only one autoincrement column allowed per table. Having a table without primary key uses an autoincremented hidden key, i.e. a table without a primary key can not have an autoincremented column" },
  { 4271, AE, "Invalid index object, not retrieved via getIndex()" },
  { 4275, AE, "The blob method is incompatible with operation type or lock mode" },
  { 4294, AE, "Scan filter is too large, discarded" }
};

static
const
int NbErrorCodes = sizeof(ErrorCodes)/sizeof(ErrorBundle);

typedef struct ErrorStatusMessage {
  ndberror_status status;
  const char * message;
} ErrorStatusMessage;

typedef struct ErrorStatusClassification {
  ndberror_status status;
  ndberror_classification classification;
  const char * message;
} ErrorStatusClassification;

/**
 * Mapping between classification and status
 */
static
const
ErrorStatusMessage StatusMessageMapping[] = {
  { ST_S, "Success"},
  { ST_P, "Permanent error"},
  { ST_T, "Temporary error"},
  { ST_U ,"Unknown result"}
};

static
const
int NbStatus = sizeof(StatusMessageMapping)/sizeof(ErrorStatusMessage);

static
const
ErrorStatusClassification StatusClassificationMapping[] = {
  { ST_S, NE, "No error"},
  { ST_P, AE, "Application error"},
  { ST_P, CE, "Configuration or application error"},
  { ST_P, ND, "No data found"},
  { ST_P, CV, "Constraint violation"},
  { ST_P, SE, "Schema error"},
  { ST_P, UD, "User defined error"},
  { ST_P, IS, "Insufficient space"},
  
  { ST_T, TR, "Temporary Resource error"},
  { ST_T, NR, "Node Recovery error"},
  { ST_T, OL, "Overload error"},
  { ST_T, TO, "Timeout expired"},
  { ST_T, NS, "Node shutdown"},
  
  { ST_U , UR, "Unknown result error"},
  { ST_U , UE, "Unknown error code"},
  
  { ST_P, IE, "Internal error"},
  { ST_P, NI, "Function not implemented"}
};

static
const
int NbClassification = sizeof(StatusClassificationMapping)/sizeof(ErrorStatusClassification);

#ifdef NOT_USED
/**
 * Complete all fields of an NdbError given the error code
 * and details
 */
static
void
set(ndberror_struct * error, int code, const char * details, ...){
  error->code = code;
  {
    va_list ap;
    va_start(ap, details);
    vsnprintf(error->details, sizeof(error->details), details, ap);
    va_end(ap);
  }
}
#endif

void
ndberror_update(ndberror_struct * error){

  int found = 0;
  int i;

  for(i = 0; i<NbErrorCodes; i++){
    if(ErrorCodes[i].code == error->code){
      error->classification = ErrorCodes[i].classification;
      error->message        = ErrorCodes[i].message;
      found = 1;
      break;
    }
  }

  if(!found){
    error->classification = UE;
    error->message        = "Unknown error code";
  }

  found = 0;
  for(i = 0; i<NbClassification; i++){
    if(StatusClassificationMapping[i].classification == error->classification){
      error->status = StatusClassificationMapping[i].status;
      found = 1;
      break;
    }
  }
  if(!found){
    error->status = ST_U;
  }
}

int
checkErrorCodes(){
  int i, j;
  for(i = 0; i<NbErrorCodes; i++)
    for(j = i+1; j<NbErrorCodes; j++)
      if(ErrorCodes[i].code == ErrorCodes[j].code){
	printf("ErrorCode %d is defined multiple times!!\n", 
		 ErrorCodes[i].code);
	assert(0);
      }
  
  return 1;
}

/*static const int a = checkErrorCodes();*/

#if CHECK_ERRORCODES
int main(void){
  checkErrorCodes();
  return 0;
}
#endif

const char *ndberror_status_message(ndberror_status status)
{
  int i;
  for (i= 0; i < NbStatus; i++)
    if (StatusMessageMapping[i].status == status)
      return StatusMessageMapping[i].message;
  return empty_string;
}

const char *ndberror_classification_message(ndberror_classification classification)
{
  int i;
  for (i= 0; i < NbClassification; i++)
    if (StatusClassificationMapping[i].classification == classification)
      return StatusClassificationMapping[i].message;
  return empty_string;
}

int ndb_error_string(int err_no, char *str, int size)
{
  ndberror_struct error;
  int len;

  assert(size > 1);
  if(size <= 1) 
    return 0;
  error.code = err_no;
  ndberror_update(&error);

  len =
    my_snprintf(str, size-1, "%s: %s: %s", error.message,
		ndberror_status_message(error.status),
		ndberror_classification_message(error.classification));
  str[size-1]= '\0';
  
  if (error.classification != UE)
    return len;
  return -len;
}
