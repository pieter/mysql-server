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

#ifndef DBTUX_H
#define DBTUX_H

#include <new>
#include <ndb_limits.h>
#include <SimulatedBlock.hpp>
#include <AttributeDescriptor.hpp>
#include <AttributeHeader.hpp>
#include <ArrayPool.hpp>
#include <DataBuffer.hpp>
#include <md5_hash.hpp>

// big brother
#include <Dbtup.hpp>

// signal classes
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/TuxContinueB.hpp>
#include <signaldata/BuildIndx.hpp>
#include <signaldata/TupFrag.hpp>
#include <signaldata/AlterIndx.hpp>
#include <signaldata/DropTab.hpp>
#include <signaldata/TuxMaint.hpp>
#include <signaldata/TupAccess.hpp>
#include <signaldata/AccScan.hpp>
#include <signaldata/TuxBound.hpp>
#include <signaldata/NextScan.hpp>
#include <signaldata/AccLock.hpp>
#include <signaldata/DumpStateOrd.hpp>

// debug
#ifdef VM_TRACE
#include <NdbOut.hpp>
#include <OutputStream.hpp>
#endif

// jams
#undef jam
#undef jamEntry
#ifdef DBTUX_GEN_CPP
#define jam()           jamLine(10000 + __LINE__)
#define jamEntry()      jamEntryLine(10000 + __LINE__)
#endif
#ifdef DBTUX_META_CPP
#define jam()           jamLine(20000 + __LINE__)
#define jamEntry()      jamEntryLine(20000 + __LINE__)
#endif
#ifdef DBTUX_MAINT_CPP
#define jam()           jamLine(30000 + __LINE__)
#define jamEntry()      jamEntryLine(30000 + __LINE__)
#endif
#ifdef DBTUX_NODE_CPP
#define jam()           jamLine(40000 + __LINE__)
#define jamEntry()      jamEntryLine(40000 + __LINE__)
#endif
#ifdef DBTUX_TREE_CPP
#define jam()           jamLine(50000 + __LINE__)
#define jamEntry()      jamEntryLine(50000 + __LINE__)
#endif
#ifdef DBTUX_SCAN_CPP
#define jam()           jamLine(60000 + __LINE__)
#define jamEntry()      jamEntryLine(60000 + __LINE__)
#endif
#ifdef DBTUX_CMP_CPP
#define jam()           jamLine(70000 + __LINE__)
#define jamEntry()      jamEntryLine(70000 + __LINE__)
#endif
#ifdef DBTUX_DEBUG_CPP
#define jam()           jamLine(90000 + __LINE__)
#define jamEntry()      jamEntryLine(90000 + __LINE__)
#endif
#ifndef jam
#define jam()           jamLine(__LINE__)
#define jamEntry()      jamEntryLine(__LINE__)
#endif

#undef max
#undef min

class Configuration;

class Dbtux : public SimulatedBlock {
public:
  Dbtux(const Configuration& conf);
  virtual ~Dbtux();

  // pointer to TUP instance in this thread
  Dbtup* c_tup;

private:
  // sizes are in words (Uint32)
  static const unsigned MaxIndexFragments = 2 * NO_OF_FRAG_PER_NODE;
  static const unsigned MaxIndexAttributes = MAX_ATTRIBUTES_IN_INDEX;
  static const unsigned MaxAttrDataSize = 2048;
public:
  static const unsigned DescPageSize = 256;
private:
  static const unsigned MaxTreeNodeSize = MAX_TTREE_NODE_SIZE;
  static const unsigned ScanBoundSegmentSize = 7;
  static const unsigned MaxAccLockOps = MAX_PARALLEL_OP_PER_SCAN;
  BLOCK_DEFINES(Dbtux);

  // forward declarations
  struct DescEnt;

  /*
   * Pointer to array of Uint32.
   */
  struct Data {
  private:
    Uint32* m_data;
  public:
    Data();
    Data(Uint32* data);
    Data& operator=(Uint32* data);
    operator Uint32*() const;
    Data& operator+=(size_t n);
    AttributeHeader& ah() const;
  };
  friend class Data;

  /*
   * Pointer to array of constant Uint32.
   */
  struct ConstData;
  friend struct ConstData;
  struct ConstData {
  private:
    const Uint32* m_data;
  public:
    ConstData();
    ConstData(const Uint32* data);
    ConstData& operator=(const Uint32* data);
    operator const Uint32*() const;
    ConstData& operator+=(size_t n);
    const AttributeHeader& ah() const;
    // non-const pointer can be cast to const pointer
    ConstData(Data data);
    ConstData& operator=(Data data);
  };

  // AttributeHeader size is assumed to be 1 word
  static const unsigned AttributeHeaderSize = 1;

  /*
   * Array of pointers to TUP table attributes.  Always read-on|y.
   */
  typedef const Uint32** TableData;

  /*
   * Logical tuple address, "local key".  Identifies table tuples.
   */
  typedef Uint32 TupAddr;
  static const unsigned NullTupAddr = (Uint32)-1;

