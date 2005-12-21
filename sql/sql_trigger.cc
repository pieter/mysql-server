/* Copyright (C) 2004-2005 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA */


#include "mysql_priv.h"
#include "sp_head.h"
#include "sql_trigger.h"
#include "parse_file.h"

static const LEX_STRING triggers_file_type=
  {(char *) STRING_WITH_LEN("TRIGGERS")};

const char * const triggers_file_ext= ".TRG";

/*
  Table of .TRG file field descriptors.
  We have here only one field now because in nearest future .TRG
  files will be merged into .FRM files (so we don't need something
  like md5 or created fields).
*/
static File_option triggers_file_parameters[]=
{
  {
    {(char *) STRING_WITH_LEN("triggers") },
    offsetof(class Table_triggers_list, definitions_list),
    FILE_OPTIONS_STRLIST
  },
  {
    {(char *) STRING_WITH_LEN("sql_modes") },
    offsetof(class Table_triggers_list, definition_modes_list),
    FILE_OPTIONS_ULLLIST
  },
  {
    {(char *) STRING_WITH_LEN("definers") },
    offsetof(class Table_triggers_list, definers_list),
    FILE_OPTIONS_STRLIST
  },
  { { 0, 0 }, 0, FILE_OPTIONS_STRING }
};

/*
  This must be kept up to date whenever a new option is added to the list
  above, as it specifies the number of required parameters of the trigger in
  .trg file.
*/

static const int TRG_NUM_REQUIRED_PARAMETERS= 4;
static const int TRG_MAX_VERSIONS= 3;

/*
  Structure representing contents of .TRN file which are used to support
  database wide trigger namespace.
*/

struct st_trigname
{
  LEX_STRING trigger_table;
};

static const LEX_STRING trigname_file_type=
  {(char *) STRING_WITH_LEN("TRIGGERNAME")};

const char * const trigname_file_ext= ".TRN";

static File_option trigname_file_parameters[]=
{
  {
    {(char *) STRING_WITH_LEN("trigger_table")},
    offsetof(struct st_trigname, trigger_table),
   FILE_OPTIONS_ESTRING
  },
  { { 0, 0 }, 0, FILE_OPTIONS_STRING }
};


const LEX_STRING trg_action_time_type_names[]=
{
  { (char *) STRING_WITH_LEN("BEFORE") },
  { (char *) STRING_WITH_LEN("AFTER") }
};

const LEX_STRING trg_event_type_names[]=
{
  { (char *) STRING_WITH_LEN("INSERT") },
  { (char *) STRING_WITH_LEN("UPDATE") },
  { (char *) STRING_WITH_LEN("DELETE") }
};


static TABLE_LIST *add_table_for_trigger(THD *thd, sp_name *trig);

bool handle_old_incorrect_sql_modes(char *&unknown_key, gptr base,
                                    MEM_ROOT *mem_root,
                                    char *end, gptr hook_data);

class Handle_old_incorrect_sql_modes_hook: public Unknown_key_hook
{
private:
  char *path;
public:
  Handle_old_incorrect_sql_modes_hook(char *file_path)
    :path(file_path)
  {};
  virtual bool process_unknown_string(char *&unknown_key, gptr base,
                                      MEM_ROOT *mem_root, char *end);
};

