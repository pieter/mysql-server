/* Copyright (C) 2002 MySQL AB

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

#include "mysql_priv.h"
#include "sp.h"
#include "sp_head.h"
#include "sp_cache.h"
#include "sql_trigger.h"

#include <my_user.h>

static bool
create_string(THD *thd, String *buf,
	      int sp_type,
	      sp_name *name,
	      const char *params, ulong paramslen,
	      const char *returns, ulong returnslen,
	      const char *body, ulong bodylen,
	      st_sp_chistics *chistics,
              const LEX_STRING *definer_user,
              const LEX_STRING *definer_host);
static int
db_load_routine(THD *thd, int type, sp_name *name, sp_head **sphp,
                ulong sql_mode, const char *params, const char *returns,
                const char *body, st_sp_chistics &chistics,
                const char *definer, longlong created, longlong modified,
                Stored_program_creation_ctx *creation_ctx);

/*
 *
 * DB storage of Stored PROCEDUREs and FUNCTIONs
 *
 */

enum
{
  MYSQL_PROC_FIELD_DB = 0,
  MYSQL_PROC_FIELD_NAME,
  MYSQL_PROC_MYSQL_TYPE,
  MYSQL_PROC_FIELD_SPECIFIC_NAME,
  MYSQL_PROC_FIELD_LANGUAGE,
  MYSQL_PROC_FIELD_ACCESS,
  MYSQL_PROC_FIELD_DETERMINISTIC,
  MYSQL_PROC_FIELD_SECURITY_TYPE,
  MYSQL_PROC_FIELD_PARAM_LIST,
  MYSQL_PROC_FIELD_RETURNS,
  MYSQL_PROC_FIELD_BODY,
  MYSQL_PROC_FIELD_DEFINER,
  MYSQL_PROC_FIELD_CREATED,
  MYSQL_PROC_FIELD_MODIFIED,
  MYSQL_PROC_FIELD_SQL_MODE,
  MYSQL_PROC_FIELD_COMMENT,
  MYSQL_PROC_FIELD_CHARACTER_SET_CLIENT,
  MYSQL_PROC_FIELD_COLLATION_CONNECTION,
  MYSQL_PROC_FIELD_DB_COLLATION,
  MYSQL_PROC_FIELD_BODY_UTF8,
  MYSQL_PROC_FIELD_COUNT
};

/* Tells what SP_DEFAULT_ACCESS should be mapped to */
#define SP_DEFAULT_ACCESS_MAPPING SP_CONTAINS_SQL

/*************************************************************************/

/**
  Stored_routine_creation_ctx -- creation context of stored routines
  (stored procedures and functions).
*/

class Stored_routine_creation_ctx : public Stored_program_creation_ctx,
                                    public Sql_alloc
{
public:
  static Stored_routine_creation_ctx *
  load_from_db(THD *thd, const sp_name *name, TABLE *proc_tbl);

public:
  virtual Stored_program_creation_ctx *clone(MEM_ROOT *mem_root)
  {
    return new (mem_root) Stored_routine_creation_ctx(m_client_cs,
                                                      m_connection_cl,
                                                      m_db_cl);
  }

protected:
  virtual Object_creation_ctx *create_backup_ctx(THD *thd) const
  {
    DBUG_ENTER("Stored_routine_creation_ctx::create_backup_ctx");
    DBUG_RETURN(new Stored_routine_creation_ctx(thd));
  }

private:
  Stored_routine_creation_ctx(THD *thd)
    : Stored_program_creation_ctx(thd)
  { }

  Stored_routine_creation_ctx(CHARSET_INFO *client_cs,
                              CHARSET_INFO *connection_cl,
                              CHARSET_INFO *db_cl)
    : Stored_program_creation_ctx(client_cs, connection_cl, db_cl)
  { }
};

/**************************************************************************
  Stored_routine_creation_ctx implementation.
**************************************************************************/

bool load_charset(MEM_ROOT *mem_root,
                  Field *field,
                  CHARSET_INFO *dflt_cs,
                  CHARSET_INFO **cs)
{
  String cs_name;

  if (get_field(mem_root, field, &cs_name))
  {
    *cs= dflt_cs;
    return TRUE;
  }

  *cs= get_charset_by_csname(cs_name.c_ptr(), MY_CS_PRIMARY, MYF(0));

  if (*cs == NULL)
  {
    *cs= dflt_cs;
    return TRUE;
  }

  return FALSE;
}

/*************************************************************************/

bool load_collation(MEM_ROOT *mem_root,
                    Field *field,
                    CHARSET_INFO *dflt_cl,
                    CHARSET_INFO **cl)
{
  String cl_name;

  if (get_field(mem_root, field, &cl_name))
  {
    *cl= dflt_cl;
    return TRUE;
  }

  *cl= get_charset_by_name(cl_name.c_ptr(), MYF(0));

  if (*cl == NULL)
  {
    *cl= dflt_cl;
    return TRUE;
  }

  return FALSE;
}

/*************************************************************************/

Stored_routine_creation_ctx *
Stored_routine_creation_ctx::load_from_db(THD *thd,
                                         const sp_name *name,
                                         TABLE *proc_tbl)
{
  /* Load character set/collation attributes. */

  CHARSET_INFO *client_cs;
  CHARSET_INFO *connection_cl;
  CHARSET_INFO *db_cl;

  const char *db_name= thd->strmake(name->m_db.str, name->m_db.length);
  const char *sr_name= thd->strmake(name->m_name.str, name->m_name.length);

  bool invalid_creation_ctx= FALSE;

  if (load_charset(thd->mem_root,
                   proc_tbl->field[MYSQL_PROC_FIELD_CHARACTER_SET_CLIENT],
                   thd->variables.character_set_client,
                   &client_cs))
  {
    sql_print_warning("Stored routine '%s'.'%s': invalid value "
                      "in column mysql.proc.character_set_client.",
                      (const char *) db_name,
                      (const char *) sr_name);

    invalid_creation_ctx= TRUE;
  }

  if (load_collation(thd->mem_root,
                     proc_tbl->field[MYSQL_PROC_FIELD_COLLATION_CONNECTION],
                     thd->variables.collation_connection,
                     &connection_cl))
  {
    sql_print_warning("Stored routine '%s'.'%s': invalid value "
                      "in column mysql.proc.collation_connection.",
                      (const char *) db_name,
                      (const char *) sr_name);

    invalid_creation_ctx= TRUE;
  }

  if (load_collation(thd->mem_root,
                     proc_tbl->field[MYSQL_PROC_FIELD_DB_COLLATION],
                     NULL,
                     &db_cl))
  {
    sql_print_warning("Stored routine '%s'.'%s': invalid value "
                      "in column mysql.proc.db_collation.",
                      (const char *) db_name,
                      (const char *) sr_name);

    invalid_creation_ctx= TRUE;
  }

  if (invalid_creation_ctx)
  {
    push_warning_printf(thd,
                        MYSQL_ERROR::WARN_LEVEL_WARN,
                        ER_SR_INVALID_CREATION_CTX,
                        ER(ER_SR_INVALID_CREATION_CTX),
                        (const char *) db_name,
                        (const char *) sr_name);
  }

  /*
    If we failed to retrieve the database collation, load the default one
    from the disk.
  */

  if (!db_cl)
    db_cl= get_default_db_collation(thd, name->m_db.str);

  /* Create the context. */

  return new Stored_routine_creation_ctx(client_cs, connection_cl, db_cl);
}

/*************************************************************************/

/*
  Open the mysql.proc table for read.

  SYNOPSIS
    open_proc_table_for_read()
      thd     Thread context
      backup  Pointer to Open_tables_state instance where information about
              currently open tables will be saved, and from which will be
              restored when we will end work with mysql.proc.

  RETURN
    0	Error
    #	Pointer to TABLE object of mysql.proc
*/

TABLE *open_proc_table_for_read(THD *thd, Open_tables_state *backup)
{
  DBUG_ENTER("open_proc_table_for_read");

  TABLE_LIST table;
  bzero((char*) &table, sizeof(table));
  table.db= (char*) "mysql";
  table.table_name= table.alias= (char*)"proc";
  table.lock_type= TL_READ;

  if (!open_system_tables_for_read(thd, &table, backup))
    DBUG_RETURN(table.table);
  else
    DBUG_RETURN(0);
}