  /*
   * Physical tuple address in TUP.  Provides fast access to table tuple
   * or index node.  Valid within the db node and across timeslices.
   * Not valid between db nodes or across restarts.
   */
  struct TupLoc {
    Uint32 m_pageId;            // page i-value
    Uint16 m_pageOffset;        // page offset in words
    TupLoc();
    TupLoc(Uint32 pageId, Uint16 pageOffset);
    bool operator==(const TupLoc& loc) const;
    bool operator!=(const TupLoc& loc) const;
  };

  /*
   * There is no const member NullTupLoc since the compiler may not be
   * able to optimize it to TupLoc() constants.  Instead null values are
   * constructed on the stack with TupLoc().
   */
#define NullTupLoc TupLoc()

  // tree definitions

  /*
   * Tree entry.  Points to a tuple in primary table via physical
   * address of "original" tuple and tuple version.
   *
   * ZTUP_VERSION_BITS must be 15 (or less).
   */
  struct TreeEnt;
  friend struct TreeEnt;
  struct TreeEnt {
    TupLoc m_tupLoc;            // address of original tuple
    unsigned m_tupVersion : 15; // version
    unsigned m_fragBit : 1;     // which duplicated table fragment
    TreeEnt();
    // methods
    int cmp(const TreeEnt ent) const;
  };
  static const unsigned TreeEntSize = sizeof(TreeEnt) >> 2;
  static const TreeEnt NullTreeEnt;

  /*
   * Tree node has 1) fixed part 2) actual table data for min and max
   * prefix 3) max and min entries 4) rest of entries 5) one extra entry
   * used as work space.
   *
   * struct TreeNode            part 1, size 6 words
   * min prefix                 part 2, size TreeHead::m_prefSize
   * max prefix                 part 2, size TreeHead::m_prefSize
   * max entry                  part 3
   * min entry                  part 3
   * rest of entries            part 4
   * work entry                 part 5
   *
   * There are 3 links to other nodes: left child, right child, parent.
   * These are in TupLoc format but the pageIds and pageOffsets are
   * stored in separate arrays (saves 1 word).
   *
   * Occupancy (number of entries) is at least 1 except temporarily when
   * a node is about to be removed.  If occupancy is 1, only max entry
   * is present but both min and max prefixes are set.
   */
  struct TreeNode;
  friend struct TreeNode;
  struct TreeNode {
    Uint32 m_linkPI[3];         // link to 0-left child 1-right child 2-parent
    Uint16 m_linkPO[3];         // page offsets for above real page ids
    unsigned m_side : 2;        // we are 0-left child 1-right child 2-root
    int m_balance : 2;          // balance -1, 0, +1
    unsigned pad1 : 4;
    Uint8 m_occup;              // current number of entries
    Uint32 m_nodeScan;          // list of scans at this node
    TreeNode();
  };
  static const unsigned NodeHeadSize = sizeof(TreeNode) >> 2;

  /*
   * Tree nodes are not always accessed fully, for cache reasons.  There
   * are 3 access sizes.
   */
  enum AccSize {
    AccNone = 0,
    AccHead = 1,                // part 1
    AccPref = 2,                // parts 1-3
    AccFull = 3                 // parts 1-5
  };

  /*
   * Tree header.  There is one in each fragment.  Contains tree
   * parameters and address of root node.
   */
  struct TreeHead;
  friend struct TreeHead;
  struct TreeHead {
    Uint8 m_nodeSize;           // words in tree node
    Uint8 m_prefSize;           // words in min/max prefix each
    Uint8 m_minOccup;           // min entries in internal node
    Uint8 m_maxOccup;           // max entries in node
    TupLoc m_root;              // root node
    TreeHead();
    // methods
    unsigned getSize(AccSize acc) const;
    Data getPref(TreeNode* node, unsigned i) const;
    TreeEnt* getEntList(TreeNode* node) const;
  };

  /*
   * Tree position.  Specifies node, position within node (from 0 to
   * m_occup), and whether the position is at an existing entry or
   * before one (if any).  Position m_occup points past the node and is
   * also represented by position 0 of next node.  Includes direction
   * and copy of entry used by scan.
   */
  struct TreePos;
  friend struct TreePos;
  struct TreePos {
    TupLoc m_loc;               // physical node address
    Uint16 m_pos;               // position 0 to m_occup
    Uint8 m_match;              // at an existing entry
    Uint8 m_dir;                // from link (0-2) or within node (3)
    TreeEnt m_ent;              // copy of current entry
    TreePos();
  };

  // packed metadata

  /*
   * Descriptor page.  The "hot" metadata for an index is stored as
   * a contiguous array of words on some page.
   */
  struct DescPage;
  friend struct DescPage;
  struct DescPage {
    Uint32 m_nextPage;
    Uint32 m_numFree;           // number of free words
    union {
    Uint32 m_data[DescPageSize];
    Uint32 nextPool;
    };
    DescPage();
  };
  typedef Ptr<DescPage> DescPagePtr;
  ArrayPool<DescPage> c_descPagePool;
  Uint32 c_descPageList;

  /*
   * Header for index metadata.  Size must be multiple of word size.
   */
  struct DescHead {
    unsigned m_indexId : 24;
    unsigned pad1 : 8;
  };
  static const unsigned DescHeadSize = sizeof(DescHead) >> 2;