/*
  Create or drop trigger for table.

  SYNOPSIS
    mysql_create_or_drop_trigger()
      thd    - current thread context (including trigger definition in LEX)
      tables - table list containing one table for which trigger is created.
      create - whenever we create (TRUE) or drop (FALSE) trigger

  NOTE
    This function is mainly responsible for opening and locking of table and
    invalidation of all its instances in table cache after trigger creation.
    Real work on trigger creation/dropping is done inside Table_triggers_list
    methods.

  RETURN VALUE
    FALSE Success
    TRUE  error
*/
bool mysql_create_or_drop_trigger(THD *thd, TABLE_LIST *tables, bool create)
{
  TABLE *table;
  bool result= TRUE;
  LEX_STRING definer_user;
  LEX_STRING definer_host;

  DBUG_ENTER("mysql_create_or_drop_trigger");

  /*
    QQ: This function could be merged in mysql_alter_table() function
    But do we want this ?
  */

  /*
    Note that once we will have check for TRIGGER privilege in place we won't
    need second part of condition below, since check_access() function also
    checks that db is specified.
  */
  if (!thd->lex->spname->m_db.length || create && !tables->db_length)
  {
    my_error(ER_NO_DB_ERROR, MYF(0));
    DBUG_RETURN(TRUE);
  }

  if (!create &&
      !(tables= add_table_for_trigger(thd, thd->lex->spname)))
    DBUG_RETURN(TRUE);

  /* We should have only one table in table list. */
  DBUG_ASSERT(tables->next_global == 0);

  /*
    TODO: We should check if user has TRIGGER privilege for table here.
    Now we just require SUPER privilege for creating/dropping because
    we don't have proper privilege checking for triggers in place yet.
  */
  if (check_global_access(thd, SUPER_ACL))
    DBUG_RETURN(TRUE);

  /*
    There is no DETERMINISTIC clause for triggers, so can't check it.
    But a trigger can in theory be used to do nasty things (if it supported
    DROP for example) so we do the check for privileges. For now there is
    already a stronger test right above; but when this stronger test will
    be removed, the test below will hold. Because triggers have the same
    nature as functions regarding binlogging: their body is implicitely
    binlogged, so they share the same danger, so trust_function_creators
    applies to them too.
  */
  if (!trust_function_creators && mysql_bin_log.is_open() &&
      !(thd->security_ctx->master_access & SUPER_ACL))
  {
    my_error(ER_BINLOG_CREATE_ROUTINE_NEED_SUPER, MYF(0));
    DBUG_RETURN(TRUE);
  }

  /* We do not allow creation of triggers on temporary tables. */
  if (create && find_temporary_table(thd, tables->db, tables->table_name))
  {
    my_error(ER_TRG_ON_VIEW_OR_TEMP_TABLE, MYF(0), tables->alias);
    DBUG_RETURN(TRUE);
  }

  /*
    We don't want perform our operations while global read lock is held
    so we have to wait until its end and then prevent it from occuring
    again until we are done. (Acquiring LOCK_open is not enough because
    global read lock is held without helding LOCK_open).
  */
  if (wait_if_global_read_lock(thd, 0, 1))
    DBUG_RETURN(TRUE);

  VOID(pthread_mutex_lock(&LOCK_open));

  if (lock_table_names(thd, tables))
    goto end;

  /* We also don't allow creation of triggers on views. */
  tables->required_type= FRMTYPE_TABLE;

  if (reopen_name_locked_table(thd, tables))
  {
    unlock_table_name(thd, tables);
    goto end;
  }
  table= tables->table;

  if (!table->triggers)
  {
    if (!create)
    {
      my_error(ER_TRG_DOES_NOT_EXIST, MYF(0));
      goto end;
    }

    if (!(table->triggers= new (&table->mem_root) Table_triggers_list(table)))
      goto end;
  }

  result= (create ?
           table->triggers->create_trigger(thd, tables, &definer_user, &definer_host):
           table->triggers->drop_trigger(thd, tables));

end:
  VOID(pthread_mutex_unlock(&LOCK_open));
  start_waiting_global_read_lock(thd);

  if (!result)
  {
    if (mysql_bin_log.is_open())
    {
      thd->clear_error();

      String log_query(thd->query, thd->query_length, system_charset_info);

      if (create)
      {
        log_query.set((char *) 0, 0, system_charset_info); /* reset log_query */

        log_query.append(STRING_WITH_LEN("CREATE "));
        append_definer(thd, &log_query, &definer_user, &definer_host);
        log_query.append(thd->lex->trigger_definition_begin);
      }

      /* Such a statement can always go directly to binlog, no trans cache. */
      Query_log_event qinfo(thd, log_query.ptr(), log_query.length(), 0, FALSE);
      mysql_bin_log.write(&qinfo);
    }

    send_ok(thd);
  }

  DBUG_RETURN(result);
}


