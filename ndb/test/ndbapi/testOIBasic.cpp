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

/*
 * testOIBasic - ordered index test
 */

#include <ndb_global.h>

#include <NdbMain.h>
#include <NdbOut.hpp>
#include <NdbApi.hpp>
#include <NdbTest.hpp>
#include <NdbMutex.h>
#include <NdbCondition.h>
#include <NdbThread.h>
#include <NdbTick.h>

// options

struct Opt {
  // common options
  const char* m_case;
  bool m_core;
  bool m_dups;
  NdbDictionary::Object::FragmentType m_fragtype;
  const char* m_index;
  unsigned m_loop;
  bool m_nologging;
  unsigned m_rows;
  unsigned m_scanrd;
  unsigned m_scanex;
  unsigned m_seed;
  unsigned m_subloop;
  const char* m_table;
  unsigned m_threads;
  unsigned m_v;
  Opt() :
    m_case(0),
    m_core(false),
    m_dups(false),
    m_fragtype(NdbDictionary::Object::FragUndefined),
    m_index(0),
    m_loop(1),
    m_nologging(false),
    m_rows(1000),
    m_scanrd(240),
    m_scanex(240),
    m_seed(1),
    m_subloop(4),
    m_table(0),
    m_threads(4),
    m_v(1) {
  }
};

static Opt g_opt;

static void printcases();
static void printtables();

static void
printhelp()
{
  Opt d;
  ndbout
    << "usage: testOIbasic [options]" << endl
    << "  -case abc     only given test cases (letters a-z)" << endl
    << "  -core         core dump on error [" << d.m_core << "]" << endl
    << "  -dups         allow duplicate tuples from index scan [" << d.m_dups << "]" << endl
    << "  -fragtype T   fragment type single/small/medium/large" << endl
    << "  -index xyz    only given index numbers (digits 1-9)" << endl
    << "  -loop N       loop count full suite forever=0 [" << d.m_loop << "]" << endl
    << "  -nologging    create tables in no-logging mode" << endl
    << "  -rows N       rows per thread [" << d.m_rows << "]" << endl
    << "  -scanrd N     scan read parallelism [" << d.m_scanrd << "]" << endl
    << "  -scanex N     scan exclusive parallelism [" << d.m_scanex << "]" << endl
    << "  -seed N       srandom seed [" << d.m_seed << "]" << endl
    << "  -subloop N    subtest loop count [" << d.m_subloop << "]" << endl
    << "  -table xyz    only given table numbers (digits 1-9)" << endl
    << "  -threads N    number of threads [" << d.m_threads << "]" << endl
    << "  -vN           verbosity [" << d.m_v << "]" << endl
    << "  -h or -help   print this help text" << endl
    ;
  printcases();
  printtables();
}

// log and error macros

static NdbMutex ndbout_mutex = NDB_MUTEX_INITIALIZER;

static unsigned getthrno();

static const char*
getthrstr()
{
  static char buf[20];
  unsigned n = getthrno();
  if (n == (unsigned)-1)
    strcpy(buf, "");
  else {
    unsigned m =
      g_opt.m_threads < 10 ? 1 :
      g_opt.m_threads < 100 ? 2 : 3;
    sprintf(buf, "[%0*u] ", m, n);
  }
  return buf;
}

#define LLN(n, s) \
  do { \
    if ((n) > g_opt.m_v) break; \
    NdbMutex_Lock(&ndbout_mutex); \
    ndbout << getthrstr() << s << endl; \
    NdbMutex_Unlock(&ndbout_mutex); \
  } while(0)

#define LL0(s) LLN(0, s)
#define LL1(s) LLN(1, s)
#define LL2(s) LLN(2, s)
#define LL3(s) LLN(3, s)
#define LL4(s) LLN(4, s)
#define LL5(s) LLN(5, s)

// following check a condition and return -1 on failure

#undef CHK      // simple check
#undef CHKTRY   // execute action (try-catch) on failure
#undef CHKMSG   // print extra message on failure
#undef CHKCON   // print NDB API errors on failure

#define CHK(x) CHKTRY(x, ;)

