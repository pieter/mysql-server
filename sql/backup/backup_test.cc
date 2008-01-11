/**
  @file

  Implementation of the backup test function.

  @todo Implement code to test service interface(s).
 */

#include "../mysql_priv.h"
#include "si_objects.h"

/**
   Call backup kernel API to execute backup related SQL statement.

   @param[in] thd  current thread
   @param[in] lex  results of parsing the statement.
  */
int execute_backup_test_command(THD *thd, List<LEX_STRING> *db_list)
{
  int res= 0; 

  DBUG_ENTER("execute_backup_command");
  DBUG_ASSERT(thd);

  Protocol *protocol= thd->protocol;    // client comms
  List<Item> field_list;                // list of fields to send
  String     op_str;                    // operations string
  String str;
  Item *i;


  /*
    Send field list.
  */
  field_list.push_back(i= new Item_empty_string("db",2));
  field_list.push_back(new Item_empty_string("table",5));
  field_list.push_back(new Item_empty_string("type",4));
  protocol->send_fields(&field_list, Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF);

  obs::ObjIterator *it= obs::get_databases(thd);
  
  if (it)
  {
    obs::Obj *db;
    
    while ((db= it->next()))
    {
      obs::ObjIterator *tit= obs::get_db_tables(thd,db->get_name());
      
      if (tit)
      {
        obs::Obj *table;
        
        while ((table= tit->next()))
        {
          protocol->prepare_for_resend();
          protocol->store(const_cast<String*>(db->get_name()));
          protocol->store(const_cast<String*>(table->get_name()));
          protocol->store("TABLE",5,system_charset_info);
          protocol->write();
          
          delete table;
        }
      }

      tit= obs::get_db_views(thd,db->get_name());
      
        
      if (tit)
      {
        obs::Obj *table;
        
        while ((table= tit->next()))
        {
          protocol->prepare_for_resend();
          protocol->store(const_cast<String*>(db->get_name()));
          protocol->store(const_cast<String*>(table->get_name()));
          protocol->store("VIEW",5,system_charset_info);
          protocol->write();
          
          delete table;
        }
        
        delete tit;      
      }
      
      delete db;
    }
  }    
  
  delete it;
  
  send_eof(thd);
//  delete i;
  DBUG_RETURN(res);
}