/*
  Create trigger for table.

  SYNOPSIS
    create_trigger()
      thd          - current thread context (including trigger definition in
                     LEX)
      tables       - table list containing one open table for which the
                     trigger is created.
      definer_user - [out] after a call it points to 0-terminated string,
                     which contains user name part of the actual trigger
                     definer. The caller is responsible to provide memory for
                     storing LEX_STRING object.
      definer_host - [out] after a call it points to 0-terminated string,
                     which contains host name part of the actual trigger
                     definer. The caller is responsible to provide memory for
                     storing LEX_STRING object.

  NOTE
    Assumes that trigger name is fully qualified.

  RETURN VALUE
    False - success
    True  - error
*/
bool Table_triggers_list::create_trigger(THD *thd, TABLE_LIST *tables,
                                         LEX_STRING *definer_user,
                                         LEX_STRING *definer_host)
{
  LEX *lex= thd->lex;
  TABLE *table= tables->table;
  char dir_buff[FN_REFLEN], file_buff[FN_REFLEN], trigname_buff[FN_REFLEN],
       trigname_path[FN_REFLEN];
  LEX_STRING dir, file, trigname_file;
  LEX_STRING *trg_def, *name;
  ulonglong *trg_sql_mode;
  char trg_definer_holder[HOSTNAME_LENGTH + USERNAME_LENGTH + 2];
  LEX_STRING *trg_definer;
  Item_trigger_field *trg_field;
  struct st_trigname trigname;


  /* Trigger must be in the same schema as target table. */
  if (my_strcasecmp(table_alias_charset, table->s->db, lex->spname->m_db.str))
  {
    my_error(ER_TRG_IN_WRONG_SCHEMA, MYF(0));
    return 1;
  }

  /* We don't allow creation of several triggers of the same type yet */
  if (bodies[lex->trg_chistics.event][lex->trg_chistics.action_time])
  {
    my_message(ER_TRG_ALREADY_EXISTS, ER(ER_TRG_ALREADY_EXISTS), MYF(0));
    return 1;
  }

  /*
    Definer attribute of the Lex instance is always set in sql_yacc.yy when
    trigger is created.
  */

  DBUG_ASSERT(lex->definer);

  /*
    If the specified definer differs from the current user, we should check
    that the current user has SUPER privilege (in order to create trigger
    under another user one must have SUPER privilege).
  */
  
  if (strcmp(lex->definer->user.str, thd->security_ctx->priv_user) ||
      my_strcasecmp(system_charset_info,
                    lex->definer->host.str,
                    thd->security_ctx->priv_host))
  {
    if (check_global_access(thd, SUPER_ACL))
    {
      my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "SUPER");
      return TRUE;
    }
  }

  /*
    Let us check if all references to fields in old/new versions of row in
    this trigger are ok.

    NOTE: We do it here more from ease of use standpoint. We still have to
    do some checks on each execution. E.g. we can catch privilege changes
    only during execution. Also in near future, when we will allow access
    to other tables from trigger we won't be able to catch changes in other
    tables...

    Since we don't plan to access to contents of the fields it does not
    matter that we choose for both OLD and NEW values the same versions
    of Field objects here.
  */
  old_field= new_field= table->field;

  for (trg_field= (Item_trigger_field *)(lex->trg_table_fields.first);
       trg_field; trg_field= trg_field->next_trg_field)
  {
    trg_field->setup_field(thd, table);
    if (!trg_field->fixed &&
        trg_field->fix_fields(thd, (Item **)0))
      return 1;
  }

  /*
    Here we are creating file with triggers and save all triggers in it.
    sql_create_definition_file() files handles renaming and backup of older
    versions
  */
  strxnmov(dir_buff, FN_REFLEN, mysql_data_home, "/", tables->db, "/", NullS);
  dir.length= unpack_filename(dir_buff, dir_buff);
  dir.str= dir_buff;
  file.length=  strxnmov(file_buff, FN_REFLEN, tables->table_name,
                         triggers_file_ext, NullS) - file_buff;
  file.str= file_buff;
  trigname_file.length= strxnmov(trigname_buff, FN_REFLEN,
                                 lex->spname->m_name.str,
                                 trigname_file_ext, NullS) - trigname_buff;
  trigname_file.str= trigname_buff;
  strxnmov(trigname_path, FN_REFLEN, dir_buff, trigname_buff, NullS);

  /* Use the filesystem to enforce trigger namespace constraints. */
  if (!access(trigname_path, F_OK))
  {
    my_error(ER_TRG_ALREADY_EXISTS, MYF(0));
    return 1;
  }

  trigname.trigger_table.str= tables->table_name;
  trigname.trigger_table.length= tables->table_name_length;

  if (sql_create_definition_file(&dir, &trigname_file, &trigname_file_type,
                                 (gptr)&trigname, trigname_file_parameters, 0))
    return 1;

  /*
    Soon we will invalidate table object and thus Table_triggers_list object
    so don't care about place to which trg_def->ptr points and other
    invariants (e.g. we don't bother to update names_list)

    QQ: Hmm... probably we should not care about setting up active thread
        mem_root too.
  */
  if (!(trg_def= (LEX_STRING *)alloc_root(&table->mem_root,
                                          sizeof(LEX_STRING))) ||
      definitions_list.push_back(trg_def, &table->mem_root) ||
      !(trg_sql_mode= (ulonglong*)alloc_root(&table->mem_root,
                                             sizeof(ulonglong))) ||
      definition_modes_list.push_back(trg_sql_mode, &table->mem_root) ||
      !(trg_definer= (LEX_STRING*) alloc_root(&table->mem_root,
                                              sizeof(LEX_STRING))) ||
      definers_list.push_back(trg_definer, &table->mem_root))
    goto err_with_cleanup;

  trg_def->str= thd->query;
  trg_def->length= thd->query_length;
  *trg_sql_mode= thd->variables.sql_mode;

#ifndef NO_EMBEDDED_ACCESS_CHECKS
  if (!is_acl_user(lex->definer->host.str,
      lex->definer->user.str))
  {
    push_warning_printf(thd,
                        MYSQL_ERROR::WARN_LEVEL_NOTE,
                        ER_NO_SUCH_USER,
                        ER(ER_NO_SUCH_USER),
                        lex->definer->user.str,
                        lex->definer->host.str);
  }
