--disable_query_log
# Ignore errors when trying to install the Falcon plugin
--disable_abort_on_error
# Install the plugin first. This should handle platform differences.
#install plugin falcon soname 'ha_falcon.so';
--enable_abort_on_error
--require r/true.require
select support = 'YES' or support = 'DEFAULT' as `TRUE` from information_schema.engines where engine = 'Falcon';
--enable_query_log
