/* Copyright (C) 2003 MySQL AB

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <ndb_global.h>
#include <ndb_opts.h>
#include <NDBT.hpp>
#include <NdbApi.hpp>

void desc_AutoGrowSpecification(struct NdbDictionary::AutoGrowSpecification ags);
int desc_logfilegroup(Ndb *myndb, char* name);
int desc_undofile(Ndb *myndb, char* name);
int desc_datafile(Ndb *myndb, char* name);
int desc_tablespace(Ndb *myndb,char* name);
int desc_table(Ndb *myndb,char* name);

NDB_STD_OPTS_VARS;

static const char* _dbname = "TEST_DB";
static int _unqualified = 0;
static struct my_option my_long_options[] =
{
  NDB_STD_OPTS("ndb_desc"),
  { "database", 'd', "Name of database table is in",
    (gptr*) &_dbname, (gptr*) &_dbname, 0,
    GET_STR, REQUIRED_ARG, 0, 0, 0, 0, 0, 0 },
  { "unqualified", 'u', "Use unqualified table names",
    (gptr*) &_unqualified, (gptr*) &_unqualified, 0,
    GET_BOOL, NO_ARG, 0, 0, 0, 0, 0, 0 }, 
  { 0, 0, 0, 0, 0, 0, GET_NO_ARG, NO_ARG, 0, 0, 0, 0, 0, 0}
};
static void usage()
{
  char desc[] = 
    "tabname\n"\
    "This program list all properties of table(s) in NDB Cluster.\n"\
    "  ex: desc T1 T2 T4\n";
  ndb_std_print_version();
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
}

int main(int argc, char** argv){
  NDB_INIT(argv[0]);
  const char *load_default_groups[]= { "mysql_cluster",0 };
  load_defaults("my",load_default_groups,&argc,&argv);
  int ho_error;
#ifndef DBUG_OFF
  opt_debug= "d:t:O,/tmp/ndb_desc.trace";
#endif
  if ((ho_error=handle_options(&argc, &argv, my_long_options, 
			       ndb_std_get_one_option)))
    return NDBT_ProgramExit(NDBT_WRONGARGS);

  Ndb_cluster_connection con(opt_connect_str);
  if(con.connect(12, 5, 1) != 0)
  {
    ndbout << "Unable to connect to management server." << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  if (con.wait_until_ready(30,0) < 0)
  {
    ndbout << "Cluster nodes not ready in 30 seconds." << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  Ndb MyNdb(&con, _dbname);
  if(MyNdb.init() != 0){
    ERR(MyNdb.getNdbError());
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  NdbDictionary::Dictionary * dict= MyNdb.getDictionary();
  for(int i= 0; i<argc;i++)
  {
    if(desc_table(&MyNdb,argv[i]))
      ;
    else if(desc_tablespace(&MyNdb,argv[i]))
      ;
    else if(desc_logfilegroup(&MyNdb,argv[i]))
      ;
    else if(desc_datafile(&MyNdb, argv[i]))
      ;
    else if(desc_undofile(&MyNdb, argv[i]))
      ;
    else
      ndbout << "No such object: " << argv[i] << endl << endl;
  }

  return NDBT_ProgramExit(NDBT_OK);
}

void desc_AutoGrowSpecification(struct NdbDictionary::AutoGrowSpecification ags)
{
  ndbout << "AutoGrow.min_free: " << ags.min_free << endl;
  ndbout << "AutoGrow.max_size: " << ags.max_size << endl;
  ndbout << "AutoGrow.file_size: " << ags.file_size << endl;
  ndbout << "AutoGrow.filename_pattern: " << ags.filename_pattern << endl;
}

int desc_logfilegroup(Ndb *myndb, char* name)
{
  NdbDictionary::Dictionary *dict= myndb->getDictionary();
  assert(dict);
  NdbDictionary::LogfileGroup lfg= dict->getLogfileGroup(name);
  NdbError err= dict->getNdbError();
  if(err.classification!=ndberror_cl_none)
    return 0;

  ndbout << "Type: LogfileGroup" << endl;
  ndbout << "Name: " << lfg.getName() << endl;
  ndbout << "UndoBuffer size: " << lfg.getUndoBufferSize() << endl;
  ndbout << "Version: " << lfg.getObjectVersion() << endl;

  desc_AutoGrowSpecification(lfg.getAutoGrowSpecification());

  ndbout << endl;

  return 1;
}

int desc_tablespace(Ndb *myndb, char* name)
{
  NdbDictionary::Dictionary *dict= myndb->getDictionary();
  assert(dict);
  NdbDictionary::Tablespace ts= dict->getTablespace(name);
  NdbError err= dict->getNdbError();
  if(err.classification!=ndberror_cl_none)
    return 0;

  ndbout << "Type: Tablespace" << endl;
  ndbout << "Name: " << ts.getName() << endl;
  ndbout << "Object Version: " << ts.getObjectVersion() << endl;
  ndbout << "Extent Size: " << ts.getExtentSize() << endl;
  ndbout << "Default Logfile Group: " << ts.getDefaultLogfileGroup() << endl;
  ndbout << endl;
  return 1;
}

int desc_undofile(Ndb *myndb, char* name)
{
  NdbDictionary::Dictionary *dict= myndb->getDictionary();
  assert(dict);
  NdbDictionary::Undofile uf= dict->getUndofile(0, name);
  NdbError err= dict->getNdbError();
  if(err.classification!=ndberror_cl_none)
    return 0;

  ndbout << "Type: Undofile" << endl;
  ndbout << "Name: " << name << endl;
  ndbout << "Path: " << uf.getPath() << endl;
  ndbout << "Size: " << uf.getSize() << endl;
  ndbout << "Free: " << uf.getFree() << endl;

  ndbout << "Logfile Group: " << uf.getLogfileGroup() << endl;

  /** FIXME: are these needed, the functions aren't there
      but the prototypes are...

      ndbout << "Node: " << uf.getNode() << endl;

      ndbout << "Number: " << uf.getFileNo() << endl;
  */

  ndbout << endl;

  return 1;
}