#endif /* NO_EMBEDDED_ACCESS_CHECKS */

  *definer_user= lex->definer->user;
  *definer_host= lex->definer->host;

  trg_definer->str= trg_definer_holder;
  trg_definer->length= strxmov(trg_definer->str, definer_user->str, "@",
                               definer_host->str, NullS) - trg_definer->str;

  if (!sql_create_definition_file(&dir, &file, &triggers_file_type,
                                  (gptr)this, triggers_file_parameters,
                                  TRG_MAX_VERSIONS))
    return 0;

err_with_cleanup:
  my_delete(trigname_path, MYF(MY_WME));
  return 1;
}


/*
  Deletes the .TRG file for a table

  SYNOPSIS
    rm_trigger_file()
      path       - char buffer of size FN_REFLEN to be used
                   for constructing path to .TRG file.
      db         - table's database name
      table_name - table's name

  RETURN VALUE
    False - success
    True  - error
*/

static bool rm_trigger_file(char *path, char *db, char *table_name)
{
  strxnmov(path, FN_REFLEN, mysql_data_home, "/", db, "/", table_name,
           triggers_file_ext, NullS);
  unpack_filename(path, path);
  return my_delete(path, MYF(MY_WME));
}


/*
  Deletes the .TRN file for a trigger

  SYNOPSIS
    rm_trigname_file()
      path       - char buffer of size FN_REFLEN to be used
                   for constructing path to .TRN file.
      db         - trigger's database name
      table_name - trigger's name

  RETURN VALUE
    False - success
    True  - error
*/

static bool rm_trigname_file(char *path, char *db, char *trigger_name)
{
  strxnmov(path, FN_REFLEN, mysql_data_home, "/", db, "/", trigger_name,
           trigname_file_ext, NullS);
  unpack_filename(path, path);
  return my_delete(path, MYF(MY_WME));
}


/*
  Drop trigger for table.

  SYNOPSIS
    drop_trigger()
      thd    - current thread context (including trigger definition in LEX)
      tables - table list containing one open table for which trigger is
               dropped.

  RETURN VALUE
    False - success
    True  - error
*/
bool Table_triggers_list::drop_trigger(THD *thd, TABLE_LIST *tables)
{
  LEX *lex= thd->lex;
  LEX_STRING *name;
  List_iterator_fast<LEX_STRING> it_name(names_list);
  List_iterator<LEX_STRING>      it_def(definitions_list);
  List_iterator<ulonglong>       it_mod(definition_modes_list);
  List_iterator<LEX_STRING>      it_definer(definers_list);
  char path[FN_REFLEN];

  while ((name= it_name++))
  {
    it_def++;
    it_mod++;
    it_definer++;

    if (my_strcasecmp(table_alias_charset, lex->spname->m_name.str,
                      name->str) == 0)
    {
      /*
        Again we don't care much about other things required for
        clean trigger removing since table will be reopened anyway.
      */
      it_def.remove();
      it_mod.remove();
      it_definer.remove();

      if (definitions_list.is_empty())
      {
        /*
          TODO: Probably instead of removing .TRG file we should move
          to archive directory but this should be done as part of
          parse_file.cc functionality (because we will need it
          elsewhere).
        */
        if (rm_trigger_file(path, tables->db, tables->table_name))
          return 1;
      }
      else
      {
        char dir_buff[FN_REFLEN], file_buff[FN_REFLEN];
        LEX_STRING dir, file;

        strxnmov(dir_buff, FN_REFLEN, mysql_data_home, "/", tables->db,
                 "/", NullS);
        dir.length= unpack_filename(dir_buff, dir_buff);
        dir.str= dir_buff;
        file.length=  strxnmov(file_buff, FN_REFLEN, tables->table_name,
                               triggers_file_ext, NullS) - file_buff;
        file.str= file_buff;

        if (sql_create_definition_file(&dir, &file, &triggers_file_type,
                                       (gptr)this, triggers_file_parameters,
                                       TRG_MAX_VERSIONS))
          return 1;
      }

      if (rm_trigname_file(path, tables->db, lex->spname->m_name.str))
        return 1;
      return 0;
    }
  }

  my_message(ER_TRG_DOES_NOT_EXIST, ER(ER_TRG_DOES_NOT_EXIST), MYF(0));
  return 1;
}


Table_triggers_list::~Table_triggers_list()
{
  for (int i= 0; i < (int)TRG_EVENT_MAX; i++)
    for (int j= 0; j < (int)TRG_ACTION_MAX; j++)
      delete bodies[i][j];

  if (record1_field)
    for (Field **fld_ptr= record1_field; *fld_ptr; fld_ptr++)
      delete *fld_ptr;
}