#define CHKTRY(x, act) \
  do { \
    if (x) break; \
    LL0("line " << __LINE__ << ": " << #x << " failed"); \
    if (g_opt.m_core) abort(); \
    act; \
    return -1; \
  } while (0)

#define CHKMSG(x, msg) \
  do { \
    if (x) break; \
    LL0("line " << __LINE__ << ": " << #x << " failed: " << msg); \
    if (g_opt.m_core) abort(); \
    return -1; \
  } while (0)

#define CHKCON(x, con) \
  do { \
    if (x) break; \
    LL0("line " << __LINE__ << ": " << #x << " failed"); \
    (con).printerror(ndbout); \
    if (g_opt.m_core) abort(); \
    return -1; \
  } while (0)

// method parameters base class

class Thr;
class Con;
class Tab;
class Set;

struct Par : public Opt {
  unsigned m_no;
  Con* m_con;
  Con& con() const { assert(m_con != 0); return *m_con; }
  const Tab* m_tab;
  const Tab& tab() const { assert(m_tab != 0); return *m_tab; }
  Set* m_set;
  Set& set() const { assert(m_set != 0); return *m_set; }
  unsigned m_totrows;
  unsigned m_batch;
  // value calculation
  unsigned m_pctnull;
  unsigned m_range;
  unsigned m_pctrange;
  // do verify after read
  bool m_verify;
  // timer location
  Par(const Opt& opt) :
    Opt(opt),
    m_no(0),
    m_con(0),
    m_tab(0),
    m_set(0),
    m_totrows(m_threads * m_rows),
    m_batch(32),
    m_pctnull(10),
    m_range(m_rows),
    m_pctrange(0),
    m_verify(false) {
  }
};

static bool
usetable(unsigned i)
{
  return g_opt.m_table == 0 || strchr(g_opt.m_table, '1' + i) != 0;
}

static bool
useindex(unsigned i)
{
  return g_opt.m_index == 0 || strchr(g_opt.m_index, '1' + i) != 0;
}

static unsigned
thrrow(Par par, unsigned j)
{
  return par.m_threads * j + par.m_no;
}

static bool
isthrrow(Par par, unsigned i)
{
  return i % par.m_threads == par.m_no;
}

// timer

struct Tmr {
  void clr();
  void on();
  void off(unsigned cnt = 0);
  const char* time();
  const char* over(const Tmr& t1);
  NDB_TICKS m_on;
  unsigned m_ms;
  unsigned m_cnt;
  char m_time[100];
  char m_over[100];
  Tmr() { clr(); }
};

void
Tmr::clr()
{
  m_on = m_ms = m_cnt = m_time[0] = m_over[0] = 0;
}

void
Tmr::on()
{
  assert(m_on == 0);
  m_on = NdbTick_CurrentMillisecond();
}

void
Tmr::off(unsigned cnt)
{
  NDB_TICKS off = NdbTick_CurrentMillisecond();
  assert(m_on != 0 && off >= m_on);
  m_ms += off - m_on;
  m_cnt += cnt;
  m_on = 0;
}

const char*
Tmr::time()
{
  if (m_cnt == 0) {
    sprintf(m_time, "%u ms", m_ms);
  } else {
    sprintf(m_time, "%u ms per %u ( %u ms per 1000 )", m_ms, m_cnt, (1000 * m_ms) / m_cnt);
  }
  return m_time;
}

const char*
Tmr::over(const Tmr& t1)
{
  if (0 < t1.m_ms && t1.m_ms < m_ms) {
    sprintf(m_over, "%u pct", (100 * (m_ms - t1.m_ms)) / t1.m_ms);
  } else {
    sprintf(m_over, "[cannot measure]");
  }
  return m_over;
}

// tables and indexes

// Col - table column

struct Col {
  unsigned m_num;
  const char* m_name;
  bool m_pk;
  NdbDictionary::Column::Type m_type;
  unsigned m_length;
  bool m_nullable;
  void verify(const void* addr) const;
};

void
Col::verify(const void* addr) const
{
  switch (m_type) {
  case NdbDictionary::Column::Unsigned:
    break;
  case NdbDictionary::Column::Varchar:
    {
      const unsigned char* p = (const unsigned char*)addr;
      unsigned n = (p[0] << 8) | p[1];
      assert(n <= m_length);
      for (unsigned i = 0; i < n; i++) {
        assert(p[2 + i] != 0);
      }
      for (unsigned i = n; i < m_length; i++) {
        assert(p[2 + i] == 0);
      }
    }
    break;
  default:
    assert(false);
    break;
  }
}

static NdbOut&
operator<<(NdbOut& out, const Col& col)
{
  out << "col " << col.m_num;
  out << " " << col.m_name;
  switch (col.m_type) {
  case NdbDictionary::Column::Unsigned:
    out << " unsigned";
    break;
  case NdbDictionary::Column::Varchar:
    out << " varchar(" << col.m_length << ")";
    break;
  default:
    out << "type" << (int)col.m_type;
    assert(false);
    break;
  }
  out << (col.m_pk ? " pk" : "");
  out << (col.m_nullable ? " nullable" : "");
  return out;
}

// ICol - index column

struct ICol {
  unsigned m_num;
  struct Col m_col;
};

// ITab - index

struct ITab {
  const char* m_name;
  unsigned m_icols;
  const ICol* m_icol;
};

static NdbOut&
operator<<(NdbOut& out, const ITab& itab)
{
  out << "itab " << itab.m_name << " " << itab.m_icols;
  for (unsigned k = 0; k < itab.m_icols; k++) {
    out << endl;
    out << "icol " << k << " " << itab.m_icol[k].m_col;
  }
  return out;
}

// Tab - table

struct Tab {
  const char* m_name;
  unsigned m_cols;
  const Col* m_col;
  unsigned m_itabs;
  const ITab* m_itab;
};

static NdbOut&
operator<<(NdbOut& out, const Tab& tab)
{
  out << "tab " << tab.m_name << " " << tab.m_cols;
  for (unsigned k = 0; k < tab.m_cols; k++) {
    out << endl;
    out << tab.m_col[k];
  }
  for (unsigned i = 0; i < tab.m_itabs; i++) {
    if (! useindex(i))
      continue;
    out << endl;
    out << tab.m_itab[i];
  }
  return out;
}

// tt1 + tt1x1 tt1x2 tt1x3 tt1x4

static const Col
tt1col[] = {
  { 0, "A", 1, NdbDictionary::Column::Unsigned, 1, 0 },
  { 1, "B", 0, NdbDictionary::Column::Unsigned, 1, 1 },
  { 2, "C", 0, NdbDictionary::Column::Unsigned, 1, 1 },
  { 3, "D", 0, NdbDictionary::Column::Unsigned, 1, 1 },
  { 4, "E", 0, NdbDictionary::Column::Unsigned, 1, 1 }
};

static const ICol
tt1x1col[] = {
  { 0, tt1col[1] }
};

static const ICol
tt1x2col[] = {
  { 0, tt1col[1] },
  { 1, tt1col[2] }
};

static const ICol
tt1x3col[] = {
  { 0, tt1col[3] },
  { 1, tt1col[2] },
  { 2, tt1col[1] }
};

static const ICol
tt1x4col[] = {
  { 0, tt1col[1] },
  { 1, tt1col[4] },
  { 2, tt1col[2] },
  { 3, tt1col[3] }
};

static const ITab
tt1x1 = {
  "TT1X1", 1, tt1x1col
};

static const ITab
tt1x2 = {
  "TT1X2", 2, tt1x2col
};

static const ITab
tt1x3 = {
  "TT1X3", 3, tt1x3col
};

static const ITab
tt1x4 = {
  "TT1X4", 4, tt1x4col
};

static const ITab
tt1itab[] = {
  tt1x1,
  tt1x2,
  tt1x3,
  tt1x4
};

static const Tab
tt1 = {
  "TT1", 5, tt1col, 4, tt1itab
};

// tt2 + tt2x1 tt2x2 tt2x3 tt2x4

static const Col
tt2col[] = {
  { 0, "A", 1, NdbDictionary::Column::Unsigned, 1, 0 },
  { 1, "B", 0, NdbDictionary::Column::Unsigned, 1, 1 },
  { 2, "C", 0, NdbDictionary::Column::Varchar, 20, 1 },
  { 3, "D", 0, NdbDictionary::Column::Varchar, 5, 1 },
  { 4, "E", 0, NdbDictionary::Column::Varchar, 5, 1 }
};

static const ICol
tt2x1col[] = {
  { 0, tt2col[1] },
  { 1, tt2col[2] }
};

static const ICol
tt2x2col[] = {
  { 0, tt2col[2] },
  { 1, tt2col[1] }
};

static const ICol
tt2x3col[] = {
  { 0, tt2col[3] },
  { 1, tt2col[4] }
};

static const ICol
tt2x4col[] = {
  { 0, tt2col[4] },
  { 1, tt2col[3] },
  { 2, tt2col[2] },
  { 3, tt2col[1] }
};

static const ITab
tt2x1 = {
  "TT2X1", 2, tt2x1col
};

static const ITab
tt2x2 = {
  "TT2X2", 2, tt2x2col
};

static const ITab
tt2x3 = {
  "TT2X3", 2, tt2x3col
};

static const ITab
tt2x4 = {
  "TT2X4", 4, tt2x4col
};

static const ITab
tt2itab[] = {
  tt2x1,
  tt2x2,
  tt2x3,
  tt2x4
};

static const Tab
tt2 = {
  "TT2", 5, tt2col, 4, tt2itab
};

// all tables

static const Tab
tablist[] = {
  tt1,
  tt2
};

static const unsigned
tabcount = sizeof(tablist) / sizeof(tablist[0]);

// connections

struct Con {
  Ndb* m_ndb;
  NdbDictionary::Dictionary* m_dic;
  NdbConnection* m_tx;
  NdbOperation* m_op;
  NdbScanOperation* m_scanop;
  NdbIndexScanOperation* m_indexscanop;
  NdbResultSet* m_resultset;
  enum ScanMode { ScanNo = 0, Committed, Latest, Exclusive };
  ScanMode m_scanmode;
  enum ErrType { ErrNone = 0, ErrDeadlock, ErrOther };
  ErrType m_errtype;
  Con() :
    m_ndb(0), m_dic(0), m_tx(0), m_op(0),
    m_scanop(0), m_indexscanop(0), m_resultset(0), m_scanmode(ScanNo), m_errtype(ErrNone) {}
  int connect();
  void disconnect();
  int startTransaction();
  int startBuddyTransaction(const Con& con);
  int getNdbOperation(const Tab& tab);
  int getNdbScanOperation(const Tab& tab);
  int getNdbScanOperation(const ITab& itab, const Tab& tab);
  int equal(int num, const char* addr);
  int getValue(int num, NdbRecAttr*& rec);
  int setValue(int num, const char* addr);
  int setBound(int num, int type, const void* value);
  int execute(ExecType t);
  int openScanRead(unsigned parallelism);
  int openScanExclusive(unsigned parallelism);
  int executeScan();
  int nextScanResult();
  int takeOverForUpdate(Con& scan);
  int takeOverForDelete(Con& scan);
  void closeTransaction();
  void printerror(NdbOut& out);
  // flush dict cache
  int bugger() {
    //disconnect();
    //CHK(connect() == 0);
    return 0;
  }
};

int
Con::connect()
{
  assert(m_ndb == 0);
  m_ndb = new Ndb("TEST_DB");
  CHKCON(m_ndb->init() == 0, *this);
  CHKCON(m_ndb->waitUntilReady(30) == 0, *this);
  m_dic = m_ndb->getDictionary();
  m_tx = 0, m_op = 0;
  return 0;
}

void
Con::disconnect()
{
  delete m_ndb;
  m_ndb = 0, m_dic = 0, m_tx = 0, m_op = 0;
}

int
Con::startTransaction()
{
  assert(m_ndb != 0 && m_tx == 0);
  CHKCON((m_tx = m_ndb->startTransaction()) != 0, *this);
  return 0;
}

int
Con::startBuddyTransaction(const Con& con)
{
  assert(m_ndb != 0 && m_tx == 0 && con.m_ndb == m_ndb && con.m_tx != 0);
  CHKCON((m_tx = m_ndb->hupp(con.m_tx)) != 0, *this);
  return 0;
}

int
Con::getNdbOperation(const Tab& tab)
{
  assert(m_tx != 0);
  CHKCON((m_op = m_tx->getNdbOperation(tab.m_name)) != 0, *this);
  return 0;
}

int
Con::getNdbScanOperation(const Tab& tab)
{
  assert(m_tx != 0);
  CHKCON((m_op = m_scanop = m_tx->getNdbScanOperation(tab.m_name)) != 0, *this);
  return 0;
}

int
Con::getNdbScanOperation(const ITab& itab, const Tab& tab)
{
  assert(m_tx != 0);
  CHKCON((m_op = m_scanop = m_indexscanop = m_tx->getNdbIndexScanOperation(itab.m_name, tab.m_name)) != 0, *this);
  return 0;
}

int
Con::equal(int num, const char* addr)
{
  assert(m_tx != 0 && m_op != 0);
  CHKCON(m_op->equal(num, addr) == 0, *this);
  return 0;
}

int
Con::getValue(int num, NdbRecAttr*& rec)
{
  assert(m_tx != 0 && m_op != 0);
  CHKCON((rec = m_op->getValue(num, 0)) != 0, *this);
  return 0;
}

int
Con::setValue(int num, const char* addr)
{
  assert(m_tx != 0 && m_op != 0);
  CHKCON(m_op->setValue(num, addr) == 0, *this);
  return 0;
}

int
Con::setBound(int num, int type, const void* value)
{
  assert(m_tx != 0 && m_op != 0);
  CHKCON(m_indexscanop->setBound(num, type, value) == 0, *this);
  return 0;
}

int
Con::execute(ExecType t)
{
  assert(m_tx != 0);
  CHKCON(m_tx->execute(t) == 0, *this);
  return 0;
}

int
Con::openScanRead(unsigned parallelism)
{
  assert(m_tx != 0 && m_op != 0);
  CHKCON((m_resultset = m_scanop->readTuples(parallelism)) != 0, *this);
  return 0;
}

int
Con::openScanExclusive(unsigned parallelism)
{
  assert(m_tx != 0 && m_op != 0);
  CHKCON((m_resultset = m_scanop->readTuplesExclusive(parallelism)) != 0, *this);
  return 0;
}

int
Con::executeScan()
{
  CHKCON(m_tx->execute(NoCommit) == 0, *this);
  return 0;
}

int
Con::nextScanResult()
{
  int ret;
  assert(m_resultset != 0);
  CHKCON((ret = m_resultset->nextResult()) != -1, *this);
  assert(ret == 0 || ret == 1);
  return ret;
}

int
Con::takeOverForUpdate(Con& scan)
{
  assert(m_tx != 0 && scan.m_op != 0);
  CHKCON((m_op = scan.m_resultset->updateTuple(m_tx)) != 0, scan);
  return 0;
}

int
Con::takeOverForDelete(Con& scan)
{
  assert(m_tx != 0 && scan.m_op != 0);
  CHKCON(scan.m_resultset->deleteTuple(m_tx) == 0, scan);
  return 0;
}

void
Con::closeTransaction()
{
  assert(m_ndb != 0 && m_tx != 0);
  m_ndb->closeTransaction(m_tx);
  m_tx = 0, m_op = 0;
}

void
Con::printerror(NdbOut& out)
{
  m_errtype = ErrOther;
  unsigned any = 0;
  int code;
  if (m_ndb) {
    if ((code = m_ndb->getNdbError().code) != 0) {
      LL0(++any << " ndb: error " << m_ndb->getNdbError());
    }
    if (m_dic && (code = m_dic->getNdbError().code) != 0) {
      LL0(++any << " dic: error " << m_dic->getNdbError());
    }
    if (m_tx) {
      if ((code = m_tx->getNdbError().code) != 0) {
        LL0(++any << " con: error " << m_tx->getNdbError());
        if (code == 266 || code == 274 || code == 296 || code == 297)
          m_errtype = ErrDeadlock;
      }
      if (m_op && m_op->getNdbError().code != 0) {
        LL0(++any << " op : error " << m_op->getNdbError());
      }
    }
  }
  if (! any) {
    LL0("failed but no NDB error code");
  }
}

// dictionary operations

static int
invalidateindex(Par par, const ITab& itab)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  con.m_dic->invalidateIndex(itab.m_name, tab.m_name);
  return 0;
}

static int
invalidateindex(Par par)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  for (unsigned i = 0; i < tab.m_itabs; i++) {
    if (! useindex(i))
      continue;
    const ITab& itab = tab.m_itab[i];
    invalidateindex(par, itab);
  }
  return 0;
}

static int
invalidatetable(Par par)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  invalidateindex(par);
  con.m_dic->invalidateTable(tab.m_name);
  return 0;
}