  /*
   * Attribute metadata.  Size must be multiple of word size.
   */
  struct DescAttr {
    Uint32 m_attrDesc;          // standard AttributeDescriptor
    Uint16 m_primaryAttrId;
    Uint16 m_typeId;
  };
  static const unsigned DescAttrSize = sizeof(DescAttr) >> 2;

  /*
   * Complete metadata for one index. The array of attributes has
   * variable size.
   */
  struct DescEnt;
  friend struct DescEnt;
  struct DescEnt {
    DescHead m_descHead;
    DescAttr m_descAttr[1];     // variable size data
  };

  // range scan
 
  /*
   * Scan bounds are stored in linked list of segments.
   */
  typedef DataBuffer<ScanBoundSegmentSize> ScanBound;
  typedef DataBuffer<ScanBoundSegmentSize>::ConstDataBufferIterator ScanBoundIterator;
  typedef DataBuffer<ScanBoundSegmentSize>::DataBufferPool ScanBoundPool;
  ScanBoundPool c_scanBoundPool;
 
  /*
   * Scan operation.
   *
   * Tuples are locked one at a time.  The current lock op is set to
   * RNIL as soon as the lock is obtained and passed to LQH.  We must
   * however remember all locks which LQH has not returned for unlocking
   * since they must be aborted by us when the scan is closed.
   *
   * Scan state describes the entry we are interested in.  There is
   * a separate lock wait flag.  It may be for current entry or it may
   * be for an entry we were moved away from.  In any case nothing
   * happens with current entry before lock wait flag is cleared.
   */
  struct ScanOp;
  friend struct ScanOp;
  struct ScanOp {
    enum {
      Undef = 0,
      First = 1,                // before first entry
      Current = 2,              // at current before locking
      Blocked = 3,              // at current waiting for ACC lock
      Locked = 4,               // at current and locked or no lock needed
      Next = 5,                 // looking for next extry
      Last = 6,                 // after last entry
      Aborting = 7,             // lock wait at scan close
      Invalid = 9               // cannot return REF to LQH currently
    };
    Uint16 m_state;
    Uint16 m_lockwait;
    Uint32 m_userPtr;           // scanptr.i in LQH
    Uint32 m_userRef;
    Uint32 m_tableId;
    Uint32 m_indexId;
    Uint32 m_fragId;
    Uint32 m_fragPtrI;
    Uint32 m_transId1;
    Uint32 m_transId2;
    Uint32 m_savePointId;
    // lock waited for or obtained and not yet passed to LQH
    Uint32 m_accLockOp;
    // locks obtained and passed to LQH but not yet returned by LQH
    Uint32 m_accLockOps[MaxAccLockOps];
    Uint8 m_readCommitted;      // no locking
    Uint8 m_lockMode;
    Uint8 m_keyInfo;
    ScanBound m_boundMin;
    ScanBound m_boundMax;
    ScanBound* m_bound[2];      // pointers to above 2
    Uint16 m_boundCnt[2];       // number of bounds in each
    TreePos m_scanPos;          // position
    TreeEnt m_lastEnt;          // last entry returned
    Uint32 m_nodeScan;          // next scan at node (single-linked)
    union {
    Uint32 nextPool;
    Uint32 nextList;
    };
    Uint32 prevList;
    ScanOp(ScanBoundPool& scanBoundPool);
  };
  typedef Ptr<ScanOp> ScanOpPtr;
  ArrayPool<ScanOp> c_scanOpPool;

  // indexes and fragments

  /*
   * Ordered index.  Top level data structure.  The primary table (table
   * being indexed) lives in TUP.
   */
  struct Index;
  friend struct Index;
  struct Index {
    enum State {
      NotDefined = 0,
      Defining = 1,
      Online = 2,               // triggers activated and build done
      Dropping = 9
    };
    State m_state;
    DictTabInfo::TableType m_tableType;
    Uint32 m_tableId;
    Uint16 m_fragOff;           // offset for duplicate fragId bits
    Uint16 m_numFrags;
    Uint32 m_fragId[MaxIndexFragments];
    Uint32 m_fragPtrI[MaxIndexFragments];
    Uint32 m_descPage;          // descriptor page
    Uint16 m_descOff;           // offset within the page
    Uint16 m_numAttrs;
    union {
    Uint32 nextPool;
    };
    Index();
  };
  typedef Ptr<Index> IndexPtr;
  ArrayPool<Index> c_indexPool;

  /*
   * Fragment of an index, as known to DIH/TC.  Represents the two
   * duplicate fragments known to LQH/ACC/TUP.  Includes tree header.
   * There are no maintenance operation records yet.
   */
  struct Frag;
  friend struct Frag;
  struct Frag {
    Uint32 m_tableId;           // copy from index level
    Uint32 m_indexId;
    Uint16 m_fragOff;
    Uint16 m_fragId;
    Uint32 m_descPage;          // copy from index level
    Uint16 m_descOff;
    Uint16 m_numAttrs;
    TreeHead m_tree;
    TupLoc m_freeLoc;           // one node pre-allocated for insert
    DLList<ScanOp> m_scanList;  // current scans on this fragment
    Uint32 m_tupIndexFragPtrI;
    Uint32 m_tupTableFragPtrI[2];
    Uint32 m_accTableFragPtrI[2];
    union {
    Uint32 nextPool;
    };
    Frag(ArrayPool<ScanOp>& scanOpPool);
  };
  typedef Ptr<Frag> FragPtr;
  ArrayPool<Frag> c_fragPool;