/*
  Prepare array of Field objects referencing to TABLE::record[1] instead
  of record[0] (they will represent OLD.* row values in ON UPDATE trigger
  and in ON DELETE trigger which will be called during REPLACE execution).

  SYNOPSIS
    prepare_record1_accessors()
      table - pointer to TABLE object for which we are creating fields.

  RETURN VALUE
    False - success
    True  - error
*/
bool Table_triggers_list::prepare_record1_accessors(TABLE *table)
{
  Field **fld, **old_fld;

  if (!(record1_field= (Field **)alloc_root(&table->mem_root,
                                            (table->s->fields + 1) *
                                            sizeof(Field*))))
    return 1;

  for (fld= table->field, old_fld= record1_field; *fld; fld++, old_fld++)
  {
    /*
      QQ: it is supposed that it is ok to use this function for field
      cloning...
    */
    if (!(*old_fld= (*fld)->new_field(&table->mem_root, table)))
      return 1;
    (*old_fld)->move_field((my_ptrdiff_t)(table->record[1] -
                                          table->record[0]));
  }
  *old_fld= 0;

  return 0;
}


/*
  Adjust Table_triggers_list with new TABLE pointer.

  SYNOPSIS
    set_table()
      new_table - new pointer to TABLE instance
*/

void Table_triggers_list::set_table(TABLE *new_table)
{
  table= new_table;
  for (Field **field= table->triggers->record1_field ; *field ; field++)
  {
    (*field)->table= (*field)->orig_table= new_table;
    (*field)->table_name= &new_table->alias;
  }
}


/*
  Check whenever .TRG file for table exist and load all triggers it contains.

  SYNOPSIS
    check_n_load()
      thd        - current thread context
      db         - table's database name
      table_name - table's name
      table      - pointer to table object
      names_only - stop after loading trigger names

  RETURN VALUE
    False - success
    True  - error
*/