static int
droptable(Par par)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  if (con.m_dic->getTable(tab.m_name) == 0) {
    // how to check for error
    LL4("no table " << tab.m_name);
  } else {
    LL3("drop table " << tab.m_name);
    CHKCON(con.m_dic->dropTable(tab.m_name) == 0, con);
  }
  return 0;
}

static int
createtable(Par par)
{
  Con& con = par.con();
  CHK(con.bugger() == 0);
  const Tab& tab = par.tab();
  LL3("create table " << tab.m_name);
  LL4(tab);
  NdbDictionary::Table t(tab.m_name);
  if (par.m_fragtype != NdbDictionary::Object::FragUndefined) {
    t.setFragmentType(par.m_fragtype);
  }
  if (par.m_nologging) {
    t.setLogging(false);
  }
  for (unsigned k = 0; k < tab.m_cols; k++) {
    const Col& col = tab.m_col[k];
    NdbDictionary::Column c(col.m_name);
    c.setPrimaryKey(col.m_pk);
    c.setType(col.m_type);
    c.setLength(col.m_length);
    c.setNullable(col.m_nullable);
    t.addColumn(c);
  }
  CHKCON(con.m_dic->createTable(t) == 0, con);
  return 0;
}

static int
dropindex(Par par, const ITab& itab)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  if (con.m_dic->getIndex(itab.m_name, tab.m_name) == 0) {
    // how to check for error
    LL4("no index " << itab.m_name);
  } else {
    LL3("drop index " << itab.m_name);
    CHKCON(con.m_dic->dropIndex(itab.m_name, tab.m_name) == 0, con);
  }
  return 0;
}

static int
dropindex(Par par)
{
  const Tab& tab = par.tab();
  for (unsigned i = 0; i < tab.m_itabs; i++) {
    if (! useindex(i))
      continue;
    const ITab& itab = tab.m_itab[i];
    CHK(dropindex(par, itab) == 0);
  }
  return 0;
}

static int
createindex(Par par, const ITab& itab)
{
  Con& con = par.con();
  CHK(con.bugger() == 0);
  const Tab& tab = par.tab();
  LL3("create index " << itab.m_name);
  LL4(itab);
  NdbDictionary::Index x(itab.m_name);
  x.setTable(tab.m_name);
  x.setType(NdbDictionary::Index::OrderedIndex);
  x.setLogging(false);
  for (unsigned k = 0; k < itab.m_icols; k++) {
    const Col& col = itab.m_icol[k].m_col;
    x.addColumnName(col.m_name);
  }
  CHKCON(con.m_dic->createIndex(x) == 0, con);
  return 0;
}

static int
createindex(Par par)
{
  const Tab& tab = par.tab();
  for (unsigned i = 0; i < tab.m_itabs; i++) {
    if (! useindex(i))
      continue;
    const ITab& itab = tab.m_itab[i];
    CHK(createindex(par, itab) == 0);
  }
  return 0;
}

// data sets

static unsigned
urandom(unsigned n)
{
  if (n == 0)
    return 0;
  unsigned i = random() % n;
  return i;
}

static int
irandom(unsigned n)
{
  if (n == 0)
    return 0;
  int i = random() % n;
  if (random() & 0x1)
    i = -i;
  return i;
}

// Val - typed column value

struct Val {
  const Col& m_col;
  union {
  Uint32 m_uint32;
  char* m_varchar;
  };
  Val(const Col& col);
  ~Val();
  void copy(const Val& val2);
  void copy(const void* addr);
  const void* dataaddr() const;
  bool m_null;
  int setval(Par par) const;
  void calc(Par par, unsigned i);
  int verify(const Val& val2) const;
  int cmp(const Val& val2) const;
private:
  Val& operator=(const Val& val2);
};

static NdbOut&
operator<<(NdbOut& out, const Val& val);

Val::Val(const Col& col) :
  m_col(col)
{
  switch (col.m_type) {
  case NdbDictionary::Column::Unsigned:
    break;
  case NdbDictionary::Column::Varchar:
    m_varchar = new char [2 + col.m_length];
    break;
  default:
    assert(false);
    break;
  }
}

Val::~Val()
{
  const Col& col = m_col;
  switch (col.m_type) {
  case NdbDictionary::Column::Unsigned:
    break;
  case NdbDictionary::Column::Varchar:
    delete [] m_varchar;
    break;
  default:
    assert(false);
    break;
  }
}

void
Val::copy(const Val& val2)
{
  const Col& col = m_col;
  const Col& col2 = val2.m_col;
  assert(col.m_type == col2.m_type && col.m_length == col2.m_length);
  if (val2.m_null) {
    m_null = true;
    return;
  }
  copy(val2.dataaddr());
}

void
Val::copy(const void* addr)
{
  const Col& col = m_col;
  switch (col.m_type) {
  case NdbDictionary::Column::Unsigned:
    m_uint32 = *(const Uint32*)addr;
    break;
  case NdbDictionary::Column::Varchar:
    memcpy(m_varchar, addr, 2 + col.m_length);
    break;
  default:
    assert(false);
    break;
  }
  m_null = false;
}

const void*
Val::dataaddr() const
{
  const Col& col = m_col;
  switch (col.m_type) {
  case NdbDictionary::Column::Unsigned:
    return &m_uint32;
  case NdbDictionary::Column::Varchar:
    return m_varchar;
  default:
    break;
  }
  assert(false);
  return 0;
}

int
Val::setval(Par par) const
{
  Con& con = par.con();
  const Col& col = m_col;
  const char* addr = (const char*)dataaddr();
  if (m_null)
    addr = 0;
  if (col.m_pk)
    CHK(con.equal(col.m_num, addr) == 0);
  else
    CHK(con.setValue(col.m_num, addr) == 0);
  LL5("setval [" << m_col << "] " << *this);
  return 0;
}

void
Val::calc(Par par, unsigned i)
{
  const Col& col = m_col;
  m_null = false;
  if (col.m_pk) {
    m_uint32 = i;
    return;
  }
  if (col.m_nullable && urandom(100) < par.m_pctnull) {
    m_null = true;
    return;
  }
  unsigned v = par.m_range + irandom((par.m_pctrange * par.m_range) / 100);
  switch (col.m_type) {
  case NdbDictionary::Column::Unsigned:
    m_uint32 = v;
    break;
  case NdbDictionary::Column::Varchar:
    {
      unsigned n = 0;
      while (n < col.m_length) {
        if (urandom(1 + col.m_length) == 0) {
          // nice distribution on lengths
          break;
        }
        m_varchar[2 + n++] = 'a' + urandom((par.m_pctrange * 10) / 100);
      }
      m_varchar[0] = (n >> 8);
      m_varchar[1] = (n & 0xff);
      while (n < col.m_length) {
        m_varchar[2 + n++] = 0;
      }
    }
    break;
  default:
    assert(false);
    break;
  }
  // verify format
  col.verify(dataaddr());
}