/*
  Open the mysql.proc table for update.

  SYNOPSIS
    open_proc_table_for_update()
      thd  Thread context

  NOTES
    Table opened with this call should closed using close_thread_tables().

  RETURN
    0	Error
    #	Pointer to TABLE object of mysql.proc
*/

static TABLE *open_proc_table_for_update(THD *thd)
{
  DBUG_ENTER("open_proc_table_for_update");

  TABLE_LIST table;
  bzero((char*) &table, sizeof(table));
  table.db= (char*) "mysql";
  table.table_name= table.alias= (char*)"proc";
  table.lock_type= TL_WRITE;

  DBUG_RETURN(open_system_table_for_update(thd, &table));
}


/*
  Find row in open mysql.proc table representing stored routine.

  SYNOPSIS
    db_find_routine_aux()
      thd    Thread context
      type   Type of routine to find (function or procedure)
      name   Name of routine
      table  TABLE object for open mysql.proc table.

  RETURN VALUE
    SP_OK           - Routine found
    SP_KEY_NOT_FOUND- No routine with given name
*/

static int
db_find_routine_aux(THD *thd, int type, sp_name *name, TABLE *table)
{
  uchar key[MAX_KEY_LENGTH];	// db, name, optional key length type
  DBUG_ENTER("db_find_routine_aux");
  DBUG_PRINT("enter", ("type: %d  name: %.*s",
		       type, (int) name->m_name.length, name->m_name.str));

  /*
    Create key to find row. We have to use field->store() to be able to
    handle VARCHAR and CHAR fields.
    Assumption here is that the three first fields in the table are
    'db', 'name' and 'type' and the first key is the primary key over the
    same fields.
  */
  if (name->m_name.length > table->field[1]->field_length)
    DBUG_RETURN(SP_KEY_NOT_FOUND);
  table->field[0]->store(name->m_db.str, name->m_db.length, &my_charset_bin);
  table->field[1]->store(name->m_name.str, name->m_name.length,
                         &my_charset_bin);
  table->field[2]->store((longlong) type, TRUE);
  key_copy(key, table->record[0], table->key_info,
           table->key_info->key_length);

  if (table->file->index_read_idx_map(table->record[0], 0, key, HA_WHOLE_KEY,
                                      HA_READ_KEY_EXACT))
    DBUG_RETURN(SP_KEY_NOT_FOUND);

  DBUG_RETURN(SP_OK);
}


/*
  Find routine definition in mysql.proc table and create corresponding
  sp_head object for it.

  SYNOPSIS
    db_find_routine()
      thd   Thread context
      type  Type of routine (TYPE_ENUM_PROCEDURE/...)
      name  Name of routine
      sphp  Out parameter in which pointer to created sp_head
            object is returned (0 in case of error).

  NOTE
    This function may damage current LEX during execution, so it is good
    idea to create temporary LEX and make it active before calling it.

  RETURN VALUE
    0     - Success
    non-0 - Error (may be one of special codes like SP_KEY_NOT_FOUND)
*/

static int
db_find_routine(THD *thd, int type, sp_name *name, sp_head **sphp)
{
  TABLE *table;
  const char *params, *returns, *body;
  int ret;
  const char *definer;
  longlong created;
  longlong modified;
  st_sp_chistics chistics;
  char *ptr;
  uint length;
  char buff[65];
  String str(buff, sizeof(buff), &my_charset_bin);
  ulong sql_mode;
  Open_tables_state open_tables_state_backup;
  Stored_program_creation_ctx *creation_ctx;

  DBUG_ENTER("db_find_routine");
  DBUG_PRINT("enter", ("type: %d name: %.*s",
		       type, (int) name->m_name.length, name->m_name.str));

  *sphp= 0;                                     // In case of errors
  if (!(table= open_proc_table_for_read(thd, &open_tables_state_backup)))
    DBUG_RETURN(SP_OPEN_TABLE_FAILED);

  if ((ret= db_find_routine_aux(thd, type, name, table)) != SP_OK)
    goto done;

  if (table->s->fields < MYSQL_PROC_FIELD_COUNT)
  {
    ret= SP_GET_FIELD_FAILED;
    goto done;
  }

  bzero((char *)&chistics, sizeof(chistics));
  if ((ptr= get_field(thd->mem_root,
		      table->field[MYSQL_PROC_FIELD_ACCESS])) == NULL)
  {
    ret= SP_GET_FIELD_FAILED;
    goto done;
  }
  switch (ptr[0]) {
  case 'N':
    chistics.daccess= SP_NO_SQL;
    break;
  case 'C':
    chistics.daccess= SP_CONTAINS_SQL;
    break;
  case 'R':
    chistics.daccess= SP_READS_SQL_DATA;
    break;
  case 'M':
    chistics.daccess= SP_MODIFIES_SQL_DATA;
    break;
  default:
    chistics.daccess= SP_DEFAULT_ACCESS_MAPPING;
  }

  if ((ptr= get_field(thd->mem_root,
		      table->field[MYSQL_PROC_FIELD_DETERMINISTIC])) == NULL)
  {
    ret= SP_GET_FIELD_FAILED;
    goto done;
  }
  chistics.detistic= (ptr[0] == 'N' ? FALSE : TRUE);    

  if ((ptr= get_field(thd->mem_root,
		      table->field[MYSQL_PROC_FIELD_SECURITY_TYPE])) == NULL)
  {
    ret= SP_GET_FIELD_FAILED;
    goto done;
  }
  chistics.suid= (ptr[0] == 'I' ? SP_IS_NOT_SUID : SP_IS_SUID);

  if ((params= get_field(thd->mem_root,
			 table->field[MYSQL_PROC_FIELD_PARAM_LIST])) == NULL)
  {
    params= "";
  }

  if (type == TYPE_ENUM_PROCEDURE)
    returns= "";
  else if ((returns= get_field(thd->mem_root,
			       table->field[MYSQL_PROC_FIELD_RETURNS])) == NULL)
  {
    ret= SP_GET_FIELD_FAILED;
    goto done;
  }

  if ((body= get_field(thd->mem_root,
		       table->field[MYSQL_PROC_FIELD_BODY])) == NULL)
  {
    ret= SP_GET_FIELD_FAILED;
    goto done;
  }

  // Get additional information
  if ((definer= get_field(thd->mem_root,
			  table->field[MYSQL_PROC_FIELD_DEFINER])) == NULL)
  {
    ret= SP_GET_FIELD_FAILED;
    goto done;
  }

  modified= table->field[MYSQL_PROC_FIELD_MODIFIED]->val_int();
  created= table->field[MYSQL_PROC_FIELD_CREATED]->val_int();

  sql_mode= (ulong) table->field[MYSQL_PROC_FIELD_SQL_MODE]->val_int();

  table->field[MYSQL_PROC_FIELD_COMMENT]->val_str(&str, &str);

  ptr= 0;
  if ((length= str.length()))
    ptr= thd->strmake(str.ptr(), length);
  chistics.comment.str= ptr;
  chistics.comment.length= length;

  creation_ctx= Stored_routine_creation_ctx::load_from_db(thd, name, table);

  close_system_tables(thd, &open_tables_state_backup);
  table= 0;

  ret= db_load_routine(thd, type, name, sphp,
                       sql_mode, params, returns, body, chistics,
                       definer, created, modified, creation_ctx);
 done:
  if (table)
    close_system_tables(thd, &open_tables_state_backup);
  DBUG_RETURN(ret);
}