int desc_datafile(Ndb *myndb, char* name)
{
  NdbDictionary::Dictionary *dict= myndb->getDictionary();
  assert(dict);
  NdbDictionary::Datafile df= dict->getDatafile(0, name);
  NdbError err= dict->getNdbError();
  if(err.classification!=ndberror_cl_none)
    return 0;

  ndbout << "Type: Datafile" << endl;
  ndbout << "Name: " << name << endl;
  ndbout << "Path: " << df.getPath() << endl;
  ndbout << "Size: " << df.getSize() << endl;
  ndbout << "Free: " << df.getFree() << endl;

  ndbout << "Tablespace: " << df.getTablespace() << endl;

  /** FIXME: are these needed, the functions aren't there
      but the prototypes are...

      ndbout << "Node: " << uf.getNode() << endl;

      ndbout << "Number: " << uf.getFileNo() << endl;
  */

  ndbout << endl;

  return 1;
}

int desc_table(Ndb *myndb, char* name)
{
  NdbDictionary::Dictionary * dict= myndb->getDictionary();
  NDBT_Table* pTab = (NDBT_Table*)dict->getTable(name);
  if (!pTab)
    return 0;

  ndbout << (* pTab) << endl;

  NdbDictionary::Dictionary::List list;
  if (dict->listIndexes(list, name) != 0){
    ndbout << name << ": " << dict->getNdbError() << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }

  ndbout << "-- Indexes -- " << endl;
  ndbout << "PRIMARY KEY(";
  unsigned j;
  for (j= 0; (int)j < pTab->getNoOfPrimaryKeys(); j++)
  {
    const NdbDictionary::Column * col= pTab->getColumn(pTab->getPrimaryKey(j));
    ndbout << col->getName();
    if ((int)j < pTab->getNoOfPrimaryKeys()-1)
      ndbout << ", ";
  }
  ndbout << ") - UniqueHashIndex" << endl;
  for (j= 0; j < list.count; j++) {
    NdbDictionary::Dictionary::List::Element& elt = list.elements[j];
    const NdbDictionary::Index *pIdx = dict->getIndex(elt.name, name);
    if (!pIdx){
      ndbout << name << ": " << dict->getNdbError() << endl;
      return NDBT_ProgramExit(NDBT_FAILED);
    }

    ndbout << (*pIdx) << endl;
  }
  ndbout << endl;

  return 1;
}