int
Val::verify(const Val& val2) const
{
  CHK(cmp(val2) == 0);
  return 0;
}

int
Val::cmp(const Val& val2) const
{
  const Col& col = m_col;
  const Col& col2 = val2.m_col;
  assert(col.m_type == col2.m_type && col.m_length == col2.m_length);
  if (m_null || val2.m_null) {
    if (! m_null)
      return -1;
    if (! val2.m_null)
      return +1;
    return 0;
  }
  // verify data formats
  col.verify(dataaddr());
  col.verify(val2.dataaddr());
  // compare
  switch (col.m_type) {
  case NdbDictionary::Column::Unsigned:
    if (m_uint32 < val2.m_uint32)
      return -1;
    if (m_uint32 > val2.m_uint32)
      return +1;
    return 0;
  case NdbDictionary::Column::Varchar:
    return memcmp(&m_varchar[2], &val2.m_varchar[2], col.m_length);
  default:
    break;
  }
  assert(false);
  return 0;
}

static NdbOut&
operator<<(NdbOut& out, const Val& val)
{
  const Col& col = val.m_col;
  if (val.m_null) {
    out << "NULL";
    return out;
  }
  switch (col.m_type) {
  case NdbDictionary::Column::Unsigned:
    out << val.m_uint32;
    break;
  case NdbDictionary::Column::Varchar:
    {
      char buf[8000];
      unsigned n = (val.m_varchar[0] << 8) | val.m_varchar[1];
      assert(n <= col.m_length);
      sprintf(buf, "'%.*s'[%d]", n, &val.m_varchar[2], n);
      out << buf;
    }
    break;
  default:
    out << "type" << col.m_type;
    assert(false);
    break;
  }
  return out;
}

// Row - table tuple

struct Row {
  const Tab& m_tab;
  Val** m_val;
  bool m_exist;
  Row(const Tab& tab);
  ~Row();
  void copy(const Row& row2);
  void calc(Par par, unsigned i);
  int verify(const Row& row2) const;
  int insrow(Par par);
  int updrow(Par par);
  int delrow(Par par);
  int selrow(Par par);
  int setrow(Par par);
  int cmp(const Row& row2) const;
private:
  Row& operator=(const Row& row2);
};

Row::Row(const Tab& tab) :
  m_tab(tab)
{
  m_val = new Val* [tab.m_cols];
  for (unsigned k = 0; k < tab.m_cols; k++) {
    const Col& col = tab.m_col[k];
    m_val[k] = new Val(col);
  }
  m_exist = false;
}

Row::~Row()
{
  const Tab& tab = m_tab;
  for (unsigned k = 0; k < tab.m_cols; k++) {
    delete m_val[k];
  }
  delete [] m_val;
}

void
Row::copy(const Row& row2)
{
  const Tab& tab = m_tab;
  assert(&tab == &row2.m_tab);
  for (unsigned k = 0; k < tab.m_cols; k++) {
    Val& val = *m_val[k];
    const Val& val2 = *row2.m_val[k];
    val.copy(val2);
  }
}

void
Row::calc(Par par, unsigned i)
{
  const Tab& tab = m_tab;
  for (unsigned k = 0; k < tab.m_cols; k++) {
    Val& val = *m_val[k];
    val.calc(par, i);
  }
}

int
Row::verify(const Row& row2) const
{
  const Tab& tab = m_tab;
  assert(&tab == &row2.m_tab);
  for (unsigned k = 0; k < tab.m_cols; k++) {
    const Val& val = *m_val[k];
    const Val& val2 = *row2.m_val[k];
    CHK(val.verify(val2) == 0);
  }
  return 0;
}

int
Row::insrow(Par par)
{
  Con& con = par.con();
  const Tab& tab = m_tab;
  assert(! m_exist);
  CHK(con.getNdbOperation(tab) == 0);
  CHKCON(con.m_op->insertTuple() == 0, con);
  for (unsigned k = 0; k < tab.m_cols; k++) {
    const Val& val = *m_val[k];
    CHK(val.setval(par) == 0);
  }
  m_exist = true;
  return 0;
}

int
Row::updrow(Par par)
{
  Con& con = par.con();
  const Tab& tab = m_tab;
  assert(m_exist);
  CHK(con.getNdbOperation(tab) == 0);
  CHKCON(con.m_op->updateTuple() == 0, con);
  for (unsigned k = 0; k < tab.m_cols; k++) {
    const Val& val = *m_val[k];
    CHK(val.setval(par) == 0);
  }
  return 0;
}

int
Row::delrow(Par par)
{
  Con& con = par.con();
  const Tab& tab = m_tab;
  assert(m_exist);
  CHK(con.getNdbOperation(m_tab) == 0);
  CHKCON(con.m_op->deleteTuple() == 0, con);
  for (unsigned k = 0; k < tab.m_cols; k++) {
    const Val& val = *m_val[k];
    const Col& col = val.m_col;
    if (col.m_pk)
      CHK(val.setval(par) == 0);
  }
  m_exist = false;
  return 0;
}

int
Row::selrow(Par par)
{
  Con& con = par.con();
  const Tab& tab = m_tab;
  CHK(con.getNdbOperation(m_tab) == 0);
  CHKCON(con.m_op->readTuple() == 0, con);
  for (unsigned k = 0; k < tab.m_cols; k++) {
    const Val& val = *m_val[k];
    const Col& col = val.m_col;
    if (col.m_pk)
      CHK(val.setval(par) == 0);
  }
  m_exist = false;
  return 0;
}

int
Row::setrow(Par par)
{
  Con& con = par.con();
  const Tab& tab = m_tab;
  for (unsigned k = 0; k < tab.m_cols; k++) {
    const Val& val = *m_val[k];
    const Col& col = val.m_col;
    if (! col.m_pk)
      CHK(val.setval(par) == 0);
  }
  return 0;
}

int
Row::cmp(const Row& row2) const
{
  const Tab& tab = m_tab;
  assert(&tab == &row2.m_tab);
  int c = 0;
  for (unsigned k = 0; k < tab.m_cols; k++) {
    const Val& val = *m_val[k];
    const Val& val2 = *row2.m_val[k];
    if ((c = val.cmp(val2)) != 0)
      break;
  }
  return c;
}

static NdbOut&
operator<<(NdbOut& out, const Row& row)
{
  const Tab& tab = row.m_tab;
  for (unsigned i = 0; i < tab.m_cols; i++) {
    if (i > 0)
      out << " ";
    out << *row.m_val[i];
  }
  return out;
}

// Set - set of table tuples

struct Set {
  const Tab& m_tab;
  unsigned m_rows;
  unsigned m_count;
  Row** m_row;
  Row** m_saverow;
  Row* m_keyrow;
  NdbRecAttr** m_rec;
  Set(const Tab& tab, unsigned rows);
  ~Set();
  // row methods
  bool exist(unsigned i) const;
  void calc(Par par, unsigned i);
  int insrow(Par par, unsigned i);
  int updrow(Par par, unsigned i);
  int delrow(Par par, unsigned i);
  int selrow(Par par, unsigned i);
  int setrow(Par par, unsigned i);
  int getval(Par par);
  int getkey(Par par, unsigned* i);
  int putval(unsigned i, bool force);
  // set methods
  int verify(const Set& set2) const;
  void savepoint();
  void commit();
  void rollback();
  // locking (not perfect since ops may complete in different order)
  NdbMutex* m_mutex;
  void lock() {
    NdbMutex_Lock(m_mutex);
  }
  void unlock() {
    NdbMutex_Unlock(m_mutex);
  }
private:
  Set& operator=(const Set& set2);
};

Set::Set(const Tab& tab, unsigned rows) :
  m_tab(tab)
{
  m_rows = rows;
  m_count = 0;
  m_row = new Row* [m_rows];
  for (unsigned i = 0; i < m_rows; i++) {
    m_row[i] = 0;
  }
  m_saverow = 0;
  m_keyrow = new Row(tab);
  m_rec = new NdbRecAttr* [tab.m_cols];
  for (unsigned k = 0; k < tab.m_cols; k++) {
    m_rec[k] = 0;
  }
  m_mutex = NdbMutex_Create();
  assert(m_mutex != 0);
}

Set::~Set()
{
  for (unsigned i = 0; i < m_rows; i++) {
    delete m_row[i];
    if (m_saverow != 0)
      delete m_saverow[i];
  }
  delete [] m_row;
  delete [] m_saverow;
  delete m_keyrow;
  delete [] m_rec;
  NdbMutex_Destroy(m_mutex);
}