static int
db_load_routine(THD *thd, int type, sp_name *name, sp_head **sphp,
                ulong sql_mode, const char *params, const char *returns,
                const char *body, st_sp_chistics &chistics,
                const char *definer, longlong created, longlong modified,
                Stored_program_creation_ctx *creation_ctx)
{
  LEX *old_lex= thd->lex, newlex;
  String defstr;
  char saved_cur_db_name_buf[NAME_LEN+1];
  LEX_STRING saved_cur_db_name=
    { saved_cur_db_name_buf, sizeof(saved_cur_db_name_buf) };
  bool cur_db_changed;
  ulong old_sql_mode= thd->variables.sql_mode;
  ha_rows old_select_limit= thd->variables.select_limit;
  sp_rcontext *old_spcont= thd->spcont;
  
  char definer_user_name_holder[USERNAME_LENGTH + 1];
  LEX_STRING definer_user_name= { definer_user_name_holder,
                                  USERNAME_LENGTH };

  char definer_host_name_holder[HOSTNAME_LENGTH + 1];
  LEX_STRING definer_host_name= { definer_host_name_holder, HOSTNAME_LENGTH };

  int ret;

  thd->variables.sql_mode= sql_mode;
  thd->variables.select_limit= HA_POS_ERROR;

  thd->lex= &newlex;
  newlex.current_select= NULL;

  parse_user(definer, strlen(definer),
             definer_user_name.str, &definer_user_name.length,
             definer_host_name.str, &definer_host_name.length);

  defstr.set_charset(creation_ctx->get_client_cs());

  /*
    We have to add DEFINER clause and provide proper routine characterstics in
    routine definition statement that we build here to be able to use this
    definition for SHOW CREATE PROCEDURE later.
   */

  if (!create_string(thd, &defstr,
		     type,
		     name,
		     params, strlen(params),
		     returns, strlen(returns),
		     body, strlen(body),
		     &chistics, &definer_user_name, &definer_host_name))
  {
    ret= SP_INTERNAL_ERROR;
    goto end;
  }

  /*
    Change the current database (if needed).

    TODO: why do we force switch here?
  */

  if (mysql_opt_change_db(thd, &name->m_db, &saved_cur_db_name, TRUE,
                          &cur_db_changed))
  {
    ret= SP_INTERNAL_ERROR;
    goto end;
  }

  thd->spcont= NULL;

  {
    Lex_input_stream lip(thd, defstr.c_ptr(), defstr.length());

    lex_start(thd);

    ret= parse_sql(thd, &lip, creation_ctx) || newlex.sphead == NULL;

    /*
      Force switching back to the saved current database (if changed),
      because it may be NULL. In this case, mysql_change_db() would
      generate an error.
    */

    if (cur_db_changed && mysql_change_db(thd, &saved_cur_db_name, TRUE))
    {
      delete newlex.sphead;
      ret= SP_INTERNAL_ERROR;
      goto end;
    }

    if (ret)
    {
      delete newlex.sphead;
      ret= SP_PARSE_ERROR;
      goto end;
    }

    *sphp= newlex.sphead;
    (*sphp)->set_definer(&definer_user_name, &definer_host_name);
    (*sphp)->set_info(created, modified, &chistics, sql_mode);
    (*sphp)->set_creation_ctx(creation_ctx);
    (*sphp)->optimize();
    /*
      Not strictly necessary to invoke this method here, since we know
      that we've parsed CREATE PROCEDURE/FUNCTION and not an
      UPDATE/DELETE/INSERT/REPLACE/LOAD/CREATE TABLE, but we try to
      maintain the invariant that this method is called for each
      distinct statement, in case its logic is extended with other
      types of analyses in future.
    */
    newlex.set_trg_event_type_for_tables();
  }

end:
  lex_end(thd->lex);
  thd->spcont= old_spcont;
  thd->variables.sql_mode= old_sql_mode;
  thd->variables.select_limit= old_select_limit;
  thd->lex= old_lex;
  return ret;
}


static void
sp_returns_type(THD *thd, String &result, sp_head *sp)
{
  TABLE table;
  TABLE_SHARE share;
  Field *field;
  bzero((char*) &table, sizeof(table));
  bzero((char*) &share, sizeof(share));
  table.in_use= thd;
  table.s = &share;
  field= sp->create_result_field(0, 0, &table);
  field->sql_type(result);

  if (field->has_charset())
  {
    result.append(STRING_WITH_LEN(" CHARSET "));
    result.append(field->charset()->csname);
  }

  delete field;
}


/**
  Write stored-routine object into mysql.proc.

  This operation stores attributes of the stored procedure/function into
  the mysql.proc.

  @param thd  Thread context.
  @param type Stored routine type
              (TYPE_ENUM_PROCEDURE or TYPE_ENUM_FUNCTION).
  @param sp   Stored routine object to store.

  @return Error code. SP_OK is returned on success. Other SP_ constants are
  used to indicate about errors.
*/

int
sp_create_routine(THD *thd, int type, sp_head *sp)
{
  int ret;
  TABLE *table;
  char definer[USER_HOST_BUFF_SIZE];

  CHARSET_INFO *db_cs= get_default_db_collation(thd, sp->m_db.str);

  DBUG_ENTER("sp_create_routine");
  DBUG_PRINT("enter", ("type: %d  name: %.*s",type, (int) sp->m_name.length,
                       sp->m_name.str));

  DBUG_ASSERT(type == TYPE_ENUM_PROCEDURE ||
              type == TYPE_ENUM_FUNCTION);

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  thd->clear_current_stmt_binlog_row_based();

  if (!(table= open_proc_table_for_update(thd)))
    ret= SP_OPEN_TABLE_FAILED;
  else
  {
    restore_record(table, s->default_values); // Get default values for fields

    /* NOTE: all needed privilege checks have been already done. */
    strxnmov(definer, sizeof(definer)-1, thd->lex->definer->user.str, "@",
            thd->lex->definer->host.str, NullS);

    if (table->s->fields < MYSQL_PROC_FIELD_COUNT)
    {
      ret= SP_GET_FIELD_FAILED;
      goto done;
    }

    if (system_charset_info->cset->numchars(system_charset_info,
                                            sp->m_name.str,
                                            sp->m_name.str+sp->m_name.length) >
        table->field[MYSQL_PROC_FIELD_NAME]->char_length())
    {
      ret= SP_BAD_IDENTIFIER;
      goto done;
    }
    if (sp->m_body.length > table->field[MYSQL_PROC_FIELD_BODY]->field_length)
    {
      ret= SP_BODY_TOO_LONG;
      goto done;
    }
    table->field[MYSQL_PROC_FIELD_DB]->
      store(sp->m_db.str, sp->m_db.length, system_charset_info);
    table->field[MYSQL_PROC_FIELD_NAME]->
      store(sp->m_name.str, sp->m_name.length, system_charset_info);
    table->field[MYSQL_PROC_MYSQL_TYPE]->
      store((longlong)type, TRUE);
    table->field[MYSQL_PROC_FIELD_SPECIFIC_NAME]->
      store(sp->m_name.str, sp->m_name.length, system_charset_info);
    if (sp->m_chistics->daccess != SP_DEFAULT_ACCESS)
      table->field[MYSQL_PROC_FIELD_ACCESS]->
	store((longlong)sp->m_chistics->daccess, TRUE);
    table->field[MYSQL_PROC_FIELD_DETERMINISTIC]->
      store((longlong)(sp->m_chistics->detistic ? 1 : 2), TRUE);
    if (sp->m_chistics->suid != SP_IS_DEFAULT_SUID)
      table->field[MYSQL_PROC_FIELD_SECURITY_TYPE]->
	store((longlong)sp->m_chistics->suid, TRUE);
    table->field[MYSQL_PROC_FIELD_PARAM_LIST]->
      store(sp->m_params.str, sp->m_params.length, system_charset_info);
    if (sp->m_type == TYPE_ENUM_FUNCTION)
    {
      String retstr(64);
      sp_returns_type(thd, retstr, sp);
      table->field[MYSQL_PROC_FIELD_RETURNS]->
	store(retstr.ptr(), retstr.length(), system_charset_info);
    }
    table->field[MYSQL_PROC_FIELD_BODY]->
      store(sp->m_body.str, sp->m_body.length, system_charset_info);
    table->field[MYSQL_PROC_FIELD_DEFINER]->
      store(definer, (uint)strlen(definer), system_charset_info);
    ((Field_timestamp *)table->field[MYSQL_PROC_FIELD_CREATED])->set_time();
    ((Field_timestamp *)table->field[MYSQL_PROC_FIELD_MODIFIED])->set_time();
    table->field[MYSQL_PROC_FIELD_SQL_MODE]->
      store((longlong)thd->variables.sql_mode, TRUE);
    if (sp->m_chistics->comment.str)
      table->field[MYSQL_PROC_FIELD_COMMENT]->
	store(sp->m_chistics->comment.str, sp->m_chistics->comment.length,
	      system_charset_info);

    if ((sp->m_type == TYPE_ENUM_FUNCTION) &&
        !trust_function_creators && mysql_bin_log.is_open())
    {
      if (!sp->m_chistics->detistic)
      {
	/*
	  Note that this test is not perfect; one could use
	  a non-deterministic read-only function in an update statement.
	*/
	enum enum_sp_data_access access=
	  (sp->m_chistics->daccess == SP_DEFAULT_ACCESS) ?
	  SP_DEFAULT_ACCESS_MAPPING : sp->m_chistics->daccess;
	if (access == SP_CONTAINS_SQL ||
	    access == SP_MODIFIES_SQL_DATA)
	{
	  my_message(ER_BINLOG_UNSAFE_ROUTINE,
		     ER(ER_BINLOG_UNSAFE_ROUTINE), MYF(0));
	  ret= SP_INTERNAL_ERROR;
	  goto done;
	}
      }
      if (!(thd->security_ctx->master_access & SUPER_ACL))
      {
	my_message(ER_BINLOG_CREATE_ROUTINE_NEED_SUPER,
		   ER(ER_BINLOG_CREATE_ROUTINE_NEED_SUPER), MYF(0));
	ret= SP_INTERNAL_ERROR;
	goto done;
      }
    }

    table->field[MYSQL_PROC_FIELD_CHARACTER_SET_CLIENT]->set_notnull();
    table->field[MYSQL_PROC_FIELD_CHARACTER_SET_CLIENT]->store(
      thd->charset()->csname,
      strlen(thd->charset()->csname),
      system_charset_info);

    table->field[MYSQL_PROC_FIELD_COLLATION_CONNECTION]->set_notnull();
    table->field[MYSQL_PROC_FIELD_COLLATION_CONNECTION]->store(
      thd->variables.collation_connection->name,
      strlen(thd->variables.collation_connection->name),
      system_charset_info);

    table->field[MYSQL_PROC_FIELD_DB_COLLATION]->set_notnull();
    table->field[MYSQL_PROC_FIELD_DB_COLLATION]->store(
      db_cs->name, strlen(db_cs->name), system_charset_info);

    table->field[MYSQL_PROC_FIELD_BODY_UTF8]->set_notnull();
    table->field[MYSQL_PROC_FIELD_BODY_UTF8]->store(
      sp->m_body_utf8.str, sp->m_body_utf8.length, system_charset_info);

    ret= SP_OK;
    if (table->file->ha_write_row(table->record[0]))
      ret= SP_WRITE_ROW_FAILED;
    else if (mysql_bin_log.is_open())
    {
      thd->clear_error();

      String log_query;
      log_query.set_charset(system_charset_info);
      log_query.append(STRING_WITH_LEN("CREATE "));
      append_definer(thd, &log_query, &thd->lex->definer->user,
                     &thd->lex->definer->host);

      LEX_STRING stmt_definition;
      stmt_definition.str= (char*) thd->lex->stmt_definition_begin;
      stmt_definition.length= thd->lex->stmt_definition_end
        - thd->lex->stmt_definition_begin;
      trim_whitespace(thd->charset(), & stmt_definition);

      log_query.append(stmt_definition.str, stmt_definition.length);

      /* Such a statement can always go directly to binlog, no trans cache */
      thd->binlog_query(THD::MYSQL_QUERY_TYPE,
                        log_query.c_ptr(), log_query.length(), FALSE, FALSE);
    }

  }

done:
  close_thread_tables(thd);
  DBUG_RETURN(ret);
}


