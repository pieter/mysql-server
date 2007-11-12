#include "../mysql_priv.h"

#include "logger.h"

namespace backup {


/**
  Output message on a given level.

  This is the low-level method used by all other methods to output messages.
  It's implementation determines how messages are delivered to the user.
  Currently they are appended to the server's error log.

  @param level       level of the message (INFO,WARNING,ERROR)
  @param error_code  optional code for message coming from errmsg.txt database -
                     for other messages set to 0
  @param msg         message text

  @returns 0 on success.
 */
int Logger::write_message(log_level::value level, int error_code,
                          const char *msg)
{
   const char *prefix= m_type == BACKUP ? "Backup" : "Restore";
   char buf[ERRMSGSIZE + 30];
   
   my_snprintf(buf,sizeof(buf),"%s: %s",prefix,msg);
  
   switch (level) {
   case log_level::ERROR:
     if (m_save_errors)
       errors.push_front(new MYSQL_ERROR(::current_thd,error_code,
                                         MYSQL_ERROR::WARN_LEVEL_ERROR,msg));
     sql_print_error(buf);
     DBUG_PRINT("backup log",("[ERROR] %s",buf));
     return 0;

   case log_level::WARNING:
     sql_print_warning(buf);
     DBUG_PRINT("backup log",("[Warning] %s",buf));
     return 0;

   case log_level::INFO:
     sql_print_information(buf);
     DBUG_PRINT("backup log",("[Info] %s",buf));
     return 0;

   default: return ERROR;
   }
}

/**
  Output message registered in errmsg.txt database.

  @param level       level of the message (INFO,WARNING,ERROR)
  @param error_code  code assigned to the message in errmsg.txt

  If the message contains placeholders, additional arguments provide
  values to be put there.

  @returns 0 on success.
 */
int Logger::v_report_error(log_level::value level, int error_code, va_list args)
{
  return v_write_message(level,error_code,ER_SAFE(error_code),args);
}

/**
  Output unregistered message.

  Format string is given explicitly as an argument.

  Note: no localization support.
 */
int Logger::v_write_message(log_level::value level, int error_code,
                            const char *format, va_list args)
{
  char buf[ERRMSGSIZE + 20];

  my_vsnprintf(buf,sizeof(buf),format,args);
  return write_message(level,error_code,buf);
}


} // backup namespace