bool Table_triggers_list::check_n_load(THD *thd, const char *db,
                                       const char *table_name, TABLE *table,
                                       bool names_only)
{
  char path_buff[FN_REFLEN];
  LEX_STRING path;
  File_parser *parser;
  LEX_STRING save_db;

  DBUG_ENTER("Table_triggers_list::check_n_load");

  strxnmov(path_buff, FN_REFLEN, mysql_data_home, "/", db, "/", table_name,
           triggers_file_ext, NullS);
  path.length= unpack_filename(path_buff, path_buff);
  path.str= path_buff;

  // QQ: should we analyze errno somehow ?
  if (access(path_buff, F_OK))
    DBUG_RETURN(0);

  /*
    File exists so we got to load triggers.
    FIXME: A lot of things to do here e.g. how about other funcs and being
    more paranoical ?
  */

  if ((parser= sql_parse_prepare(&path, &table->mem_root, 1)))
  {
    if (is_equal(&triggers_file_type, parser->type()))
    {
      Table_triggers_list *triggers=
        new (&table->mem_root) Table_triggers_list(table);
      Handle_old_incorrect_sql_modes_hook sql_modes_hook(path.str);

      if (!triggers)
        DBUG_RETURN(1);

      /*
        We don't have the following attributes in old versions of .TRG file, so
        we should initialize the list for safety:
          - sql_modes;
          - definers;
      */
      triggers->definition_modes_list.empty();
      triggers->definers_list.empty();

      if (parser->parse((gptr)triggers, &table->mem_root,
                        triggers_file_parameters,
                        TRG_NUM_REQUIRED_PARAMETERS,
                        &sql_modes_hook))
        DBUG_RETURN(1);

      List_iterator_fast<LEX_STRING> it(triggers->definitions_list);
      LEX_STRING *trg_create_str, *trg_name_str;
      ulonglong *trg_sql_mode;

      if (triggers->definition_modes_list.is_empty() &&
          !triggers->definitions_list.is_empty())
      {
        /*
          It is old file format => we should fill list of sql_modes.

          We use one mode (current) for all triggers, because we have not
          information about mode in old format.
        */
        if (!(trg_sql_mode= (ulonglong*)alloc_root(&table->mem_root,
                                                   sizeof(ulonglong))))
        {
          DBUG_RETURN(1); // EOM
        }
        *trg_sql_mode= global_system_variables.sql_mode;
        while (it++)
        {
          if (triggers->definition_modes_list.push_back(trg_sql_mode,
                                                        &table->mem_root))
          {
            DBUG_RETURN(1); // EOM
          }
        }
        it.rewind();
      }

      if (triggers->definers_list.is_empty() &&
          !triggers->definitions_list.is_empty())
      {
        /*
          It is old file format => we should fill list of definers.

          If there is no definer information, we should not switch context to
          definer when checking privileges. I.e. privileges for such triggers
          are checked for "invoker" rather than for "definer".
        */

        LEX_STRING *trg_definer;

        if (! (trg_definer= (LEX_STRING*)alloc_root(&table->mem_root,
                                                    sizeof(LEX_STRING))))
          DBUG_RETURN(1); // EOM

        trg_definer->str= "";
        trg_definer->length= 0;

        while (it++)
        {
          if (triggers->definers_list.push_back(trg_definer,
                                                &table->mem_root))
          {
            DBUG_RETURN(1); // EOM
          }
        }

        it.rewind();
      }

      DBUG_ASSERT(triggers->definition_modes_list.elements ==
                  triggers->definitions_list.elements);
      DBUG_ASSERT(triggers->definers_list.elements ==
                  triggers->definitions_list.elements);

      table->triggers= triggers;

      /*
        Construct key that will represent triggers for this table in the set
        of routines used by statement.
      */
      triggers->sroutines_key.length= 1+strlen(db)+1+strlen(table_name)+1;
      if (!(triggers->sroutines_key.str=
              alloc_root(&table->mem_root, triggers->sroutines_key.length)))
        DBUG_RETURN(1);
      triggers->sroutines_key.str[0]= TYPE_ENUM_TRIGGER;
      strxmov(triggers->sroutines_key.str+1, db, ".", table_name, NullS);

      /*
        TODO: This could be avoided if there is no triggers
              for UPDATE and DELETE.
      */
      if (!names_only && triggers->prepare_record1_accessors(table))
        DBUG_RETURN(1);

      char *trg_name_buff;
      List_iterator_fast<ulonglong> itm(triggers->definition_modes_list);
      List_iterator_fast<LEX_STRING> it_definer(triggers->
                                                definers_list);
      LEX *old_lex= thd->lex, lex;
      sp_rcontext *save_spcont= thd->spcont;
      ulong save_sql_mode= thd->variables.sql_mode;

      thd->lex= &lex;

      save_db.str= thd->db;
      save_db.length= thd->db_length;
      thd->db_length= strlen(db);
      thd->db= (char *) db;
      while ((trg_create_str= it++))
      {
        trg_sql_mode= itm++;
        LEX_STRING *trg_definer= it_definer++;
        thd->variables.sql_mode= (ulong)*trg_sql_mode;
        lex_start(thd, (uchar*)trg_create_str->str, trg_create_str->length);

	thd->spcont= 0;
        if (yyparse((void *)thd) || thd->is_fatal_error)
        {
          /*
            Free lex associated resources.
            QQ: Do we really need all this stuff here ?
          */
          delete lex.sphead;
          goto err_with_lex_cleanup;
        }

        lex.sphead->set_info(0, 0, &lex.sp_chistics, *trg_sql_mode);

        triggers->bodies[lex.trg_chistics.event]
                             [lex.trg_chistics.action_time]= lex.sphead;

        if (!trg_definer->length)
        {
          /*
            This trigger was created/imported from the previous version of
            MySQL, which does not support triggers definers. We should emit
            warning here.
          */

          push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                              ER_TRG_NO_DEFINER, ER(ER_TRG_NO_DEFINER),
                              (const char*) db,
                              (const char*) lex.sphead->m_name.str);

          /*
            Set definer to the '' to correct displaying in the information
            schema.
          */

          lex.sphead->set_definer("", 0);

          /*
            Triggers without definer information are executed under the
            authorization of the invoker.
          */

          lex.sphead->m_chistics->suid= SP_IS_NOT_SUID;
        }
        else
          lex.sphead->set_definer(trg_definer->str, trg_definer->length);

        if (triggers->names_list.push_back(&lex.sphead->m_name,
                                           &table->mem_root))
            goto err_with_lex_cleanup;

        if (names_only)
        {
          lex_end(&lex);
          continue;
        }

        /*
          Let us bind Item_trigger_field objects representing access to fields
          in old/new versions of row in trigger to Field objects in table being
          opened.

          We ignore errors here, because if even something is wrong we still
          will be willing to open table to perform some operations (e.g.
          SELECT)...
          Anyway some things can be checked only during trigger execution.
        */
        for (Item_trigger_field *trg_field=
               (Item_trigger_field *)(lex.trg_table_fields.first);
             trg_field;
             trg_field= trg_field->next_trg_field)
          trg_field->setup_field(thd, table);

        triggers->m_spec_var_used[lex.trg_chistics.event]
          [lex.trg_chistics.action_time]=
          lex.trg_table_fields.first ? TRUE : FALSE;

        lex_end(&lex);
      }
      thd->db= save_db.str;
      thd->db_length= save_db.length;
      thd->lex= old_lex;
      thd->spcont= save_spcont;
      thd->variables.sql_mode= save_sql_mode;

      DBUG_RETURN(0);

err_with_lex_cleanup:
      // QQ: anything else ?
      lex_end(&lex);
      thd->lex= old_lex;
      thd->spcont= save_spcont;
      thd->variables.sql_mode= save_sql_mode;
      thd->db= save_db.str;
      thd->db_length= save_db.length;
      DBUG_RETURN(1);
    }

    /*
      We don't care about this error message much because .TRG files will
      be merged into .FRM anyway.
    */
    my_error(ER_WRONG_OBJECT, MYF(0),
             table_name, triggers_file_ext+1, "TRIGGER");
    DBUG_RETURN(1);
  }

  DBUG_RETURN(1);
}