/**
  Delete the record for the stored routine object from mysql.proc.

  The operation deletes the record for the stored routine specified by name
  from the mysql.proc table and invalidates the stored-routine cache.

  @param thd  Thread context.
  @param type Stored routine type
              (TYPE_ENUM_PROCEDURE or TYPE_ENUM_FUNCTION)
  @param name Stored routine name.

  @return Error code. SP_OK is returned on success. Other SP_ constants are
  used to indicate about errors.
*/

int
sp_drop_routine(THD *thd, int type, sp_name *name)
{
  TABLE *table;
  int ret;
  DBUG_ENTER("sp_drop_routine");
  DBUG_PRINT("enter", ("type: %d  name: %.*s",
		       type, (int) name->m_name.length, name->m_name.str));

  DBUG_ASSERT(type == TYPE_ENUM_PROCEDURE ||
              type == TYPE_ENUM_FUNCTION);

  /*
    This statement will be replicated as a statement, even when using
    row-based replication.  The flag will be reset at the end of the
    statement.
  */
  thd->clear_current_stmt_binlog_row_based();

  if (!(table= open_proc_table_for_update(thd)))
    DBUG_RETURN(SP_OPEN_TABLE_FAILED);
  if ((ret= db_find_routine_aux(thd, type, name, table)) == SP_OK)
  {
    if (table->file->ha_delete_row(table->record[0]))
      ret= SP_DELETE_ROW_FAILED;
  }

  if (ret == SP_OK)
  {
    write_bin_log(thd, TRUE, thd->query, thd->query_length);
    sp_cache_invalidate();
  }

  close_thread_tables(thd);
  DBUG_RETURN(ret);
}


/**
  Find and updated the record for the stored routine object in mysql.proc.

  The operation finds the record for the stored routine specified by name
  in the mysql.proc table and updates it with new attributes. After
  successful update, the cache is invalidated.

  @param thd      Thread context.
  @param type     Stored routine type
                  (TYPE_ENUM_PROCEDURE or TYPE_ENUM_FUNCTION)
  @param name     Stored routine name.
  @param chistics New values of stored routine attributes to write.

  @return Error code. SP_OK is returned on success. Other SP_ constants are
  used to indicate about errors.
*/

int
sp_update_routine(THD *thd, int type, sp_name *name, st_sp_chistics *chistics)
{
  TABLE *table;
  int ret;
  DBUG_ENTER("sp_update_routine");
  DBUG_PRINT("enter", ("type: %d  name: %.*s",
		       type, (int) name->m_name.length, name->m_name.str));

  DBUG_ASSERT(type == TYPE_ENUM_PROCEDURE ||
              type == TYPE_ENUM_FUNCTION);
  /*
    This statement will be replicated as a statement, even when using
    row-based replication. The flag will be reset at the end of the
    statement.
  */
  thd->clear_current_stmt_binlog_row_based();

  if (!(table= open_proc_table_for_update(thd)))
    DBUG_RETURN(SP_OPEN_TABLE_FAILED);
  if ((ret= db_find_routine_aux(thd, type, name, table)) == SP_OK)
  {
    store_record(table,record[1]);
    table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;
    ((Field_timestamp *)table->field[MYSQL_PROC_FIELD_MODIFIED])->set_time();
    if (chistics->suid != SP_IS_DEFAULT_SUID)
      table->field[MYSQL_PROC_FIELD_SECURITY_TYPE]->
	store((longlong)chistics->suid, TRUE);
    if (chistics->daccess != SP_DEFAULT_ACCESS)
      table->field[MYSQL_PROC_FIELD_ACCESS]->
	store((longlong)chistics->daccess, TRUE);
    if (chistics->comment.str)
      table->field[MYSQL_PROC_FIELD_COMMENT]->store(chistics->comment.str,
						    chistics->comment.length,
						    system_charset_info);
    if ((ret= table->file->ha_update_row(table->record[1],table->record[0])) &&
        ret != HA_ERR_RECORD_IS_THE_SAME)
      ret= SP_WRITE_ROW_FAILED;
    else
      ret= 0;
  }

  if (ret == SP_OK)
  {
    write_bin_log(thd, TRUE, thd->query, thd->query_length);
    sp_cache_invalidate();
  }

  close_thread_tables(thd);
  DBUG_RETURN(ret);
}


struct st_used_field
{
  const char *field_name;
  uint field_length;
  enum enum_field_types field_type;
  Field *field;
};

static struct st_used_field init_fields[]=
{
  { "Db",                     NAME_CHAR_LEN, MYSQL_TYPE_STRING,    0},
  { "Name",                   NAME_CHAR_LEN, MYSQL_TYPE_STRING,    0},
  { "Type",                               9, MYSQL_TYPE_STRING,    0},
  { "Definer",          USER_HOST_BUFF_SIZE, MYSQL_TYPE_STRING,    0},
  { "Modified",                           0, MYSQL_TYPE_TIMESTAMP, 0},
  { "Created",                            0, MYSQL_TYPE_TIMESTAMP, 0},
  { "Security_type",                      1, MYSQL_TYPE_STRING,    0},
  { "Comment",                NAME_CHAR_LEN, MYSQL_TYPE_STRING,    0},
  { "character_set_client", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING,    0},
  { "collation_connection", MY_CS_NAME_SIZE, MYSQL_TYPE_STRING,    0},
  { "Database Collation",   MY_CS_NAME_SIZE, MYSQL_TYPE_STRING,    0},
  { 0,                                    0, MYSQL_TYPE_STRING,    0}
};