  /*
   * Fragment metadata operation.
   */
  struct FragOp {
    Uint32 m_userPtr;
    Uint32 m_userRef;
    Uint32 m_indexId;
    Uint32 m_fragId;
    Uint32 m_fragPtrI;
    Uint32 m_fragNo;            // fragment number starting at zero
    Uint32 m_numAttrsRecvd;
    union {
    Uint32 nextPool;
    };
    FragOp();
  };
  typedef Ptr<FragOp> FragOpPtr;
  ArrayPool<FragOp> c_fragOpPool;

  // node handles

  /*
   * A node handle is a reference to a tree node in TUP.  It is used to
   * operate on the node.  Node handles are allocated on the stack.
   */
  struct NodeHandle;
  friend struct NodeHandle;
  struct NodeHandle {
    Frag& m_frag;               // fragment using the node
    TupLoc m_loc;               // physical node address
    TreeNode* m_node;           // pointer to node storage
    AccSize m_acc;              // accessed size
    NodeHandle(Frag& frag);
    NodeHandle(const NodeHandle& node);
    NodeHandle& operator=(const NodeHandle& node);
    // getters
    TupLoc getLink(unsigned i);
    unsigned getChilds();       // cannot spell
    unsigned getSide();
    unsigned getOccup();
    int getBalance();
    Uint32 getNodeScan();
    // setters
    void setLink(unsigned i, TupLoc loc);
    void setSide(unsigned i);
    void setOccup(unsigned n);
    void setBalance(int b);
    void setNodeScan(Uint32 scanPtrI);
    // access other parts of the node
    Data getPref(unsigned i);
    TreeEnt getEnt(unsigned pos);
    TreeEnt getMinMax(unsigned i);
    // for ndbrequire and ndbassert
    void progError(int line, int cause, const char* file);
  };

  // parameters for methods
  
  /*
   * Copy attribute data.
   */
  struct CopyPar {
    unsigned m_items;           // number of attributes
    bool m_headers;             // copy headers flag (default true)
    unsigned m_maxwords;        // limit size (default no limit)
    // output
    unsigned m_numitems;        // number of attributes fully copied
    unsigned m_numwords;        // number of words copied
    CopyPar();
  };

  /*
   * Read index key attributes.
   */
  struct ReadPar;
  friend struct ReadPar;
  struct ReadPar {
    TreeEnt m_ent;              // tuple to read
    unsigned m_first;           // first index attribute
    unsigned m_count;           // number of consecutive index attributes
    Data m_data;                // set pointer if 0 else copy result to it
    unsigned m_size;            // number of words (set in read keys only)
    ReadPar();
  };

  /*
   * Scan bound comparison.
   */
  struct BoundPar;
  friend struct BoundPar;
  struct BoundPar {
    ConstData m_data1;          // full bound data
    ConstData m_data2;          // full or prefix data
    unsigned m_count1;          // number of bounds
    unsigned m_len2;            // words in data2 buffer
    unsigned m_dir;             // 0-lower bound 1-upper bound
    BoundPar();
  };

  // methods

  /*
   * DbtuxGen.cpp
   */
  void execCONTINUEB(Signal* signal);
  void execSTTOR(Signal* signal);
  void execREAD_CONFIG_REQ(Signal* signal);
  // utils
  void copyAttrs(Data dst, ConstData src, CopyPar& copyPar);

  /*
   * DbtuxMeta.cpp
   */
  void execTUXFRAGREQ(Signal* signal);
  void execTUX_ADD_ATTRREQ(Signal* signal);
  void execALTER_INDX_REQ(Signal* signal);
  void execDROP_TAB_REQ(Signal* signal);
  bool allocDescEnt(IndexPtr indexPtr);
  void freeDescEnt(IndexPtr indexPtr);
  void dropIndex(Signal* signal, IndexPtr indexPtr, Uint32 senderRef, Uint32 senderData);

  /*
   * DbtuxMaint.cpp
   */
  void execTUX_MAINT_REQ(Signal* signal);
  void tupReadAttrs(Signal* signal, const Frag& frag, ReadPar& readPar);
  void tupReadKeys(Signal* signal, const Frag& frag, ReadPar& readPar);
  
