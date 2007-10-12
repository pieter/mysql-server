// #define PARAMETER(name, text, min, default, max, flags, function)

PARAMETER(debug_mask, "Falcon message type mask for logged messages.", 0, 0, INT_MAX, 0, NULL)
PARAMETER(debug_trace, "Falcon debug trace trigger.", 0, 0, INT_MAX, 0, NULL)
PARAMETER(purifier_interval, "Falcon purifier internal.", 0, 0, INT_MAX, 0, NULL)
PARAMETER(purifier_stale_threshold, "Falcon write threshold for stale pages.", 0, 20, INT_MAX, 0, NULL)
PARAMETER(sync_threshold, "Falcon page writes per sync.", 0, 0, INT_MAX, 0, NULL)
PARAMETER(io_threads, "Number of Falcon I/O threads", 2, 5, 20, 0, NULL)