static int
print_field_values(THD *thd, TABLE *table,
		   struct st_used_field *used_fields,
		   int type, const char *wild)
{
  Protocol *protocol= thd->protocol;

  if (table->field[MYSQL_PROC_MYSQL_TYPE]->val_int() == type)
  {
    String db_string;
    String name_string;
    struct st_used_field *used_field= used_fields;

    if (get_field(thd->mem_root, used_field->field, &db_string))
      db_string.set_ascii("", 0);
    used_field+= 1;
    get_field(thd->mem_root, used_field->field, &name_string);

    if (!wild || !wild[0] || !wild_compare(name_string.ptr(), wild, 0))
    {
      protocol->prepare_for_resend();
      protocol->store(&db_string);
      protocol->store(&name_string);
      for (used_field++;
	   used_field->field_name;
	   used_field++)
      {
	switch (used_field->field_type) {
	case MYSQL_TYPE_TIMESTAMP:
	  {
	    MYSQL_TIME tmp_time;

	    bzero((char *)&tmp_time, sizeof(tmp_time));
	    ((Field_timestamp *) used_field->field)->get_time(&tmp_time);
	    protocol->store(&tmp_time);
	  }
	  break;
	default:
	  {
	    String tmp_string;

	    get_field(thd->mem_root, used_field->field, &tmp_string);
	    protocol->store(&tmp_string);
	  }
	  break;
	}
      }
      if (protocol->write())
	return SP_INTERNAL_ERROR;
    }
  }

  return SP_OK;
}


/**
  Implement SHOW STATUS statement for stored routines.

  @param thd          Thread context.
  @param type         Stored routine type
                      (TYPE_ENUM_PROCEDURE or TYPE_ENUM_FUNCTION)
  @param name_pattern Stored routine name pattern.

  @return Error code. SP_OK is returned on success. Other SP_ constants are
  used to indicate about errors.
*/

int
sp_show_status_routine(THD *thd, int type, const char *name_pattern)
{
  TABLE *table;
  TABLE_LIST tables;
  int res;
  DBUG_ENTER("sp_show_status_routine");

  DBUG_ASSERT(type == TYPE_ENUM_PROCEDURE ||
              type == TYPE_ENUM_FUNCTION);

  memset(&tables, 0, sizeof(tables));
  tables.db= (char*)"mysql";
  tables.table_name= tables.alias= (char*)"proc";

  if (! (table= open_ltable(thd, &tables, TL_READ, 0)))
  {
    res= SP_OPEN_TABLE_FAILED;
    goto done;
  }
  else
  {
    Item *item;
    List<Item> field_list;
    struct st_used_field *used_field;
    TABLE_LIST *leaves= 0;
    st_used_field used_fields[array_elements(init_fields)];

    table->use_all_columns();
    memcpy((char*) used_fields, (char*) init_fields, sizeof(used_fields));
    /* Init header */
    for (used_field= &used_fields[0];
	 used_field->field_name;
	 used_field++)
    {
      switch (used_field->field_type) {
      case MYSQL_TYPE_TIMESTAMP:
	item= new Item_return_date_time(used_field->field_name,
                                        MYSQL_TYPE_DATETIME);
	field_list.push_back(item);
	break;
      default:
        item= new Item_empty_string(used_field->field_name,
                                    used_field->field_length);
	field_list.push_back(item);
	break;
      }
    }
    /* Print header */
    if (thd->protocol->send_fields(&field_list, Protocol::SEND_NUM_ROWS |
                                                Protocol::SEND_EOF))
    {
      res= SP_INTERNAL_ERROR;
      goto err_case;
    }

    /*
      Init fields

      tables is not VIEW for sure => we can pass 0 as condition
    */
    thd->lex->select_lex.context.resolve_in_table_list_only(&tables);
    setup_tables(thd, &thd->lex->select_lex.context,
                 &thd->lex->select_lex.top_join_list,
                 &tables, &leaves, FALSE);
    for (used_field= &used_fields[0];
	 used_field->field_name;
	 used_field++)
    {
      Item_field *field= new Item_field(&thd->lex->select_lex.context,
                                        "mysql", "proc",
					used_field->field_name);
      if (!field ||
          !(used_field->field= find_field_in_tables(thd, field, &tables, NULL,
						    0, REPORT_ALL_ERRORS, 1,
                                                    TRUE)))
      {
	res= SP_INTERNAL_ERROR;
	goto err_case1;
      }
    }

    table->file->ha_index_init(0, 1);
    if ((res= table->file->index_first(table->record[0])))
    {
      res= (res == HA_ERR_END_OF_FILE) ? 0 : SP_INTERNAL_ERROR;
      goto err_case1;
    }

    do
    {
      res= print_field_values(thd, table, used_fields, type, name_pattern);

      if (res)
	goto err_case1;
    }
    while (!table->file->index_next(table->record[0]));

    res= SP_OK;
  }

err_case1:
  send_eof(thd);
err_case:
  table->file->ha_index_end();
  close_thread_tables(thd);
done:
  DBUG_RETURN(res);
}


/* Drop all routines in database 'db' */
int
sp_drop_db_routines(THD *thd, char *db)
{
  TABLE *table;
  int ret;
  uint key_len;
  DBUG_ENTER("sp_drop_db_routines");
  DBUG_PRINT("enter", ("db: %s", db));

  ret= SP_OPEN_TABLE_FAILED;
  if (!(table= open_proc_table_for_update(thd)))
    goto err;

  table->field[MYSQL_PROC_FIELD_DB]->store(db, strlen(db), system_charset_info);
  key_len= table->key_info->key_part[0].store_length;

  ret= SP_OK;
  table->file->ha_index_init(0, 1);
  if (! table->file->index_read_map(table->record[0],
                                    (uchar *)table->field[MYSQL_PROC_FIELD_DB]->ptr,
                                    (key_part_map)1, HA_READ_KEY_EXACT))
  {
    int nxtres;
    bool deleted= FALSE;

    do
    {
      if (! table->file->ha_delete_row(table->record[0]))
	deleted= TRUE;		/* We deleted something */
      else
      {
	ret= SP_DELETE_ROW_FAILED;
	nxtres= 0;
	break;
      }
    } while (! (nxtres= table->file->index_next_same(table->record[0],
                                (uchar *)table->field[MYSQL_PROC_FIELD_DB]->ptr,
						     key_len)));
    if (nxtres != HA_ERR_END_OF_FILE)
      ret= SP_KEY_NOT_FOUND;
    if (deleted)
      sp_cache_invalidate();
  }
  table->file->ha_index_end();

  close_thread_tables(thd);

err:
  DBUG_RETURN(ret);
}


/**
  Implement SHOW CREATE statement for stored routines.

  The operation finds the stored routine object specified by name and then
  calls sp_head::show_create_routine() for the object.

  @param thd  Thread context.
  @param type Stored routine type
              (TYPE_ENUM_PROCEDURE or TYPE_ENUM_FUNCTION)
  @param name Stored routine name.

  @return Error status.
    @retval FALSE on success
    @retval TRUE on error
*/

bool
sp_show_create_routine(THD *thd, int type, sp_name *name)
{
  bool err_status= TRUE;
  sp_head *sp;
  sp_cache **cache = type == TYPE_ENUM_PROCEDURE ?
                     &thd->sp_proc_cache : &thd->sp_func_cache;

  DBUG_ENTER("sp_show_create_routine");
  DBUG_PRINT("enter", ("name: %.*s",
                       (int) name->m_name.length,
                       name->m_name.str));

  DBUG_ASSERT(type == TYPE_ENUM_PROCEDURE ||
              type == TYPE_ENUM_FUNCTION);

  if (type == TYPE_ENUM_PROCEDURE)
  {
    /*
       SHOW CREATE PROCEDURE may require two instances of one sp_head
       object when SHOW CREATE PROCEDURE is called for the procedure that
       is being executed. Basically, there is no actual recursion, so we
       increase the recursion limit for this statement (kind of hack).

       SHOW CREATE FUNCTION does not require this because SHOW CREATE
       statements are prohibitted within stored functions.
     */

    thd->variables.max_sp_recursion_depth++;
  }

  if ((sp= sp_find_routine(thd, type, name, cache, FALSE)))
    err_status= sp->show_create_routine(thd, type);

  if (type == TYPE_ENUM_PROCEDURE)
    thd->variables.max_sp_recursion_depth--;

  DBUG_RETURN(err_status);
}