  /*
   * DbtuxNode.cpp
   */
  int allocNode(Signal* signal, NodeHandle& node);
  void accessNode(Signal* signal, NodeHandle& node, AccSize acc);
  void selectNode(Signal* signal, NodeHandle& node, TupLoc loc, AccSize acc);
  void insertNode(Signal* signal, NodeHandle& node, AccSize acc);
  void deleteNode(Signal* signal, NodeHandle& node);
  void setNodePref(Signal* signal, NodeHandle& node, unsigned i);
  // node operations
  void nodePushUp(Signal* signal, NodeHandle& node, unsigned pos, const TreeEnt& ent);
  void nodePopDown(Signal* signal, NodeHandle& node, unsigned pos, TreeEnt& ent);
  void nodePushDown(Signal* signal, NodeHandle& node, unsigned pos, TreeEnt& ent);
  void nodePopUp(Signal* signal, NodeHandle& node, unsigned pos, TreeEnt& ent);
  void nodeSlide(Signal* signal, NodeHandle& dstNode, NodeHandle& srcNode, unsigned i);
  // scans linked to node
  void linkScan(NodeHandle& node, ScanOpPtr scanPtr);
  void unlinkScan(NodeHandle& node, ScanOpPtr scanPtr);
  bool islinkScan(NodeHandle& node, ScanOpPtr scanPtr);

  /*
   * DbtuxTree.cpp
   */
  void treeSearch(Signal* signal, Frag& frag, TableData searchKey, TreeEnt searchEnt, TreePos& treePos);
  void treeAdd(Signal* signal, Frag& frag, TreePos treePos, TreeEnt ent);
  void treeRemove(Signal* signal, Frag& frag, TreePos treePos);
  void treeRotateSingle(Signal* signal, Frag& frag, NodeHandle& node, unsigned i);
  void treeRotateDouble(Signal* signal, Frag& frag, NodeHandle& node, unsigned i);

  /*
   * DbtuxScan.cpp
   */
  void execACC_SCANREQ(Signal* signal);
  void execTUX_BOUND_INFO(Signal* signal);
  void execNEXT_SCANREQ(Signal* signal);
  void execACC_CHECK_SCAN(Signal* signal);
  void execACCKEYCONF(Signal* signal);
  void execACCKEYREF(Signal* signal);
  void execACC_ABORTCONF(Signal* signal);
  void scanFirst(Signal* signal, ScanOpPtr scanPtr);
  void scanNext(Signal* signal, ScanOpPtr scanPtr);
  bool scanVisible(Signal* signal, ScanOpPtr scanPtr, TreeEnt ent);
  void scanClose(Signal* signal, ScanOpPtr scanPtr);
  void addAccLockOp(ScanOp& scan, Uint32 accLockOp);
  void removeAccLockOp(ScanOp& scan, Uint32 accLockOp);
  void releaseScanOp(ScanOpPtr& scanPtr);

  /*
   * DbtuxCmp.cpp
   */
  int cmpSearchKey(const Frag& frag, unsigned& start, TableData data1, ConstData data2, unsigned size2 = MaxAttrDataSize);
  int cmpSearchKey(const Frag& frag, unsigned& start, TableData data1, TableData data2);
  int cmpScanBound(const Frag& frag, const BoundPar boundPar);

  /*
   * DbtuxDebug.cpp
   */
  void execDUMP_STATE_ORD(Signal* signal);
#ifdef VM_TRACE
  struct PrintPar {
    char m_path[100];           // LR prefix
    unsigned m_side;            // expected side
    TupLoc m_parent;            // expected parent address
    int m_depth;                // returned depth
    unsigned m_occup;           // returned occupancy
    bool m_ok;                  // returned status
    PrintPar();
  };
  void printTree(Signal* signal, Frag& frag, NdbOut& out);
  void printNode(Signal* signal, Frag& frag, NdbOut& out, TupLoc loc, PrintPar& par);
  friend class NdbOut& operator<<(NdbOut&, const TupLoc&);
  friend class NdbOut& operator<<(NdbOut&, const TreeEnt&);
  friend class NdbOut& operator<<(NdbOut&, const TreeNode&);
  friend class NdbOut& operator<<(NdbOut&, const TreeHead&);
  friend class NdbOut& operator<<(NdbOut&, const TreePos&);
  friend class NdbOut& operator<<(NdbOut&, const DescAttr&);
  friend class NdbOut& operator<<(NdbOut&, const ScanOp&);
  friend class NdbOut& operator<<(NdbOut&, const Index&);
  friend class NdbOut& operator<<(NdbOut&, const Frag&);
  friend class NdbOut& operator<<(NdbOut&, const NodeHandle&);
  FILE* debugFile;
  NdbOut debugOut;
  unsigned debugFlags;
  enum {
    DebugMeta = 1,              // log create and drop index
    DebugMaint = 2,             // log maintenance ops
    DebugTree = 4,              // log and check tree after each op
    DebugScan = 8               // log scans
  };
#endif

  // start up info
  Uint32 c_internalStartPhase;
  Uint32 c_typeOfStart;

  // buffer for scan bounds and keyinfo (primary key)
  Data c_dataBuffer;

  // array of index key attribute ids in AttributeHeader format
  Data c_keyAttrs;

  // search key data as pointers to TUP storage
  TableData c_searchKey;

  // current entry key data as pointers to TUP storage
  TableData c_entryKey;

  // inlined utils
  DescEnt& getDescEnt(Uint32 descPage, Uint32 descOff);
  Uint32 getTupAddr(const Frag& frag, TreeEnt ent);
  void setKeyAttrs(const Frag& frag, Data keyAttrs);
  void readKeyAttrs(const Frag& frag, TreeEnt ent, unsigned start, ConstData keyAttrs, TableData keyData);
  static unsigned min(unsigned x, unsigned y);
  static unsigned max(unsigned x, unsigned y);
};

