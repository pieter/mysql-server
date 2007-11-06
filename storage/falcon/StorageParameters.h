// #define PARAMETER(name, text, min, default, max, flags, function)

PARAMETER(debug_mask, "Falcon message type mask for logged messages.", 0, 0, INT_MAX, 0, NULL)
PARAMETER(debug_trace, "Falcon debug trace trigger.", 0, 0, INT_MAX, 0, NULL)
PARAMETER(io_threads, "Number of Falcon I/O threads", 2, 2, 20, 0, NULL)
PARAMETER(large_blob_threshold, "Threshold for large blobs", 0, 160000, INT_MAX, 0, NULL)