/*
  Obtain object representing stored procedure/function by its name from
  stored procedures cache and looking into mysql.proc if needed.

  SYNOPSIS
    sp_find_routine()
      thd        - thread context
      type       - type of object (TYPE_ENUM_FUNCTION or TYPE_ENUM_PROCEDURE)
      name       - name of procedure
      cp         - hash to look routine in
      cache_only - if true perform cache-only lookup
                   (Don't look in mysql.proc).

  RETURN VALUE
    Non-0 pointer to sp_head object for the procedure, or
    0 - in case of error.
*/

sp_head *
sp_find_routine(THD *thd, int type, sp_name *name, sp_cache **cp,
                bool cache_only)
{
  sp_head *sp;
  ulong depth= (type == TYPE_ENUM_PROCEDURE ?
                thd->variables.max_sp_recursion_depth :
                0);
  DBUG_ENTER("sp_find_routine");
  DBUG_PRINT("enter", ("name:  %.*s.%.*s  type: %d  cache only %d",
                       (int) name->m_db.length, name->m_db.str,
                       (int) name->m_name.length, name->m_name.str,
                       type, cache_only));

  if ((sp= sp_cache_lookup(cp, name)))
  {
    ulong level;
    sp_head *new_sp;
    const char *returns= "";
    char definer[USER_HOST_BUFF_SIZE];

    /*
      String buffer for RETURNS data type must have system charset;
      64 -- size of "returns" column of mysql.proc.
    */
    String retstr(64);

    DBUG_PRINT("info", ("found: 0x%lx", (ulong)sp));
    if (sp->m_first_free_instance)
    {
      DBUG_PRINT("info", ("first free: 0x%lx  level: %lu  flags %x",
                          (ulong)sp->m_first_free_instance,
                          sp->m_first_free_instance->m_recursion_level,
                          sp->m_first_free_instance->m_flags));
      DBUG_ASSERT(!(sp->m_first_free_instance->m_flags & sp_head::IS_INVOKED));
      if (sp->m_first_free_instance->m_recursion_level > depth)
      {
        sp->recursion_level_error(thd);
        DBUG_RETURN(0);
      }
      DBUG_RETURN(sp->m_first_free_instance);
    }
    /*
      Actually depth could be +1 than the actual value in case a SP calls
      SHOW CREATE PROCEDURE. Hence, the linked list could hold up to one more
      instance.
    */

    level= sp->m_last_cached_sp->m_recursion_level + 1;
    if (level > depth)
    {
      sp->recursion_level_error(thd);
      DBUG_RETURN(0);
    }

    strxmov(definer, sp->m_definer_user.str, "@",
            sp->m_definer_host.str, NullS);
    if (type == TYPE_ENUM_FUNCTION)
    {
      sp_returns_type(thd, retstr, sp);
      returns= retstr.ptr();
    }
    if (db_load_routine(thd, type, name, &new_sp,
                        sp->m_sql_mode, sp->m_params.str, returns,
                        sp->m_body.str, *sp->m_chistics, definer,
                        sp->m_created, sp->m_modified,
                        sp->get_creation_ctx()) == SP_OK)
    {
      sp->m_last_cached_sp->m_next_cached_sp= new_sp;
      new_sp->m_recursion_level= level;
      new_sp->m_first_instance= sp;
      sp->m_last_cached_sp= sp->m_first_free_instance= new_sp;
      DBUG_PRINT("info", ("added level: 0x%lx, level: %lu, flags %x",
                          (ulong)new_sp, new_sp->m_recursion_level,
                          new_sp->m_flags));
      DBUG_RETURN(new_sp);
    }
    DBUG_RETURN(0);
  }
  if (!cache_only)
  {
    if (db_find_routine(thd, type, name, &sp) == SP_OK)
    {
      sp_cache_insert(cp, sp);
      DBUG_PRINT("info", ("added new: 0x%lx, level: %lu, flags %x",
                          (ulong)sp, sp->m_recursion_level,
                          sp->m_flags));
    }
  }
  DBUG_RETURN(sp);
}


/*
  This is used by sql_acl.cc:mysql_routine_grant() and is used to find
  the routines in 'routines'.
*/

int
sp_exist_routines(THD *thd, TABLE_LIST *routines, bool any, bool no_error)
{
  TABLE_LIST *routine;
  bool result= 0;
  bool sp_object_found;
  DBUG_ENTER("sp_exists_routine");
  for (routine= routines; routine; routine= routine->next_global)
  {
    sp_name *name;
    LEX_STRING lex_db;
    LEX_STRING lex_name;
    lex_db.length= strlen(routine->db);
    lex_name.length= strlen(routine->table_name);
    lex_db.str= thd->strmake(routine->db, lex_db.length);
    lex_name.str= thd->strmake(routine->table_name, lex_name.length);
    name= new sp_name(lex_db, lex_name, true);
    name->init_qname(thd);
    sp_object_found= sp_find_routine(thd, TYPE_ENUM_PROCEDURE, name,
                                     &thd->sp_proc_cache, FALSE) != NULL ||
                     sp_find_routine(thd, TYPE_ENUM_FUNCTION, name,
                                     &thd->sp_func_cache, FALSE) != NULL;
    mysql_reset_errors(thd, TRUE);
    if (sp_object_found)
    {
      if (any)
        DBUG_RETURN(1);
      result= 1;
    }
    else if (!any)
    {
      if (!no_error)
      {
	my_error(ER_SP_DOES_NOT_EXIST, MYF(0), "FUNCTION or PROCEDURE", 
		 routine->table_name);
	DBUG_RETURN(-1);
      }
      DBUG_RETURN(0);
    }
  }
  DBUG_RETURN(result);
}


/*
  Check if a routine exists in the mysql.proc table, without actually
  parsing the definition. (Used for dropping)

  SYNOPSIS
    sp_routine_exists_in_table()
      thd        - thread context
      name       - name of procedure

  RETURN VALUE
    0     - Success
    non-0 - Error;  SP_OPEN_TABLE_FAILED or SP_KEY_NOT_FOUND
*/

int
sp_routine_exists_in_table(THD *thd, int type, sp_name *name)
{
  TABLE *table;
  int ret;
  Open_tables_state open_tables_state_backup;

  if (!(table= open_proc_table_for_read(thd, &open_tables_state_backup)))
    ret= SP_OPEN_TABLE_FAILED;
  else
  {
    if ((ret= db_find_routine_aux(thd, type, name, table)) != SP_OK)
      ret= SP_KEY_NOT_FOUND;
    close_system_tables(thd, &open_tables_state_backup);
  }
  return ret;
}


/*
  Structure that represents element in the set of stored routines
  used by statement or routine.
*/
struct Sroutine_hash_entry;

struct Sroutine_hash_entry
{
  /* Set key consisting of one-byte routine type and quoted routine name. */
  LEX_STRING key;
  /*
    Next element in list linking all routines in set. See also comments
    for LEX::sroutine/sroutine_list and sp_head::m_sroutines.
  */
  Sroutine_hash_entry *next;
  /*
    Uppermost view which directly or indirectly uses this routine.
    0 if routine is not used in view. Note that it also can be 0 if
    statement uses routine both via view and directly.
  */
  TABLE_LIST *belong_to_view;
};


extern "C" uchar* sp_sroutine_key(const uchar *ptr, size_t *plen,
                                  my_bool first)
{
  Sroutine_hash_entry *rn= (Sroutine_hash_entry *)ptr;
  *plen= rn->key.length;
  return (uchar *)rn->key.str;
}


/*
  Check if
   - current statement (the one in thd->lex) needs table prelocking
   - first routine in thd->lex->sroutines_list needs to execute its body in
     prelocked mode.

  SYNOPSIS
    sp_get_prelocking_info()
      thd                  Current thread, thd->lex is the statement to be
                           checked.
      need_prelocking      OUT TRUE  - prelocked mode should be activated
                                       before executing the statement
                               FALSE - Don't activate prelocking 
      first_no_prelocking  OUT TRUE  - Tables used by first routine in
                                       thd->lex->sroutines_list should be
                                       prelocked.
                               FALSE - Otherwise.
  NOTES 
    This function assumes that for any "CALL proc(...)" statement routines_list 
    will have 'proc' as first element (it may have several, consider e.g.
    "proc(sp_func(...)))". This property is currently guaranted by the parser.
*/