// Dbtux::Data

inline
Dbtux::Data::Data() :
  m_data(0)
{
}

inline
Dbtux::Data::Data(Uint32* data) :
  m_data(data)
{
}

inline Dbtux::Data&
Dbtux::Data::operator=(Uint32* data)
{
  m_data = data;
  return *this;
}

inline
Dbtux::Data::operator Uint32*() const
{
  return m_data;
}

inline Dbtux::Data&
Dbtux::Data::operator+=(size_t n)
{
  m_data += n;
  return *this;
}

inline AttributeHeader&
Dbtux::Data::ah() const
{
  return *reinterpret_cast<AttributeHeader*>(m_data);
}

// Dbtux::ConstData

inline
Dbtux::ConstData::ConstData() :
  m_data(0)
{
}

inline
Dbtux::ConstData::ConstData(const Uint32* data) :
  m_data(data)
{
}

inline Dbtux::ConstData&
Dbtux::ConstData::operator=(const Uint32* data)
{
  m_data = data;
  return *this;
}

inline
Dbtux::ConstData::operator const Uint32*() const
{
  return m_data;
}

inline Dbtux::ConstData&
Dbtux::ConstData::operator+=(size_t n)
{
  m_data += n;
  return *this;
}

inline const AttributeHeader&
Dbtux::ConstData::ah() const
{
  return *reinterpret_cast<const AttributeHeader*>(m_data);
}

inline
Dbtux::ConstData::ConstData(Data data) :
  m_data(static_cast<Uint32*>(data))
{
}

inline Dbtux::ConstData&
Dbtux::ConstData::operator=(Data data)
{
  m_data = static_cast<Uint32*>(data);
  return *this;
}

// Dbtux::TupLoc

inline
Dbtux::TupLoc::TupLoc() :
  m_pageId(RNIL),
  m_pageOffset(0)
{
}

inline
Dbtux::TupLoc::TupLoc(Uint32 pageId, Uint16 pageOffset) :
  m_pageId(pageId),
  m_pageOffset(pageOffset)
{
}

inline bool
Dbtux::TupLoc::operator==(const TupLoc& loc) const
{
  return m_pageId == loc.m_pageId && m_pageOffset == loc.m_pageOffset;
}

inline bool
Dbtux::TupLoc::operator!=(const TupLoc& loc) const
{
  return ! (*this == loc);
}

// Dbtux::TreeEnt

inline
Dbtux::TreeEnt::TreeEnt() :
  m_tupLoc(),
  m_tupVersion(0),
  m_fragBit(0)
{
}

inline int
Dbtux::TreeEnt::cmp(const TreeEnt ent) const
{
  if (m_fragBit < ent.m_fragBit)
    return -1;
  if (m_fragBit > ent.m_fragBit)
    return +1;
  if (m_tupLoc.m_pageId < ent.m_tupLoc.m_pageId)
    return -1;
  if (m_tupLoc.m_pageId > ent.m_tupLoc.m_pageId)
    return +1;
  if (m_tupLoc.m_pageOffset < ent.m_tupLoc.m_pageOffset)
    return -1;
  if (m_tupLoc.m_pageOffset > ent.m_tupLoc.m_pageOffset)
    return +1;
  if (m_tupVersion < ent.m_tupVersion)
    return -1;
  if (m_tupVersion > ent.m_tupVersion)
    return +1;
  return 0;
}

// Dbtux::TreeNode

inline
Dbtux::TreeNode::TreeNode() :
  m_side(2),
  m_balance(0),
  pad1(0),
  m_occup(0),
  m_nodeScan(RNIL)
{
  m_linkPI[0] = NullTupLoc.m_pageId;
  m_linkPO[0] = NullTupLoc.m_pageOffset;
  m_linkPI[1] = NullTupLoc.m_pageId;
  m_linkPO[1] = NullTupLoc.m_pageOffset;
  m_linkPI[2] = NullTupLoc.m_pageId;
  m_linkPO[2] = NullTupLoc.m_pageOffset;
}

// Dbtux::TreeHead

inline
Dbtux::TreeHead::TreeHead() :
  m_nodeSize(0),
  m_prefSize(0),
  m_minOccup(0),
  m_maxOccup(0),
  m_root()
{
}

inline unsigned
Dbtux::TreeHead::getSize(AccSize acc) const
{
  switch (acc) {
  case AccNone:
    return 0;
  case AccHead:
    return NodeHeadSize;
  case AccPref:
    return NodeHeadSize + 2 * m_prefSize + 2 * TreeEntSize;
  case AccFull:
    return m_nodeSize;
  }
  REQUIRE(false, "invalid Dbtux::AccSize");
  return 0;
}

inline Dbtux::Data
Dbtux::TreeHead::getPref(TreeNode* node, unsigned i) const
{
  Uint32* ptr = (Uint32*)node + NodeHeadSize + i * m_prefSize;
  return ptr;
}

inline Dbtux::TreeEnt*
Dbtux::TreeHead::getEntList(TreeNode* node) const
{
  Uint32* ptr = (Uint32*)node + NodeHeadSize + 2 * m_prefSize;
  return (TreeEnt*)ptr;
}