/*
  Obtains and returns trigger metadata

  SYNOPSIS
    get_trigger_info()
      thd       - current thread context
      event     - trigger event type
      time_type - trigger action time
      name      - returns name of trigger
      stmt      - returns statement of trigger
      sql_mode  - returns sql_mode of trigger
      definer_user - returns definer/creator of trigger. The caller is
                  responsible to allocate enough space for storing definer
                  information.

  RETURN VALUE
    False - success
    True  - error
*/

bool Table_triggers_list::get_trigger_info(THD *thd, trg_event_type event,
                                           trg_action_time_type time_type,
                                           LEX_STRING *trigger_name,
                                           LEX_STRING *trigger_stmt,
                                           ulong *sql_mode,
                                           LEX_STRING *definer)
{
  sp_head *body;
  DBUG_ENTER("get_trigger_info");
  if ((body= bodies[event][time_type]))
  {
    *trigger_name= body->m_name;
    *trigger_stmt= body->m_body;
    *sql_mode= body->m_sql_mode;

    if (body->m_chistics->suid == SP_IS_NOT_SUID)
    {
      definer->str[0]= 0;
      definer->length= 0;
    }
    else
    {
      definer->length= strxmov(definer->str, body->m_definer_user.str, "@",
                               body->m_definer_host.str, NullS) - definer->str;
    }

    DBUG_RETURN(0);
  }
  DBUG_RETURN(1);
}


/*
  Find trigger's table from trigger identifier and add it to
  the statement table list.

  SYNOPSIS
    mysql_table_for_trigger()
      thd    - current thread context
      trig   - identifier for trigger

  RETURN VALUE
    0 - error
    # - pointer to TABLE_LIST object for the table
*/

static TABLE_LIST *add_table_for_trigger(THD *thd, sp_name *trig)
{
  LEX *lex= thd->lex;
  char path_buff[FN_REFLEN];
  LEX_STRING path;
  File_parser *parser;
  struct st_trigname trigname;
  DBUG_ENTER("add_table_for_trigger");

  strxnmov(path_buff, FN_REFLEN, mysql_data_home, "/", trig->m_db.str, "/",
           trig->m_name.str, trigname_file_ext, NullS);
  path.length= unpack_filename(path_buff, path_buff);
  path.str= path_buff;

  if (access(path_buff, F_OK))
  {
    my_error(ER_TRG_DOES_NOT_EXIST, MYF(0));
    DBUG_RETURN(0);
  }

  if (!(parser= sql_parse_prepare(&path, thd->mem_root, 1)))
    DBUG_RETURN(0);

  if (!is_equal(&trigname_file_type, parser->type()))
  {
    my_error(ER_WRONG_OBJECT, MYF(0), trig->m_name.str, trigname_file_ext+1,
             "TRIGGERNAME");
    DBUG_RETURN(0);
  }

  if (parser->parse((gptr)&trigname, thd->mem_root,
                    trigname_file_parameters, 1,
                    &file_parser_dummy_hook))
    DBUG_RETURN(0);

  /* We need to reset statement table list to be PS/SP friendly. */
  lex->query_tables= 0;
  lex->query_tables_last= &lex->query_tables;
  DBUG_RETURN(sp_add_to_query_tables(thd, lex, trig->m_db.str,
                                     trigname.trigger_table.str, TL_WRITE));
}


/*
  Drop all triggers for table.

  SYNOPSIS
    drop_all_triggers()
      thd    - current thread context
      db     - schema for table
      name   - name for table

  NOTE
    The calling thread should hold the LOCK_open mutex;

  RETURN VALUE
    False - success
    True  - error
*/

bool Table_triggers_list::drop_all_triggers(THD *thd, char *db, char *name)
{
  TABLE table;
  char path[FN_REFLEN];
  bool result= 0;
  DBUG_ENTER("drop_all_triggers");

  bzero(&table, sizeof(table));
  init_alloc_root(&table.mem_root, 8192, 0);

  safe_mutex_assert_owner(&LOCK_open);

  if (Table_triggers_list::check_n_load(thd, db, name, &table, 1))
  {
    result= 1;
    goto end;
  }
  if (table.triggers)
  {
    LEX_STRING *trigger;
    List_iterator_fast<LEX_STRING> it_name(table.triggers->names_list);

    while ((trigger= it_name++))
    {
      if (rm_trigname_file(path, db, trigger->str))
      {
        /*
          Instead of immediately bailing out with error if we were unable
          to remove .TRN file we will try to drop other files.
        */
        result= 1;
        continue;
      }
    }

    if (rm_trigger_file(path, db, name))
    {
      result= 1;
      goto end;
    }
  }
end:
  if (table.triggers)
    delete table.triggers;
  free_root(&table.mem_root, MYF(0));
  DBUG_RETURN(result);
}