void sp_get_prelocking_info(THD *thd, bool *need_prelocking, 
                            bool *first_no_prelocking)
{
  Sroutine_hash_entry *routine;
  routine= (Sroutine_hash_entry*)thd->lex->sroutines_list.first;

  DBUG_ASSERT(routine);
  bool first_is_procedure= (routine->key.str[0] == TYPE_ENUM_PROCEDURE);

  *first_no_prelocking= first_is_procedure;
  *need_prelocking= !first_is_procedure || test(routine->next);
}


/*
  Auxilary function that adds new element to the set of stored routines
  used by statement.

  SYNOPSIS
    add_used_routine()
      lex             LEX representing statement
      arena           Arena in which memory for new element will be allocated
      key             Key for the hash representing set
      belong_to_view  Uppermost view which uses this routine
                      (0 if routine is not used by view)

  NOTES
    Will also add element to end of 'LEX::sroutines_list' list.

    In case when statement uses stored routines but does not need
    prelocking (i.e. it does not use any tables) we will access the
    elements of LEX::sroutines set on prepared statement re-execution.
    Because of this we have to allocate memory for both hash element
    and copy of its key in persistent arena.

  TODO
    When we will got rid of these accesses on re-executions we will be
    able to allocate memory for hash elements in non-persitent arena
    and directly use key values from sp_head::m_sroutines sets instead
    of making their copies.

  RETURN VALUE
    TRUE  - new element was added.
    FALSE - element was not added (because it is already present in the set).
*/

static bool add_used_routine(LEX *lex, Query_arena *arena,
                             const LEX_STRING *key,
                             TABLE_LIST *belong_to_view)
{
  hash_init_opt(&lex->sroutines, system_charset_info,
                Query_tables_list::START_SROUTINES_HASH_SIZE,
                0, 0, sp_sroutine_key, 0, 0);

  if (!hash_search(&lex->sroutines, (uchar *)key->str, key->length))
  {
    Sroutine_hash_entry *rn=
      (Sroutine_hash_entry *)arena->alloc(sizeof(Sroutine_hash_entry) +
                                          key->length);
    if (!rn)              // OOM. Error will be reported using fatal_error().
      return FALSE;
    rn->key.length= key->length;
    rn->key.str= (char *)rn + sizeof(Sroutine_hash_entry);
    memcpy(rn->key.str, key->str, key->length);
    my_hash_insert(&lex->sroutines, (uchar *)rn);
    lex->sroutines_list.link_in_list((uchar *)rn, (uchar **)&rn->next);
    rn->belong_to_view= belong_to_view;
    return TRUE;
  }
  return FALSE;
}


/*
  Add routine which is explicitly used by statement to the set of stored
  routines used by this statement.

  SYNOPSIS
    sp_add_used_routine()
      lex     - LEX representing statement
      arena   - arena in which memory for new element of the set
                will be allocated
      rt      - routine name
      rt_type - routine type (one of TYPE_ENUM_PROCEDURE/...)

  NOTES
    Will also add element to end of 'LEX::sroutines_list' list (and will
    take into account that this is explicitly used routine).

    To be friendly towards prepared statements one should pass
    persistent arena as second argument.
*/

void sp_add_used_routine(LEX *lex, Query_arena *arena,
                         sp_name *rt, char rt_type)
{
  rt->set_routine_type(rt_type);
  (void)add_used_routine(lex, arena, &rt->m_sroutines_key, 0);
  lex->sroutines_list_own_last= lex->sroutines_list.next;
  lex->sroutines_list_own_elements= lex->sroutines_list.elements;
}


/*
  Remove routines which are only indirectly used by statement from
  the set of routines used by this statement.

  SYNOPSIS
    sp_remove_not_own_routines()
      lex  LEX representing statement
*/

void sp_remove_not_own_routines(LEX *lex)
{
  Sroutine_hash_entry *not_own_rt, *next_rt;
  for (not_own_rt= *(Sroutine_hash_entry **)lex->sroutines_list_own_last;
       not_own_rt; not_own_rt= next_rt)
  {
    /*
      It is safe to obtain not_own_rt->next after calling hash_delete() now
      but we want to be more future-proof.
    */
    next_rt= not_own_rt->next;
    hash_delete(&lex->sroutines, (uchar *)not_own_rt);
  }

  *(Sroutine_hash_entry **)lex->sroutines_list_own_last= NULL;
  lex->sroutines_list.next= lex->sroutines_list_own_last;
  lex->sroutines_list.elements= lex->sroutines_list_own_elements;
}


/*
  Merge contents of two hashes representing sets of routines used
  by statements or by other routines.

  SYNOPSIS
    sp_update_sp_used_routines()
      dst - hash to which elements should be added
      src - hash from which elements merged

  NOTE
    This procedure won't create new Sroutine_hash_entry objects,
    instead it will simply add elements from source to destination
    hash. Thus time of life of elements in destination hash becomes
    dependant on time of life of elements from source hash. It also
    won't touch lists linking elements in source and destination
    hashes.
*/

void sp_update_sp_used_routines(HASH *dst, HASH *src)
{
  for (uint i=0 ; i < src->records ; i++)
  {
    Sroutine_hash_entry *rt= (Sroutine_hash_entry *)hash_element(src, i);
    if (!hash_search(dst, (uchar *)rt->key.str, rt->key.length))
      my_hash_insert(dst, (uchar *)rt);
  }
}


/*
  Add contents of hash representing set of routines to the set of
  routines used by statement.

  SYNOPSIS
    sp_update_stmt_used_routines()
      thd             Thread context
      lex             LEX representing statement
      src             Hash representing set from which routines will be added
      belong_to_view  Uppermost view which uses these routines, 0 if none

  NOTE
    It will also add elements to end of 'LEX::sroutines_list' list.
*/

static void
sp_update_stmt_used_routines(THD *thd, LEX *lex, HASH *src,
                             TABLE_LIST *belong_to_view)
{
  for (uint i=0 ; i < src->records ; i++)
  {
    Sroutine_hash_entry *rt= (Sroutine_hash_entry *)hash_element(src, i);
    (void)add_used_routine(lex, thd->stmt_arena, &rt->key, belong_to_view);
  }
}


/*
  Add contents of list representing set of routines to the set of
  routines used by statement.

  SYNOPSIS
    sp_update_stmt_used_routines()
      thd             Thread context
      lex             LEX representing statement
      src             List representing set from which routines will be added
      belong_to_view  Uppermost view which uses these routines, 0 if none

  NOTE
    It will also add elements to end of 'LEX::sroutines_list' list.
*/

static void sp_update_stmt_used_routines(THD *thd, LEX *lex, SQL_LIST *src,
                                         TABLE_LIST *belong_to_view)
{
  for (Sroutine_hash_entry *rt= (Sroutine_hash_entry *)src->first;
       rt; rt= rt->next)
    (void)add_used_routine(lex, thd->stmt_arena, &rt->key, belong_to_view);
}


/*
  Cache sub-set of routines used by statement, add tables used by these
  routines to statement table list. Do the same for all routines used
  by these routines.

  SYNOPSIS
    sp_cache_routines_and_add_tables_aux()
      thd              - thread context
      lex              - LEX representing statement
      start            - first routine from the list of routines to be cached
                         (this list defines mentioned sub-set).
      first_no_prelock - If true, don't add tables or cache routines used by
                         the body of the first routine (i.e. *start)
                         will be executed in non-prelocked mode.
  NOTE
    If some function is missing this won't be reported here.
    Instead this fact will be discovered during query execution.

  RETURN VALUE
     0     - success
     non-0 - failure
*/