// Dbtux::TreePos

inline
Dbtux::TreePos::TreePos() :
  m_loc(),
  m_pos(ZNIL),
  m_match(false),
  m_dir(255),
  m_ent()
{
}

// Dbtux::DescPage

inline
Dbtux::DescPage::DescPage() :
  m_nextPage(RNIL),
  m_numFree(ZNIL)
{
  for (unsigned i = 0; i < DescPageSize; i++) {
#ifdef VM_TRACE
    m_data[i] = 0x13571357;
#else
    m_data[i] = 0;
#endif
  }
}

// Dbtux::ScanOp

inline
Dbtux::ScanOp::ScanOp(ScanBoundPool& scanBoundPool) :
  m_state(Undef),
  m_lockwait(false),
  m_userPtr(RNIL),
  m_userRef(RNIL),
  m_tableId(RNIL),
  m_indexId(RNIL),
  m_fragPtrI(RNIL),
  m_transId1(0),
  m_transId2(0),
  m_savePointId(0),
  m_accLockOp(RNIL),
  m_readCommitted(0),
  m_lockMode(0),
  m_keyInfo(0),
  m_boundMin(scanBoundPool),
  m_boundMax(scanBoundPool),
  m_scanPos(),
  m_lastEnt(),
  m_nodeScan(RNIL)
{
  m_bound[0] = &m_boundMin;
  m_bound[1] = &m_boundMax;
  m_boundCnt[0] = 0;
  m_boundCnt[1] = 0;
  for (unsigned i = 0; i < MaxAccLockOps; i++) {
    m_accLockOps[i] = RNIL;
  }
}

// Dbtux::Index

inline
Dbtux::Index::Index() :
  m_state(NotDefined),
  m_tableType(DictTabInfo::UndefTableType),
  m_tableId(RNIL),
  m_numFrags(0),
  m_descPage(RNIL),
  m_descOff(0),
  m_numAttrs(0)
{
  for (unsigned i = 0; i < MaxIndexFragments; i++) {
    m_fragId[i] = ZNIL;
    m_fragPtrI[i] = RNIL;
  };
};

// Dbtux::Frag

inline
Dbtux::Frag::Frag(ArrayPool<ScanOp>& scanOpPool) :
  m_tableId(RNIL),
  m_indexId(RNIL),
  m_fragOff(ZNIL),
  m_fragId(ZNIL),
  m_descPage(RNIL),
  m_descOff(0),
  m_numAttrs(ZNIL),
  m_tree(),
  m_freeLoc(),
  m_scanList(scanOpPool),
  m_tupIndexFragPtrI(RNIL)
{
  m_tupTableFragPtrI[0] = RNIL;
  m_tupTableFragPtrI[1] = RNIL;
  m_accTableFragPtrI[0] = RNIL;
  m_accTableFragPtrI[1] = RNIL;
}

// Dbtux::FragOp

inline
Dbtux::FragOp::FragOp() :
  m_userPtr(RNIL),
  m_userRef(RNIL),
  m_indexId(RNIL),
  m_fragId(ZNIL),
  m_fragPtrI(RNIL),
  m_fragNo(ZNIL),
  m_numAttrsRecvd(ZNIL)
{
};

// Dbtux::NodeHandle

inline
Dbtux::NodeHandle::NodeHandle(Frag& frag) :
  m_frag(frag),
  m_loc(),
  m_node(0),
  m_acc(AccNone)
{
}

inline
Dbtux::NodeHandle::NodeHandle(const NodeHandle& node) :
  m_frag(node.m_frag),
  m_loc(node.m_loc),
  m_node(node.m_node),
  m_acc(node.m_acc)
{
}

inline Dbtux::NodeHandle&
Dbtux::NodeHandle::operator=(const NodeHandle& node)
{
  ndbassert(&m_frag == &node.m_frag);
  m_loc = node.m_loc;
  m_node = node.m_node;
  m_acc = node.m_acc;
  return *this;
}

inline Dbtux::TupLoc
Dbtux::NodeHandle::getLink(unsigned i)
{
  ndbrequire(i <= 2);
  return TupLoc(m_node->m_linkPI[i], m_node->m_linkPO[i]);
}

inline unsigned
Dbtux::NodeHandle::getChilds()
{
  return (getLink(0) != NullTupLoc) + (getLink(1) != NullTupLoc);
}

inline unsigned
Dbtux::NodeHandle::getSide()
{
  return m_node->m_side;
}

inline unsigned
Dbtux::NodeHandle::getOccup()
{
  return m_node->m_occup;
}

inline int
Dbtux::NodeHandle::getBalance()
{
  return m_node->m_balance;
}

inline Uint32
Dbtux::NodeHandle::getNodeScan()
{
  return m_node->m_nodeScan;
}

inline void
Dbtux::NodeHandle::setLink(unsigned i, TupLoc loc)
{
  ndbrequire(i <= 2);
  m_node->m_linkPI[i] = loc.m_pageId;
  m_node->m_linkPO[i] = loc.m_pageOffset;
}