bool
Set::exist(unsigned i) const
{
  assert(i < m_rows);
  return m_row[i] != 0 && m_row[i]->m_exist;
}

void
Set::calc(Par par, unsigned i)
{
  const Tab& tab = m_tab;
  if (m_row[i] == 0)
    m_row[i] = new Row(tab);
  Row& row = *m_row[i];
  // value generation parameters
  par.m_pctnull = 10;
  par.m_pctrange = 40;
  row.calc(par, i);
}

int
Set::insrow(Par par, unsigned i)
{
  assert(m_row[i] != 0 && m_count < m_rows);
  CHK(m_row[i]->insrow(par) == 0);
  m_count++;
  return 0;
}

int
Set::updrow(Par par, unsigned i)
{
  assert(m_row[i] != 0);
  CHK(m_row[i]->updrow(par) == 0);
  return 0;
}

int
Set::delrow(Par par, unsigned i)
{
  assert(m_row[i] != 0 && m_count != 0);
  CHK(m_row[i]->delrow(par) == 0);
  m_count--;
  return 0;
}

int
Set::selrow(Par par, unsigned i)
{
  Con& con = par.con();
  m_keyrow->calc(par, i);
  CHK(m_keyrow->selrow(par) == 0);
  CHK(getval(par) == 0);
  return 0;
}

int
Set::setrow(Par par, unsigned i)
{
  Con& con = par.con();
  assert(m_row[i] != 0);
  CHK(m_row[i]->setrow(par) == 0);
  return 0;
}

int
Set::getval(Par par)
{
  Con& con = par.con();
  const Tab& tab = m_tab;
  for (unsigned k = 0; k < tab.m_cols; k++) {
    CHK(con.getValue(k, m_rec[k]) == 0);
  }
  return 0;
}

int
Set::getkey(Par par, unsigned* i)
{
  assert(m_rec[0] != 0);
  const char* aRef0 = m_rec[0]->aRef();
  Uint32 key = *(const Uint32*)aRef0;
  CHKMSG(key < m_rows, "key=" << key << " rows=" << m_rows);
  *i = key;
  return 0;
}

int
Set::putval(unsigned i, bool force)
{
  const Tab& tab = m_tab;
  if (m_row[i] == 0)
    m_row[i] = new Row(tab);
  Row& row = *m_row[i];
  CHK(! row.m_exist || force);
  for (unsigned k = 0; k < tab.m_cols; k++) {
    Val& val = *row.m_val[k];
    NdbRecAttr* rec = m_rec[k];
    assert(rec != 0);
    if (rec->isNULL()) {
      val.m_null = true;
      continue;
    }
    const char* aRef = m_rec[k]->aRef();
    val.copy(aRef);
    val.m_null = false;
  }
  if (! row.m_exist) {
    row.m_exist = true;
    m_count++;
  }
  return 0;
}

int
Set::verify(const Set& set2) const
{
  const Tab& tab = m_tab;
  assert(&tab == &set2.m_tab && m_rows == set2.m_rows);
  CHKMSG(m_count == set2.m_count, "set=" << m_count << " set2=" << set2.m_count);
  for (unsigned i = 0; i < m_rows; i++) {
    CHK(exist(i) == set2.exist(i));
    if (! exist(i))
      continue;
    Row& row = *m_row[i];
    Row& row2 = *set2.m_row[i];
    CHK(row.verify(row2) == 0);
  }
  return 0;
}

void
Set::savepoint()
{
  const Tab& tab = m_tab;
  assert(m_saverow == 0);
  m_saverow = new Row* [m_rows];
  for (unsigned i = 0; i < m_rows; i++) {
    if (m_row[i] == 0)
      m_saverow[i] = 0;
    else {
      m_saverow[i] = new Row(tab);
      m_saverow[i]->copy(*m_row[i]);
    }
  }
}

void
Set::commit()
{
  delete [] m_saverow;
  m_saverow = 0;
}

void
Set::rollback()
{
  assert(m_saverow != 0);
  m_row = m_saverow;
  m_saverow = 0;
}

static NdbOut&
operator<<(NdbOut& out, const Set& set)
{
  for (unsigned i = 0; i < set.m_rows; i++) {
    const Row& row = *set.m_row[i];
    if (i > 0)
      out << endl;
    out << row;
  }
  return out;
}

// BVal - range scan bound

struct BVal : public Val {
  const ICol& m_icol;
  int m_type;
  BVal(const ICol& icol);
  int setbnd(Par par) const;
};

BVal::BVal(const ICol& icol) :
  Val(icol.m_col),
  m_icol(icol)
{
}

int
BVal::setbnd(Par par) const
{
  Con& con = par.con();
  const char* addr = (const char*)dataaddr();
  assert(! m_null);
  const ICol& icol = m_icol;
  CHK(con.setBound(icol.m_num, m_type, addr) == 0);
  return 0;
}

static NdbOut&
operator<<(NdbOut& out, const BVal& bval)
{
  const ICol& icol = bval.m_icol;
  const Col& col = icol.m_col;
  const Val& val = bval;
  out << "type " << bval.m_type;
  out << " icol " << icol.m_num;
  out << " col " << col.m_name << "(" << col.m_num << ")";
  out << " value " << val;
  return out;
}

// BSet - set of bounds

struct BSet {
  const Tab& m_tab;
  const ITab& m_itab;
  unsigned m_alloc;
  unsigned m_bvals;
  BVal** m_bval;
  BSet(const Tab& tab, const ITab& itab, unsigned rows);
  void calc(Par par);
  int setbnd(Par par) const;
  void filter(const Set& set, Set& set2) const;
};

BSet::BSet(const Tab& tab, const ITab& itab, unsigned rows) :
  m_tab(tab),
  m_itab(itab),
  m_alloc(2 * itab.m_icols),
  m_bvals(0)
{
  m_bval = new BVal* [m_alloc];
}

void
BSet::calc(Par par)
{
  const ITab& itab = m_itab;
  for (unsigned k = 0; k < itab.m_icols; k++) {
    const ICol& icol = itab.m_icol[k];
    const Col& col = icol.m_col;
    for (unsigned i = 0; i <= 1; i++) {
      if (urandom(10) == 0)
        return;
      assert(m_bvals < m_alloc);
      BVal& bval = *new BVal(icol);
      m_bval[m_bvals++] = &bval;
      bval.m_null = false;
      // equality bound only on i==0
      unsigned sel = urandom(5 - i);
      if (sel < 2)
        bval.m_type = 0 | (1 << i);
      else if (sel < 4)
        bval.m_type = 1 | (1 << i);
      else
        bval.m_type = 4;
      if (k + 1 < itab.m_icols)
        bval.m_type = 4;
      // value generation parammeters
      par.m_pctnull = 0;
      par.m_pctrange = 50;      // bit higher
      do {
        bval.calc(par, 0);
        if (i == 1) {
          assert(m_bvals >= 2);
          const BVal& bv1 = *m_bval[m_bvals - 2];
          const BVal& bv2 = *m_bval[m_bvals - 1];
          if (bv1.cmp(bv2) > 0 && urandom(100) != 0)
            continue;
        }
      } while (0);
      // equality bound only once
      if (bval.m_type == 4)
        break;
    }
  }
}

int
BSet::setbnd(Par par) const
{
  for (unsigned j = 0; j < m_bvals; j++) {
    const BVal& bval = *m_bval[j];
    CHK(bval.setbnd(par) == 0);
  }
  return 0;
}

void
BSet::filter(const Set& set, Set& set2) const
{
  const Tab& tab = m_tab;
  const ITab& itab = m_itab;
  assert(&tab == &set2.m_tab && set.m_rows == set2.m_rows);
  assert(set2.m_count == 0);
  for (unsigned i = 0; i < set.m_rows; i++) {
    if (! set.exist(i))
      continue;
    const Row& row = *set.m_row[i];
    bool ok1 = false;
    for (unsigned k = 0; k < itab.m_icols; k++) {
      const ICol& icol = itab.m_icol[k];
      const Col& col = icol.m_col;
      const Val& val = *row.m_val[col.m_num];
      if (! val.m_null) {
        ok1 = true;
        break;
      }
    }
    if (! ok1)
      continue;
    bool ok2 = true;
    for (unsigned j = 0; j < m_bvals; j++) {
      const BVal& bval = *m_bval[j];
      const ICol& icol = bval.m_icol;
      const Col& col = icol.m_col;
      const Val& val = *row.m_val[col.m_num];
      int ret = bval.cmp(val);
      if (bval.m_type == 0)
        ok2 = (ret <= 0);
      else if (bval.m_type == 1)
        ok2 = (ret < 0);
      else if (bval.m_type == 2)
        ok2 = (ret >= 0);
      else if (bval.m_type == 3)
        ok2 = (ret > 0);
      else if (bval.m_type == 4)
        ok2 = (ret == 0);
      else {
        assert(false);
      }
      if (! ok2)
        break;
    }
    if (! ok2)
      continue;
    if (set2.m_row[i] == 0)
      set2.m_row[i] = new Row(tab);
    Row& row2 = *set2.m_row[i];
    assert(! row2.m_exist);
    row2.copy(row);
    row2.m_exist = true;
    set2.m_count++;
  }
}

