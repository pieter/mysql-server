// #define PARAMETER_UINT(name, text, min, default, max, flags, update_function)
// #define PARAMETER_BOOL(name, text, default, flags, update_function)

// These flags are defined in include/mysql/plugin.h 
// 0x0000 = Argument required for cmd line
// 0x0100 = Variable is per-connection
// 0x0200 = Server variable is read only
// 0x0400 = Not a server variable
// 0x0800 = Not a command line option
// 0x1000 = No argument for cmd line
// 0x2000 = Argument optional for cmd line
// 0x8000 = String needs memory allocated


PARAMETER_BOOL(consistent_read, "Enable Consistent Read Mode for Repeatable Reads", 1, 0, StorageInterface::updateConsistentRead)
PARAMETER_UINT(debug_mask, "Falcon message type mask for logged messages.", 0, 0, INT_MAX, 0, StorageInterface::updateDebugMask)
PARAMETER_BOOL(debug_server, "Enable Falcon debug code.", 0, 0x0200, NULL)
PARAMETER_UINT(debug_trace, "Falcon debug trace trigger.", 0, 0, INT_MAX, 0, NULL)
PARAMETER_UINT(direct_io, "Whether Falcon should use O_DIRECT.", 0, 1, 2, 0, NULL)
PARAMETER_UINT(gopher_threads, "Number of Falcon gopher threads", 1, 5, 20, 0, NULL)
PARAMETER_UINT(index_chill_threshold, "Mbytes of pending index data that is 'frozen' to the Falcon serial log.", 1, 4, 1024, 0, &updateIndexChillThreshold)
PARAMETER_UINT(io_threads, "Number of Falcon I/O threads", 2, 2, 20, 0, NULL)
PARAMETER_UINT(large_blob_threshold, "Threshold for large blobs", 0, 160000, INT_MAX, 0, NULL)
PARAMETER_UINT(lock_timeout, "Transaction lock time period (milliseconds)", 0, 0, INT_MAX, 0, NULL)
PARAMETER_UINT(max_transaction_backlog, "Maximum number of backlogged transactions.", 1, 150, 1000000, 0, NULL)
PARAMETER_UINT(page_size, "The page size used when creating a Falcon tablespace.", 1024, 4096, 32768, 0x0200, NULL)
PARAMETER_UINT(record_chill_threshold, "Mbytes of pending record data that is 'frozen' to the Falcon serial log.", 1, 5, 1024, 0, &updateRecordChillThreshold)
PARAMETER_UINT(record_scavenge_floor, "A percentage of falcon_record_memory_threshold that defines the amount of record data that will remain in the record cache after a scavenge run.", 10, 50, 90, 0x2000, &StorageInterface::updateRecordScavengeFloor)
PARAMETER_UINT(record_scavenge_threshold, "The percentage of falcon_record_memory_max that will cause the scavenger thread to start scavenging records from the record cache.", 10, 67, 100, 0x2000, &StorageInterface::updateRecordScavengeThreshold)
PARAMETER_UINT(serial_log_block_size, "Minimum block size for serial log.", 0, 0, 4096, 0, NULL)
PARAMETER_UINT(serial_log_buffers, "The number of buffers allocated for Falcon serial log.", 10, 20, 32768, 0x0200, NULL)
PARAMETER_UINT(serial_log_priority, "Whether or not serial log has write priority over other writes.", 0, 1, 1, 0, NULL)
PARAMETER_BOOL(use_deferred_index_hash, "Use Deferred Index hash lookup", 0, 0, NULL)
PARAMETER_BOOL(support_xa, "Enable XA two phase commit", 0, 0x0200, NULL) 

// #define PARAMETER_BOOL(name, text, default, flags, update_function)