inline void
Dbtux::NodeHandle::setSide(unsigned i)
{
  ndbrequire(i <= 2);
  m_node->m_side = i;
}

inline void
Dbtux::NodeHandle::setOccup(unsigned n)
{
  TreeHead& tree = m_frag.m_tree;
  ndbrequire(n <= tree.m_maxOccup);
  m_node->m_occup = n;
}

inline void
Dbtux::NodeHandle::setBalance(int b)
{
  ndbrequire(abs(b) <= 1);
  m_node->m_balance = b;
}

inline void
Dbtux::NodeHandle::setNodeScan(Uint32 scanPtrI)
{
  m_node->m_nodeScan = scanPtrI;
}

inline Dbtux::Data
Dbtux::NodeHandle::getPref(unsigned i)
{
  TreeHead& tree = m_frag.m_tree;
  ndbrequire(m_acc >= AccPref && i <= 1);
  return tree.getPref(m_node, i);
}

inline Dbtux::TreeEnt
Dbtux::NodeHandle::getEnt(unsigned pos)
{
  TreeHead& tree = m_frag.m_tree;
  TreeEnt* entList = tree.getEntList(m_node);
  const unsigned occup = m_node->m_occup;
  ndbrequire(pos < occup);
  if (pos == 0 || pos == occup - 1) {
    ndbrequire(m_acc >= AccPref)
  } else {
    ndbrequire(m_acc == AccFull)
  }
  return entList[(1 + pos) % occup];
}

inline Dbtux::TreeEnt
Dbtux::NodeHandle::getMinMax(unsigned i)
{
  const unsigned occup = m_node->m_occup;
  ndbrequire(i <= 1 && occup != 0);
  return getEnt(i == 0 ? 0 : occup - 1);
}

// parameters for methods

inline
Dbtux::CopyPar::CopyPar() :
  m_items(0),
  m_headers(true),
  m_maxwords(~0),       // max unsigned
  // output
  m_numitems(0),
  m_numwords(0)
{
}

inline
Dbtux::ReadPar::ReadPar() :
  m_first(0),
  m_count(0),
  m_data(0),
  m_size(0)
{
}

inline
Dbtux::BoundPar::BoundPar() :
  m_data1(0),
  m_data2(0),
  m_count1(0),
  m_len2(0),
  m_dir(255)
{
}

#ifdef VM_TRACE
inline
Dbtux::PrintPar::PrintPar() :
  // caller fills in
  m_path(),
  m_side(255),
  m_parent(),
  // default return values
  m_depth(0),
  m_occup(0),
  m_ok(true)
{
}
#endif

// utils

inline Dbtux::DescEnt&
Dbtux::getDescEnt(Uint32 descPage, Uint32 descOff)
{
  DescPagePtr pagePtr;
  pagePtr.i = descPage;
  c_descPagePool.getPtr(pagePtr);
  ndbrequire(descOff < DescPageSize);
  DescEnt* descEnt = (DescEnt*)&pagePtr.p->m_data[descOff];
  return *descEnt;
}

inline Uint32
Dbtux::getTupAddr(const Frag& frag, TreeEnt ent)
{
  const Uint32 tableFragPtrI = frag.m_tupTableFragPtrI[ent.m_fragBit];
  const TupLoc tupLoc = ent.m_tupLoc;
  Uint32 tupAddr = NullTupAddr;
  c_tup->tuxGetTupAddr(tableFragPtrI, tupLoc.m_pageId, tupLoc.m_pageOffset, tupAddr);
  jamEntry();
  return tupAddr;
}

inline void
Dbtux::setKeyAttrs(const Frag& frag, Data keyAttrs)
{
  const unsigned numAttrs = frag.m_numAttrs;
  const DescEnt& descEnt = getDescEnt(frag.m_descPage, frag.m_descOff);
  for (unsigned i = 0; i < numAttrs; i++) {
    const DescAttr& descAttr = descEnt.m_descAttr[i];
    Uint32 size = AttributeDescriptor::getSizeInWords(descAttr.m_attrDesc);
    keyAttrs.ah() = AttributeHeader(descAttr.m_primaryAttrId, size);
    keyAttrs += 1;
  }
}

inline void
Dbtux::readKeyAttrs(const Frag& frag, TreeEnt ent, unsigned start, ConstData keyAttrs, TableData keyData)
{
  const Uint32 tableFragPtrI = frag.m_tupTableFragPtrI[ent.m_fragBit];
  const TupLoc tupLoc = ent.m_tupLoc;
  const Uint32 tupVersion = ent.m_tupVersion;
  ndbrequire(start < frag.m_numAttrs);
  const unsigned numAttrs = frag.m_numAttrs - start;
  // start applies to both keys and output data
  keyAttrs += start;
  keyData += start;
  c_tup->tuxReadAttrs(tableFragPtrI, tupLoc.m_pageId, tupLoc.m_pageOffset, tupVersion, numAttrs, keyAttrs, keyData);
  jamEntry();
}

inline unsigned
Dbtux::min(unsigned x, unsigned y)
{
  return x < y ? x : y;
}

inline unsigned
Dbtux::max(unsigned x, unsigned y)
{
  return x > y ? x : y;
}

#endif