static NdbOut&
operator<<(NdbOut& out, const BSet& bset)
{
  out << "bounds=" << bset.m_bvals;
  for (unsigned j = 0; j < bset.m_bvals; j++) {
    out << endl;
    const BVal& bval = *bset.m_bval[j];
    out << "bound " << j << ": " << bval;
  }
  return out;
}

// pk operations

static int
pkinsert(Par par)
{
  Con& con = par.con();
  Set& set = par.set();
  LL3("pkinsert");
  CHK(con.startTransaction() == 0);
  unsigned n = 0;
  for (unsigned j = 0; j < par.m_rows; j++) {
    unsigned i = thrrow(par, j);
    set.lock();
    if (set.exist(i)) {
      set.unlock();
      continue;
    }
    set.calc(par, i);
    LL4("pkinsert " << i << ": " << *set.m_row[i]);
    CHKTRY(set.insrow(par, i) == 0, set.unlock());
    set.unlock();
    if (++n == par.m_batch) {
      CHK(con.execute(Commit) == 0);
      con.closeTransaction();
      CHK(con.startTransaction() == 0);
      n = 0;
    }
  }
  if (n != 0) {
    CHK(con.execute(Commit) == 0);
    n = 0;
  }
  con.closeTransaction();
  return 0;
};

static int
pkupdate(Par par)
{
  Con& con = par.con();
  Set& set = par.set();
  LL3("pkupdate");
  CHK(con.startTransaction() == 0);
  unsigned n = 0;
  for (unsigned j = 0; j < par.m_rows; j++) {
    unsigned i = thrrow(par, j);
    set.lock();
    if (! set.exist(i)) {
      set.unlock();
      continue;
    }
    set.calc(par, i);
    LL4("pkupdate " << i << ": " << *set.m_row[i]);
    CHKTRY(set.updrow(par, i) == 0, set.unlock());
    set.unlock();
    if (++n == par.m_batch) {
      CHK(con.execute(Commit) == 0);
      con.closeTransaction();
      CHK(con.startTransaction() == 0);
      n = 0;
    }
  }
  if (n != 0) {
    CHK(con.execute(Commit) == 0);
    n = 0;
  }
  con.closeTransaction();
  return 0;
};

static int
pkdelete(Par par)
{
  Con& con = par.con();
  Set& set = par.set();
  LL3("pkdelete");
  CHK(con.startTransaction() == 0);
  unsigned n = 0;
  for (unsigned j = 0; j < par.m_rows; j++) {
    unsigned i = thrrow(par, j);
    set.lock();
    if (! set.exist(i)) {
      set.unlock();
      continue;
    }
    LL4("pkdelete " << i << ": " << *set.m_row[i]);
    CHKTRY(set.delrow(par, i) == 0, set.unlock());
    set.unlock();
    if (++n == par.m_batch) {
      CHK(con.execute(Commit) == 0);
      con.closeTransaction();
      CHK(con.startTransaction() == 0);
      n = 0;
    }
  }
  if (n != 0) {
    CHK(con.execute(Commit) == 0);
    n = 0;
  }
  con.closeTransaction();
  return 0;
};

static int
pkread(Par par)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  const Set& set = par.set();
  LL3((par.m_verify ? "pkverify " : "pkread ") << tab.m_name);
  // expected
  const Set& set1 = set;
  Set set2(tab, set.m_rows);
  for (unsigned i = 0; i < set.m_rows; i++) {
    if (! set.exist(i))
      continue;
    CHK(con.startTransaction() == 0);
    CHK(set2.selrow(par, i) == 0);
    CHK(con.execute(Commit) == 0);
    unsigned i2 = (unsigned)-1;
    CHK(set2.getkey(par, &i2) == 0 && i == i2);
    CHK(set2.putval(i, false) == 0);
    LL4("row " << set2.m_count << ": " << *set2.m_row[i]);
    con.closeTransaction();
  }
  if (par.m_verify)
    CHK(set1.verify(set2) == 0);
  return 0;
}

// scan read

static int
scanreadtable(Par par)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  const Set& set = par.set();
  // expected
  const Set& set1 = set;
  LL3((par.m_verify ? "scanverify " : "scanread ") << tab.m_name);
  Set set2(tab, set.m_rows);
  CHK(con.startTransaction() == 0);
  CHK(con.getNdbScanOperation(tab) == 0);
  CHK(con.openScanRead(par.m_scanrd) == 0);
  set2.getval(par);
  CHK(con.executeScan() == 0);
  while (1) {
    int ret;
    CHK((ret = con.nextScanResult()) == 0 || ret == 1);
    if (ret == 1)
      break;
    unsigned i = (unsigned)-1;
    CHK(set2.getkey(par, &i) == 0);
    CHK(set2.putval(i, false) == 0);
    LL4("row " << set2.m_count << ": " << *set2.m_row[i]);
  }
  con.closeTransaction();
  if (par.m_verify)
    CHK(set1.verify(set2) == 0);
  return 0;
}

static int
scanreadindex(Par par, const ITab& itab, const BSet& bset)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  const Set& set = par.set();
  // expected
  Set set1(tab, set.m_rows);
  bset.filter(set, set1);
  LL3((par.m_verify ? "scanverify " : "scanread ") << itab.m_name << " bounds=" << bset.m_bvals);
  LL4(bset);
  Set set2(tab, set.m_rows);
  CHK(con.startTransaction() == 0);
  CHK(con.getNdbScanOperation(itab, tab) == 0);
  CHK(con.openScanRead(par.m_scanrd) == 0);
  CHK(bset.setbnd(par) == 0);
  set2.getval(par);
  CHK(con.executeScan() == 0);
  while (1) {
    int ret;
    CHK((ret = con.nextScanResult()) == 0 || ret == 1);
    if (ret == 1)
      break;
    unsigned i = (unsigned)-1;
    CHK(set2.getkey(par, &i) == 0);
    LL4("key " << i);
    CHK(set2.putval(i, par.m_dups) == 0);
    LL4("row " << set2.m_count << ": " << *set2.m_row[i]);
  }
  con.closeTransaction();
  if (par.m_verify)
    CHK(set1.verify(set2) == 0);
  return 0;
}

static int
scanreadindex(Par par, const ITab& itab)
{
  const Tab& tab = par.tab();
  for (unsigned i = 0; i < par.m_subloop; i++) {
    BSet bset(tab, itab, par.m_rows);
    bset.calc(par);
    CHK(scanreadindex(par, itab, bset) == 0);
  }
  return 0;
}

static int
scanreadindex(Par par)
{
  const Tab& tab = par.tab();
  for (unsigned i = 0; i < tab.m_itabs; i++) {
    if (! useindex(i))
      continue;
    const ITab& itab = tab.m_itab[i];
    CHK(scanreadindex(par, itab) == 0);
  }
  return 0;
}

static int
scanreadall(Par par)
{
  if (par.m_no < 11)
    CHK(scanreadtable(par) == 0);
  CHK(scanreadindex(par) == 0);
  return 0;
}

// scan update

static int
scanupdatetable(Par par)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  Set& set = par.set();
  LL3("scan update " << tab.m_name);
  Set set2(tab, set.m_rows);
  CHK(con.startTransaction() == 0);
  CHK(con.getNdbScanOperation(tab) == 0);
  CHK(con.openScanExclusive(par.m_scanex) == 0);
  set2.getval(par);
  CHK(con.executeScan() == 0);
  unsigned count = 0;
  // updating trans
  Con con2;
  con2.m_ndb = con.m_ndb;
  CHK(con2.startBuddyTransaction(con) == 0);
  while (1) {
    int ret;
    CHK((ret = con.nextScanResult()) == 0 || ret == 1);
    if (ret == 1)
      break;
    unsigned i = (unsigned)-1;
    CHK(set2.getkey(par, &i) == 0);
    LL4("key " << i);
    CHK(set2.putval(i, false) == 0);
    CHK(con2.takeOverForUpdate(con) == 0);
    Par par2 = par;
    par2.m_con = &con2;
    set.lock();
    set.calc(par, i);
    LL4("scan update " << tab.m_name << ": " << *set.m_row[i]);
    CHKTRY(set.setrow(par2, i) == 0, set.unlock());
    set.unlock();
    CHK(con2.execute(NoCommit) == 0);
    count++;
  }
  CHK(con2.execute(Commit) == 0);
  con2.closeTransaction();
  LL3("scan update " << tab.m_name << " rows updated=" << count);
  con.closeTransaction();
  return 0;
}

