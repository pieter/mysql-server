/* Copyright (C) 2000-2006 MySQL AB
   
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


/* Execute DO statement */

#include "mysql_priv.h"

bool mysql_do(THD *thd, List<Item> &values)
{
  List_iterator<Item> li(values);
  Item *value;
  DBUG_ENTER("mysql_do");
  if (setup_fields(thd, 0, values, 0, 0, 0))
    DBUG_RETURN(TRUE);
  while ((value = li++))
    value->val_int();
  free_underlaid_joins(thd, &thd->lex->select_lex);
  thd->clear_error(); // DO always is OK
  send_ok(thd);
  DBUG_RETURN(FALSE);
}
