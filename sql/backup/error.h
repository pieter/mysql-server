#ifndef _BACKUP_ERROR_H
#define _BACKUP_ERROR_H

namespace util {

/**
  Report error stored in MYSQL_ERROR structure to a client.

  If @c err doesn't contain any error code, the given error code is reported.

  @returns 0 if error was reported, non-zero otherwise.
 */
inline
int report_mysql_error(THD*, MYSQL_ERROR *err, int code= 0)
{
  DBUG_ASSERT(err);

  if (err->level == MYSQL_ERROR::WARN_LEVEL_END
      && !err->msg && !err->code ) // err doesn't store any error
    return -1;

  switch (err->level) {

  case MYSQL_ERROR::WARN_LEVEL_ERROR:
    return my_printf_error(err->code ? err->code : code, err->msg, MYF(0));

  default: // Q: What to do with warnings and notes? push them... ?
    return -1;
  }
}

} // util namespace

#endif