static int
scanupdateindex(Par par, const ITab& itab, const BSet& bset)
{
  Con& con = par.con();
  const Tab& tab = par.tab();
  Set& set = par.set();
  LL3("scan update " << itab.m_name);
  Set set2(tab, set.m_rows);
  CHK(con.startTransaction() == 0);
  CHK(con.getNdbScanOperation(itab, tab) == 0);
  CHK(con.openScanExclusive(par.m_scanex) == 0);
  CHK(bset.setbnd(par) == 0);
  set2.getval(par);
  CHK(con.executeScan() == 0);
  unsigned count = 0;
  // updating trans
  Con con2;
  con2.m_ndb = con.m_ndb;
  CHK(con2.startBuddyTransaction(con) == 0);
  while (1) {
    int ret;
    CHK((ret = con.nextScanResult()) == 0 || ret == 1);
    if (ret == 1)
      break;
    unsigned i = (unsigned)-1;
    CHK(set2.getkey(par, &i) == 0);
    LL4("key " << i);
    CHK(set2.putval(i, par.m_dups) == 0);
    // avoid deadlock for now
    //if (! isthrrow(par, i))
      //continue;
    CHK(con2.takeOverForUpdate(con) == 0);
    Par par2 = par;
    par2.m_con = &con2;
    set.lock();
    set.calc(par, i);
    LL4("scan update " << itab.m_name << ": " << *set.m_row[i]);
    CHKTRY(set.setrow(par2, i) == 0, set.unlock());
    set.unlock();
    CHK(con2.execute(NoCommit) == 0);
    count++;
  }
  CHK(con2.execute(Commit) == 0);
  con2.closeTransaction();
  LL3("scan update " << itab.m_name << " rows updated=" << count);
  con.closeTransaction();
  return 0;
}

static int
scanupdateindex(Par par, const ITab& itab)
{
  const Tab& tab = par.tab();
  for (unsigned i = 0; i < par.m_subloop; i++) {
    BSet bset(tab, itab, par.m_rows);
    bset.calc(par);
    CHK(scanupdateindex(par, itab, bset) == 0);
  }
  return 0;
}

static int
scanupdateindex(Par par)
{
  const Tab& tab = par.tab();
  for (unsigned i = 0; i < tab.m_itabs; i++) {
    if (! useindex(i))
      continue;
    const ITab& itab = tab.m_itab[i];
    CHK(scanupdateindex(par, itab) == 0);
  }
  return 0;
}

static int
scanupdateall(Par par)
{
  CHK(scanupdatetable(par) == 0);
  CHK(scanupdateindex(par) == 0);
  return 0;
}

// medium level routines

static bool
ignoreverifyerror(Par par)
{
  Con& con = par.con();
  bool b = par.m_threads > 1;
  if (b) {
    LL1("ignore verify error");
    if (con.m_tx != 0)
      con.closeTransaction();
    return true;
  }
  return b;
}

static int
readverify(Par par)
{
  par.m_verify = true;
  CHK(pkread(par) == 0 || ignoreverifyerror(par));
  CHK(scanreadall(par) == 0 || ignoreverifyerror(par));
  return 0;
}

static bool
ignoredeadlock(Par par)
{
  Con& con = par.con();
  if (con.m_errtype == Con::ErrDeadlock) {
    LL1("ignore deadlock");
    con.closeTransaction();
    return true;
  }
  return false;
}

static int
pkupdatescanread(Par par)
{
  par.m_dups = true;
  unsigned sel = urandom(10);
  if (sel < 5) {
    CHK(pkupdate(par) == 0);
  } else if (sel < 6) {
    par.m_verify = false;
    CHK(scanreadtable(par) == 0);
  } else {
    par.m_verify = false;
    CHK(scanreadindex(par) == 0);
  }
  return 0;
}

static int
mixedoperations(Par par)
{
  par.m_dups = true;
  unsigned sel = urandom(10);
  if (sel < 2) {
    CHK(pkdelete(par) == 0 || ignoredeadlock(par));
  } else if (sel < 4) {
    CHK(pkupdate(par) == 0 || ignoredeadlock(par));
  } else if (sel < 6) {
    CHK(scanupdatetable(par) == 0 || ignoredeadlock(par));
  } else {
    CHK(scanupdateindex(par) == 0 || ignoredeadlock(par));
  }
  return 0;
}

static int
pkupdateindexbuild(Par par)
{
  if (par.m_no == 0) {
    CHK(createindex(par) == 0);
  } else {
    CHK(pkupdate(par) == 0);
  }
  return 0;
}

// threads

typedef int (*TFunc)(Par par);
enum TMode { ST = 1, MT = 2 };

extern "C" { static void* runthread(void* arg); }

struct Thr {
  enum State { Wait, Start, Stop, Stopped, Exit };
  State m_state;
  Par m_par;
  Uint64 m_id;
  NdbThread* m_thread;
  NdbMutex* m_mutex;
  NdbCondition* m_cond;
  TFunc m_func;
  int m_ret;
  void* m_status;
  Thr(Par par, unsigned n);
  ~Thr();
  int run();
  void start();
  void stop();
  void stopped();
  void exit();
  //
  void lock() {
    NdbMutex_Lock(m_mutex);
  }
  void unlock() {
    NdbMutex_Unlock(m_mutex);
  }
  void wait() {
    NdbCondition_Wait(m_cond, m_mutex);
  }
  void signal() {
    NdbCondition_Signal(m_cond);
  }
  void join() {
    NdbThread_WaitFor(m_thread, &m_status);
    m_thread = 0;
  }
};

Thr::Thr(Par par, unsigned n) :
  m_state(Wait),
  m_par(par),
  m_id(0),
  m_thread(0),
  m_mutex(0),
  m_cond(0),
  m_func(0),
  m_ret(0),
  m_status(0)
{
  m_par.m_no = n;
  char buf[10];
  sprintf(buf, "thr%03u", par.m_no);
  const char* name = strcpy(new char[10], buf);
  // mutex
  m_mutex = NdbMutex_Create();
  m_cond = NdbCondition_Create();
  assert(m_mutex != 0 && m_cond != 0);
  // run
  const unsigned stacksize = 256 * 1024;
  const NDB_THREAD_PRIO prio = NDB_THREAD_PRIO_LOW;
  m_thread = NdbThread_Create(runthread, (void**)this, stacksize, name, prio);
}

Thr::~Thr()
{
  if (m_thread != 0) {
    NdbThread_Destroy(&m_thread);
    m_thread = 0;
  }
  if (m_cond != 0) {
    NdbCondition_Destroy(m_cond);
    m_cond = 0;
  }
  if (m_mutex != 0) {
    NdbMutex_Destroy(m_mutex);
    m_mutex = 0;
  }
}

static void*
runthread(void* arg)
{
  Thr& thr = *(Thr*)arg;
  thr.m_id = (Uint64)pthread_self();
  if (thr.run() < 0) {
    LL1("exit on error");
  } else {
    LL4("exit ok");
  }
  return 0;
}

int
Thr::run()
{
  LL4("run");
  Con con;
  CHK(con.connect() == 0);
  m_par.m_con = &con;
  LL4("connected");
  while (1) {
    lock();
    while (m_state != Start && m_state != Exit) {
      LL4("wait");
      wait();
    }
    if (m_state == Exit) {
      LL4("exit");
      unlock();
      break;
    }
    LL4("start");
    CHK(con.bugger() == 0);
    assert(m_state == Start);
    m_ret = (*m_func)(m_par);
    m_state = Stopped;
    LL4("stop");
    signal();
    unlock();
    CHK(m_ret == 0);
  }
  con.disconnect();
  return 0;
}

void
Thr::start()
{
  lock();
  m_state = Start;
  signal();
  unlock();
}

void
Thr::stop()
{
  lock();
  m_state = Stop;
  signal();
  unlock();
}

void
Thr::stopped()
{
  lock();
  while (m_state != Stopped)
    wait();
  m_state = Wait;
  unlock();
}

void
Thr::exit()
{
  lock();
  m_state = Exit;
  signal();
  unlock();
}

// test run

static Thr** g_thrlist = 0;

static unsigned
getthrno()
{
  if (g_thrlist != 0) {
    Uint64 id = (Uint64)pthread_self();
    for (unsigned n = 0; n < g_opt.m_threads; n++) {
      if (g_thrlist[n] != 0) {
        const Thr& thr = *g_thrlist[n];
        if (thr.m_id == id)
          return thr.m_par.m_no;
      }
    }
  }
  return (unsigned)-1;
}

static int
runstep(Par par, const char* fname, TFunc func, unsigned mode)
{
  LL2(fname);
  const int threads = (mode & ST ? 1 : par.m_threads);
  for (int n = 0; n < threads; n++) {
    LL4("start " << n);
    Thr& thr = *g_thrlist[n];
    thr.m_par.m_tab = par.m_tab;
    thr.m_par.m_set = par.m_set;
    thr.m_func = func;
    thr.start();
  }
  unsigned errs = 0;
  for (int n = threads - 1; n >= 0; n--) {
    LL4("stop " << n);
    Thr& thr = *g_thrlist[n];
    thr.stopped();
    if (thr.m_ret != 0)
      errs++;
  }
  CHK(errs == 0);
  return 0;
}

#define RUNSTEP(par, func, mode) CHK(runstep(par, #func, func, mode) == 0)

static int
tbuild(Par par)
{
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  for (unsigned i = 0; i < par.m_subloop; i++) {
    if (i % 2 == 0) {
      RUNSTEP(par, createindex, ST);
      RUNSTEP(par, invalidateindex, MT);
      RUNSTEP(par, pkinsert, MT);
    } else {
      RUNSTEP(par, pkinsert, MT);
      RUNSTEP(par, createindex, ST);
      RUNSTEP(par, invalidateindex, MT);
    }
    RUNSTEP(par, readverify, MT);
    RUNSTEP(par, pkdelete, MT);
    RUNSTEP(par, readverify, MT);
    RUNSTEP(par, dropindex, ST);
  }
  return 0;
}

