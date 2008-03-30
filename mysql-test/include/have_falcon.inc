--disable_query_log
# Ignore errors when trying to install the Falcon plugin
--require r/true.require
SELECT (support = 'YES' OR support = 'DEFAULT' OR support = 'ENABLED') AS `TRUE` FROM INFORMATION_SCHEMA.ENGINES WHERE ENGINE = 'Falcon';
--enable_query_log