bool Table_triggers_list::process_triggers(THD *thd, trg_event_type event,
                                           trg_action_time_type time_type,
                                           bool old_row_is_record1)
{
  bool err_status= FALSE;
  sp_head *sp_trigger= bodies[event][time_type];

  if (sp_trigger)
  {
    Sub_statement_state statement_state;

    if (old_row_is_record1)
    {
      old_field= record1_field;
      new_field= table->field;
    }
    else
    {
      new_field= record1_field;
      old_field= table->field;
    }

#ifndef NO_EMBEDDED_ACCESS_CHECKS
    Security_context *save_ctx;

    if (sp_change_security_context(thd, sp_trigger, &save_ctx))
      return TRUE;

    /*
      NOTE: TRIGGER_ACL should be used below.
    */

    if (check_global_access(thd, SUPER_ACL))
    {
      sp_restore_security_context(thd, save_ctx);
      return TRUE;
    }

    /*
      If the trigger uses special variables (NEW/OLD), check that we have
      SELECT and UPDATE privileges on the subject table.
    */
    
    if (is_special_var_used(event, time_type))
    {
      TABLE_LIST table_list;
      bzero((char *) &table_list, sizeof (table_list));
      table_list.db= (char *) table->s->db;
      table_list.db_length= strlen(table_list.db);
      table_list.table_name= (char *) table->s->table_name;
      table_list.table_name_length= strlen(table_list.table_name);
      table_list.alias= (char *) table->alias;
      table_list.table= table;

      if (check_table_access(thd, SELECT_ACL | UPDATE_ACL, &table_list, 0))
      {
        sp_restore_security_context(thd, save_ctx);
        return TRUE;
      }
    }
    
#endif // NO_EMBEDDED_ACCESS_CHECKS

    thd->reset_sub_statement_state(&statement_state, SUB_STMT_TRIGGER);
    err_status= sp_trigger->execute_function(thd, 0, 0, 0);
    thd->restore_sub_statement_state(&statement_state);

#ifndef NO_EMBEDDED_ACCESS_CHECKS
    sp_restore_security_context(thd, save_ctx);
#endif // NO_EMBEDDED_ACCESS_CHECKS
  }

  return err_status;
}


/*
  Trigger BUG#14090 compatibility hook

  SYNOPSIS
    Handle_old_incorrect_sql_modes_hook::process_unknown_string()
    unknown_key          [in/out] reference on the line with unknown
                                  parameter and the parsing point
    base                 [in] base address for parameter writing (structure
                              like TABLE)
    mem_root             [in] MEM_ROOT for parameters allocation
    end                  [in] the end of the configuration

  NOTE: this hook process back compatibility for incorrectly written
  sql_modes parameter (see BUG#14090).

  RETURN
    FALSE OK
    TRUE  Error
*/

bool
Handle_old_incorrect_sql_modes_hook::process_unknown_string(char *&unknown_key,
                                                            gptr base,
                                                            MEM_ROOT *mem_root,
                                                            char *end)
{
#define INVALID_SQL_MODES_LENGTH 13
  DBUG_ENTER("handle_old_incorrect_sql_modes");
  DBUG_PRINT("info", ("unknown key:%60s", unknown_key));
  if (unknown_key + INVALID_SQL_MODES_LENGTH + 1 < end &&
      unknown_key[INVALID_SQL_MODES_LENGTH] == '=' &&
      !memcmp(unknown_key, STRING_WITH_LEN("sql_modes")))
  {
    DBUG_PRINT("info", ("sql_modes affected by BUG#14090 detected"));
    push_warning_printf(current_thd,
                        MYSQL_ERROR::WARN_LEVEL_NOTE,
                        ER_OLD_FILE_FORMAT,
                        ER(ER_OLD_FILE_FORMAT),
                        (char *)path, "TRIGGER");
    File_option sql_modes_parameters=
      {
        {(char *) STRING_WITH_LEN("sql_modes") },
        offsetof(class Table_triggers_list, definition_modes_list),
        FILE_OPTIONS_ULLLIST
      };
    char *ptr= unknown_key + INVALID_SQL_MODES_LENGTH + 1;
    if (get_file_options_ulllist(ptr, end, unknown_key, base,
                                 &sql_modes_parameters, mem_root))
    {
      DBUG_RETURN(TRUE);
    }
    /*
      Set parsing pointer to the last symbol of string (\n)
      1) to avoid problem with \0 in the junk after sql_modes
      2) to speed up skipping this line by parser.
    */
    unknown_key= ptr-1;
  }
  DBUG_RETURN(FALSE);
}