static int
tpkops(Par par)
{
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  RUNSTEP(par, pkinsert, MT);
  RUNSTEP(par, createindex, ST);
  RUNSTEP(par, invalidateindex, MT);
  RUNSTEP(par, readverify, MT);
  for (unsigned i = 0; i < par.m_subloop; i++) {
    RUNSTEP(par, pkupdatescanread, MT);
    RUNSTEP(par, readverify, MT);
  }
  RUNSTEP(par, pkdelete, MT);
  RUNSTEP(par, readverify, MT);
  return 0;
}

static int
tmixedops(Par par)
{
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  RUNSTEP(par, pkinsert, MT);
  RUNSTEP(par, createindex, ST);
  RUNSTEP(par, invalidateindex, MT);
  RUNSTEP(par, readverify, MT);
  for (unsigned i = 0; i < par.m_subloop; i++) {
    RUNSTEP(par, mixedoperations, MT);
    RUNSTEP(par, readverify, MT);
  }
  return 0;
}

static int
tbusybuild(Par par)
{
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  RUNSTEP(par, pkinsert, MT);
  for (unsigned i = 0; i < par.m_subloop; i++) {
    RUNSTEP(par, pkupdateindexbuild, MT);
    RUNSTEP(par, invalidateindex, MT);
    RUNSTEP(par, readverify, ST);
    RUNSTEP(par, dropindex, ST);
  }
  return 0;
}

static int
ttimebuild(Par par)
{
  Tmr t1;
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  for (unsigned i = 0; i < par.m_subloop; i++) {
    RUNSTEP(par, pkinsert, MT);
    t1.on();
    RUNSTEP(par, createindex, ST);
    t1.off(par.m_totrows);
    RUNSTEP(par, invalidateindex, MT);
    RUNSTEP(par, dropindex, ST);
  }
  LL1("build index - " << t1.time());
  return 0;
}

static int
ttimemaint(Par par)
{
  Tmr t1, t2;
  RUNSTEP(par, droptable, ST);
  RUNSTEP(par, createtable, ST);
  RUNSTEP(par, invalidatetable, MT);
  for (unsigned i = 0; i < par.m_subloop; i++) {
    RUNSTEP(par, pkinsert, MT);
    t1.on();
    RUNSTEP(par, pkupdate, MT);
    t1.off(par.m_totrows);
    RUNSTEP(par, createindex, ST);
    RUNSTEP(par, invalidateindex, MT);
    RUNSTEP(par, readverify, ST);
    t2.on();
    RUNSTEP(par, pkupdate, MT);
    t2.off(par.m_totrows);
    RUNSTEP(par, readverify, ST);
    RUNSTEP(par, dropindex, ST);
  }
  LL1("update - " << t1.time());
  LL1("update indexed - " << t2.time());
  LL1("overhead - " << t2.over(t1));
  return 0;
}

static int
tdrop(Par par)
{
  RUNSTEP(par, droptable, ST);
  return 0;
}

struct TCase {
  const char* m_name;
  TFunc m_func;
  const char* m_desc;
  TCase(const char* name, TFunc func, const char* desc) :
    m_name(name),
    m_func(func),
    m_desc(desc) {
  }
};

static const TCase
tcaselist[] = {
  TCase("a", tbuild, "index build"),
  TCase("b", tpkops, "pk operations and scan reads"),
  TCase("c", tmixedops, "pk operations and scan operations"),
  TCase("d", tbusybuild, "pk operations and index build"),
  TCase("t", ttimebuild, "time index build"),
  TCase("u", ttimemaint, "time index maintenance"),
  TCase("z", tdrop, "drop test tables")
};

static const unsigned
tcasecount = sizeof(tcaselist) / sizeof(tcaselist[0]);

static void
printcases()
{
  ndbout << "test cases:" << endl;
  for (unsigned i = 0; i < tcasecount; i++) {
    const TCase& tcase = tcaselist[i];
    ndbout << "  " << tcase.m_name << " - " << tcase.m_desc << endl;
  }
}

static void
printtables()
{
  ndbout << "tables and indexes:" << endl;
  for (unsigned j = 0; j < tabcount; j++) {
    const Tab& tab = tablist[j];
    ndbout << "  " << tab.m_name;
    for (unsigned i = 0; i < tab.m_itabs; i++) {
      const ITab& itab = tab.m_itab[i];
      ndbout << " " << itab.m_name;
    }
    ndbout << endl;
  }
}

static int
runtest(Par par)
{
  LL1("start");
  srandom(par.m_seed);
  Con con;
  CHK(con.connect() == 0);
  par.m_con = &con;
  g_thrlist = new Thr* [par.m_threads];
  for (unsigned n = 0; n < par.m_threads; n++) {
    g_thrlist[n] = 0;
  }
  for (unsigned n = 0; n < par.m_threads; n++) {
    g_thrlist[n] = new Thr(par, n);
    Thr& thr = *g_thrlist[n];
    assert(thr.m_thread != 0);
  }
  for (unsigned l = 0; par.m_loop == 0 || l < par.m_loop; l++) {
    LL1("loop " << l);
    for (unsigned i = 0; i < tcasecount; i++) {
      const TCase& tcase = tcaselist[i];
      if (par.m_case != 0 && strchr(par.m_case, tcase.m_name[0]) == 0)
        continue;
      LL1("case " << tcase.m_name << " - " << tcase.m_desc);
      for (unsigned j = 0; j < tabcount; j++) {
        if (! usetable(j))
          continue;
        const Tab& tab = tablist[j];
        par.m_tab = &tab;
        Set set(tab, par.m_totrows);
        par.m_set = &set;
        LL1("table " << tab.m_name);
        CHK(tcase.m_func(par) == 0);
      }
    }
  }
  for (unsigned n = 0; n < par.m_threads; n++) {
    Thr& thr = *g_thrlist[n];
    thr.exit();
  }
  for (unsigned n = 0; n < par.m_threads; n++) {
    Thr& thr = *g_thrlist[n];
    thr.join();
    delete &thr;
  }
  delete [] g_thrlist;
  g_thrlist = 0;
  con.disconnect();
  LL1("done");
  return 0;
}

NDB_COMMAND(testOIBasic, "testOIBasic", "testOIBasic", "testOIBasic", 65535)
{
  while (++argv, --argc > 0) {
    const char* arg = argv[0];
    if (*arg != '-') {
      ndbout << "testOIBasic: unknown argument " << arg;
      goto usage;
    }
    if (strcmp(arg, "-case") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_case = strdup(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-core") == 0) {
      g_opt.m_core = true;
      continue;
    }
    if (strcmp(arg, "-dups") == 0) {
      g_opt.m_dups = true;
      continue;
    }
    if (strcmp(arg, "-fragtype") == 0) {
      if (++argv, --argc > 0) {
        if (strcmp(argv[0], "single") == 0) {
          g_opt.m_fragtype = NdbDictionary::Object::FragSingle;
          continue;
        }
        if (strcmp(argv[0], "small") == 0) {
          g_opt.m_fragtype = NdbDictionary::Object::FragAllSmall;
          continue;
        }
        if (strcmp(argv[0], "medium") == 0) {
          g_opt.m_fragtype = NdbDictionary::Object::FragAllMedium;
          continue;
        }
        if (strcmp(argv[0], "large") == 0) {
          g_opt.m_fragtype = NdbDictionary::Object::FragAllLarge;
          continue;
        }
      }
    }
    if (strcmp(arg, "-index") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_index = strdup(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-loop") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_loop = atoi(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-nologging") == 0) {
      g_opt.m_nologging = true;
      continue;
    }
    if (strcmp(arg, "-rows") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_rows = atoi(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-scanrd") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_scanrd = atoi(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-scanex") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_scanex = atoi(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-seed") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_seed = atoi(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-subloop") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_subloop = atoi(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-table") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_table = strdup(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-threads") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_threads = atoi(argv[0]);
        continue;
      }
    }
    if (strcmp(arg, "-v") == 0) {
      if (++argv, --argc > 0) {
        g_opt.m_v = atoi(argv[0]);
        continue;
      }
    }
    if (strncmp(arg, "-v", 2) == 0 && isdigit(arg[2])) {
      g_opt.m_v = atoi(&arg[2]);
      continue;
    }
    if (strcmp(arg, "-h") == 0 || strcmp(arg, "-help") == 0) {
      printhelp();
      goto wrongargs;
    }
    ndbout << "testOIBasic: unknown option " << arg;
    goto usage;
  }
  {
    Par par(g_opt);
    if (runtest(par) < 0)
      goto failed;
  }
  // always exit with NDBT code
ok:
  return NDBT_ProgramExit(NDBT_OK);
failed:
  return NDBT_ProgramExit(NDBT_FAILED);
usage:
  ndbout << " (use -h for help)" << endl;
wrongargs:
  return NDBT_ProgramExit(NDBT_WRONGARGS);
}

// vim: set sw=2 et:
