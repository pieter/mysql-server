// #define PARAMETER(name, text, min, default, max, flags, function)

PARAMETER(debug_mask, "Falcon message type mask for logged messages.", 0, 0, INT_MAX, 0, NULL)
PARAMETER(debug_trace, "Falcon debug trace trigger.", 0, 0, INT_MAX, 0, NULL)
PARAMETER(purifier_internal, "Falcon purifier internal.", 0, 0, INT_MAX, 0, NULL)
PARAMETER(purifier_sync_threshold, "Falcon page writes per sync.", 0, 0, INT_MAX, 0, NULL)