static int
sp_cache_routines_and_add_tables_aux(THD *thd, LEX *lex,
                                     Sroutine_hash_entry *start, 
                                     bool first_no_prelock)
{
  int ret= 0;
  bool first= TRUE;
  DBUG_ENTER("sp_cache_routines_and_add_tables_aux");

  for (Sroutine_hash_entry *rt= start; rt; rt= rt->next)
  {
    sp_name name(rt->key.str, rt->key.length);
    int type= rt->key.str[0];
    sp_head *sp;

    if (!(sp= sp_cache_lookup((type == TYPE_ENUM_FUNCTION ?
                              &thd->sp_func_cache : &thd->sp_proc_cache),
                              &name)))
    {
      name.m_name.str= strchr(name.m_qname.str, '.');
      name.m_db.length= name.m_name.str - name.m_qname.str;
      name.m_db.str= strmake_root(thd->mem_root, name.m_qname.str,
                                  name.m_db.length);
      name.m_name.str+= 1;
      name.m_name.length= name.m_qname.length - name.m_db.length - 1;

      switch ((ret= db_find_routine(thd, type, &name, &sp)))
      {
      case SP_OK:
        {
          if (type == TYPE_ENUM_FUNCTION)
            sp_cache_insert(&thd->sp_func_cache, sp);
          else
            sp_cache_insert(&thd->sp_proc_cache, sp);
        }
        break;
      case SP_KEY_NOT_FOUND:
        ret= SP_OK;
        break;
      default:
        /*
          Any error when loading an existing routine is either some problem
          with the mysql.proc table, or a parse error because the contents
          has been tampered with (in which case we clear that error).
        */
        if (ret == SP_PARSE_ERROR)
          thd->clear_error();
        /*
          If we cleared the parse error, or when db_find_routine() flagged
          an error with it's return value without calling my_error(), we
          set the generic "mysql.proc table corrupt" error here.
         */
        if (!thd->net.report_error)
        {
          /*
            SP allows full NAME_LEN chars thus he have to allocate enough
            size in bytes. Otherwise there is stack overrun could happen
            if multibyte sequence is `name`. `db` is still safe because the
            rest of the server checks agains NAME_LEN bytes and not chars.
            Hence, the overrun happens only if the name is in length > 32 and
            uses multibyte (cyrillic, greek, etc.)
          */
          char n[NAME_LEN*2+2];

          /* m_qname.str is not always \0 terminated */
          memcpy(n, name.m_qname.str, name.m_qname.length);
          n[name.m_qname.length]= '\0';
          my_error(ER_SP_PROC_TABLE_CORRUPT, MYF(0), n, ret);
        }
        break;
      }
    }
    if (sp)
    {
      if (!(first && first_no_prelock))
      {
        sp_update_stmt_used_routines(thd, lex, &sp->m_sroutines,
                                     rt->belong_to_view);
        (void)sp->add_used_tables_to_table_list(thd, &lex->query_tables_last,
                                                rt->belong_to_view);
      }
      sp->propagate_attributes(lex);
    }
    first= FALSE;
  }
  DBUG_RETURN(ret);
}


/*
  Cache all routines from the set of used by statement, add tables used
  by those routines to statement table list. Do the same for all routines
  used by those routines.

  SYNOPSIS
    sp_cache_routines_and_add_tables()
      thd              - thread context
      lex              - LEX representing statement
      first_no_prelock - If true, don't add tables or cache routines used by
                         the body of the first routine (i.e. *start)

  RETURN VALUE
     0     - success
     non-0 - failure
*/

int
sp_cache_routines_and_add_tables(THD *thd, LEX *lex, bool first_no_prelock)
{
  return sp_cache_routines_and_add_tables_aux(thd, lex,
           (Sroutine_hash_entry *)lex->sroutines_list.first,
           first_no_prelock);
}


/*
  Add all routines used by view to the set of routines used by statement.
  Add tables used by those routines to statement table list. Do the same
  for all routines used by these routines.

  SYNOPSIS
    sp_cache_routines_and_add_tables_for_view()
      thd   Thread context
      lex   LEX representing statement
      view  Table list element representing view

  RETURN VALUE
     0     - success
     non-0 - failure
*/

int
sp_cache_routines_and_add_tables_for_view(THD *thd, LEX *lex, TABLE_LIST *view)
{
  Sroutine_hash_entry **last_cached_routine_ptr=
                          (Sroutine_hash_entry **)lex->sroutines_list.next;
  sp_update_stmt_used_routines(thd, lex, &view->view->sroutines_list,
                               view->top_table());
  return sp_cache_routines_and_add_tables_aux(thd, lex,
                                              *last_cached_routine_ptr, FALSE);
}


/*
  Add triggers for table to the set of routines used by statement.
  Add tables used by them to statement table list. Do the same for
  all implicitly used routines.

  SYNOPSIS
    sp_cache_routines_and_add_tables_for_triggers()
      thd    thread context
      lex    LEX respresenting statement
      table  Table list element for table with trigger

  RETURN VALUE
     0     - success
     non-0 - failure
*/

int
sp_cache_routines_and_add_tables_for_triggers(THD *thd, LEX *lex,
                                              TABLE_LIST *table)
{
  int ret= 0;

  Sroutine_hash_entry **last_cached_routine_ptr=
    (Sroutine_hash_entry **)lex->sroutines_list.next;

  if (static_cast<int>(table->lock_type) >=
      static_cast<int>(TL_WRITE_ALLOW_WRITE))
  {
    for (int i= 0; i < (int)TRG_EVENT_MAX; i++)
    {
      if (table->trg_event_map &
          static_cast<uint8>(1 << static_cast<int>(i)))
      {
        for (int j= 0; j < (int)TRG_ACTION_MAX; j++)
        {
          /* We can have only one trigger per action type currently */
          sp_head *trigger= table->table->triggers->bodies[i][j];
          if (trigger &&
              add_used_routine(lex, thd->stmt_arena, &trigger->m_sroutines_key,
                               table->belong_to_view))
          {
            trigger->add_used_tables_to_table_list(thd, &lex->query_tables_last,
                                                   table->belong_to_view);
            trigger->propagate_attributes(lex);
            sp_update_stmt_used_routines(thd, lex,
                                         &trigger->m_sroutines,
                                         table->belong_to_view);
          }
        }
      }
    }
  }
  ret= sp_cache_routines_and_add_tables_aux(thd, lex,
                                            *last_cached_routine_ptr,
                                            FALSE);
  return ret;
}


/*
 * Generates the CREATE... string from the table information.
 * Returns TRUE on success, FALSE on (alloc) failure.
 */
static bool
create_string(THD *thd, String *buf,
	      int type,
	      sp_name *name,
	      const char *params, ulong paramslen,
	      const char *returns, ulong returnslen,
	      const char *body, ulong bodylen,
	      st_sp_chistics *chistics,
              const LEX_STRING *definer_user,
              const LEX_STRING *definer_host)
{
  /* Make some room to begin with */
  if (buf->alloc(100 + name->m_qname.length + paramslen + returnslen + bodylen +
		 chistics->comment.length + 10 /* length of " DEFINER= "*/ +
                 USER_HOST_BUFF_SIZE))
    return FALSE;

  buf->append(STRING_WITH_LEN("CREATE "));
  append_definer(thd, buf, definer_user, definer_host);
  if (type == TYPE_ENUM_FUNCTION)
    buf->append(STRING_WITH_LEN("FUNCTION "));
  else
    buf->append(STRING_WITH_LEN("PROCEDURE "));
  append_identifier(thd, buf, name->m_name.str, name->m_name.length);
  buf->append('(');
  buf->append(params, paramslen);
  buf->append(')');
  if (type == TYPE_ENUM_FUNCTION)
  {
    buf->append(STRING_WITH_LEN(" RETURNS "));
    buf->append(returns, returnslen);
  }
  buf->append('\n');
  switch (chistics->daccess) {
  case SP_NO_SQL:
    buf->append(STRING_WITH_LEN("    NO SQL\n"));
    break;
  case SP_READS_SQL_DATA:
    buf->append(STRING_WITH_LEN("    READS SQL DATA\n"));
    break;
  case SP_MODIFIES_SQL_DATA:
    buf->append(STRING_WITH_LEN("    MODIFIES SQL DATA\n"));
    break;
  case SP_DEFAULT_ACCESS:
  case SP_CONTAINS_SQL:
    /* Do nothing */
    break;
  }
  if (chistics->detistic)
    buf->append(STRING_WITH_LEN("    DETERMINISTIC\n"));
  if (chistics->suid == SP_IS_NOT_SUID)
    buf->append(STRING_WITH_LEN("    SQL SECURITY INVOKER\n"));
  if (chistics->comment.length)
  {
    buf->append(STRING_WITH_LEN("    COMMENT "));
    append_unescaped(buf, chistics->comment.str, chistics->comment.length);
    buf->append('\n');
  }
  buf->append(body, bodylen);
  return TRUE;
}
