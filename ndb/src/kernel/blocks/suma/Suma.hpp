/* Copyright (C) 2003 MySQL AB

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

#ifndef SUMA_H
#define SUMA_H

#include <ndb_limits.h>
#include <SimulatedBlock.hpp>

#include <NodeBitmask.hpp>

#include <SLList.hpp>
#include <DLList.hpp>
#include <KeyTable.hpp>
#include <DataBuffer.hpp>
#include <SignalCounter.hpp>
#include <AttributeHeader.hpp>
#include <AttributeList.hpp>

#include <signaldata/UtilSequence.hpp>
#include <signaldata/SumaImpl.hpp>

class SumaParticipant : public SimulatedBlock {
protected:
  SumaParticipant(const Configuration & conf);
  virtual ~SumaParticipant();
  BLOCK_DEFINES(SumaParticipant);
  
protected:
  /**
   * Private interface
   */
  void execSUB_CREATE_REQ(Signal* signal);
  void execSUB_REMOVE_REQ(Signal* signal);
  
  void execSUB_START_REQ(Signal* signal);
  void execSUB_STOP_REQ(Signal* signal);
  
  void execSUB_SYNC_REQ(Signal* signal);
  void execSUB_ABORT_SYNC_REQ(Signal* signal);

  void execSUB_STOP_CONF(Signal* signal);
  void execSUB_STOP_REF(Signal* signal);

 /**
   * Dict interface
   */
  void execLIST_TABLES_REF(Signal* signal);
  void execLIST_TABLES_CONF(Signal* signal);
  void execGET_TABINFOREF(Signal* signal);
  void execGET_TABINFO_CONF(Signal* signal);
#if 0
  void execGET_TABLEID_CONF(Signal* signal);
  void execGET_TABLEID_REF(Signal* signal);
#endif
  /**
   * Scan interface
   */
  void execSCAN_HBREP(Signal* signal);
  void execSCAN_FRAGREF(Signal* signal);
  void execSCAN_FRAGCONF(Signal* signal);
  void execTRANSID_AI(Signal* signal);
  void execSUB_SYNC_CONTINUE_REF(Signal* signal);
  void execSUB_SYNC_CONTINUE_CONF(Signal* signal);
  
  /**
   * DIH signals
   */
  void execDI_FCOUNTREF(Signal* signal);
  void execDI_FCOUNTCONF(Signal* signal);
  void execDIGETPRIMREF(Signal* signal);
  void execDIGETPRIMCONF(Signal* signal);

  /**
   * continueb
   */
  void execCONTINUEB(Signal* signal);

public:
  typedef DataBuffer<15> TableList;
  
  union FragmentDescriptor { 
    struct  {
      Uint16 m_fragmentNo;
      Uint16 m_nodeId;
    } m_fragDesc;
    Uint32 m_dummy;
  };
  
  /**
   * Used when sending SCAN_FRAG
   */
  union AttributeDescriptor {
    struct {
      Uint16 attrId;
      Uint16 unused;
    } m_attrDesc;
    Uint32 m_dummy;
  };

  struct Table {
    Table() { m_tableId = ~0; }
    void release(SumaParticipant&);

    union { Uint32 m_tableId; Uint32 key; };
    Uint32 m_schemaVersion;
    Uint32 m_hasTriggerDefined[3]; // Insert/Update/Delete
    Uint32 m_triggerIds[3]; // Insert/Update/Delete
    
    /**
     * Default order in which to ask for attributes during scan
     *   1) Fixed, not nullable
     *   2) Rest
     */
    DataBuffer<15>::Head m_attributes; // Attribute id's
    
    /**
     * Fragments
     */
    DataBuffer<15>::Head m_fragments;  // Fragment descriptors
    
    /**
     * Hash table stuff
     */
    Uint32 nextHash;
    union { Uint32 prevHash; Uint32 nextPool; };
    Uint32 hashValue() const {
      return m_tableId;
    }
    bool equal(const Table& rec) const {
      return m_tableId == rec.m_tableId;
    }
  };
  typedef Ptr<Table> TablePtr;

  /**
   * Subscriptions
   */
  struct SyncRecord {
    SyncRecord(SumaParticipant& s, DataBuffer<15>::DataBufferPool & p)
      : m_locked(false), m_tableList(p), suma(s)
#ifdef ERROR_INSERT
	, cerrorInsert(s.cerrorInsert)
#endif
    {}
    
    void release();

    Uint32 m_subscriptionPtrI;
    bool   m_locked;
    bool   m_doSendSyncData;
    bool   m_error;
    TableList m_tableList;    // Tables to sync (snapshoted at beginning)
    TableList::DataBufferIterator m_tableList_it;

    /**
     * Sync meta
     */
    void startMeta(Signal*);
    void nextMeta(Signal*);
    void completeMeta(Signal*);
    
    /**
     * Sync data
     */
    Uint32 m_currentTable;          // Index in m_tableList
    Uint32 m_currentFragment;       // Index in tabPtr.p->m_fragments
    DataBuffer<15>::Head m_attributeList; // Attribute if other than default
    DataBuffer<15>::Head m_tabList; // tables if other than default
    
    Uint32 m_currentTableId;        // Current table
    Uint32 m_currentNoOfAttributes; // No of attributes for current table
    void startScan(Signal*);
    void nextScan(Signal*);
    bool getNextFragment(TablePtr * tab, FragmentDescriptor * fd);
    void completeScan(Signal*);

    SumaParticipant & suma;
#ifdef ERROR_INSERT
    UintR &cerrorInsert;
#endif
    BlockNumber number() const { return suma.number(); }
    void progError(int line, int cause, const char * extra) { 
      suma.progError(line, cause, extra); 
    }
    
    void runGET_TABINFO_CONF(Signal* signal);    
    void runGET_TABINFOREF(Signal* signal);
    
    void runDI_FCOUNTCONF(Signal* signal);
    void runDIGETPRIMCONF(Signal* signal);

    Uint32 ptrI;
    union { Uint32 nextPool; Uint32 nextList; };
  };
  friend struct SyncRecord;
  
  struct Subscription {
    Subscription() {}
    Uint32 m_subscriberRef;
    Uint32 m_subscriberData;
    Uint32 m_senderRef;
    Uint32 m_senderData;
    Uint32 m_subscriptionId;
    Uint32 m_subscriptionKey;
    Uint32 m_subscriptionType;
    Uint32 m_coordinatorRef;
    Uint32 m_syncPtrI;  // Active sync operation
    Uint32 m_nSubscribers;
    bool m_markRemove;

    Uint32 nextHash;
    union { Uint32 prevHash; Uint32 nextPool; };

    Uint32 hashValue() const {
      return m_subscriptionId + m_subscriptionKey;
    }

    bool equal(const Subscription & s) const {
      return 
	m_subscriptionId == s.m_subscriptionId && 
	m_subscriptionKey == s.m_subscriptionKey;
    }
    /**
     * The following holds the table names of tables included 
     * in the subscription.
     */
    // TODO we've got to fix this, this is to inefficient. Tomas
    char m_tables[MAX_TABLES];
#if 0
    char m_tableNames[MAX_TABLES][MAX_TAB_NAME_SIZE];
#endif
    /**
     * "Iterator" used to iterate through m_tableNames
     */
    Uint32 m_maxTables;
    Uint32 m_currentTable;
  };
  typedef Ptr<Subscription> SubscriptionPtr;
  
  struct Subscriber {
    Uint32 m_senderRef;
    Uint32 m_senderData;
    Uint32 m_subscriberRef;
    Uint32 m_subscriberData;
    Uint32 m_subPtrI; //reference to subscription
    Uint32 nextList;
    union { Uint32 nextPool; Uint32 prevList; };
  };
  typedef Ptr<Subscriber> SubscriberPtr;

  /**
   * 
   */
  DLList<Subscriber> c_metaSubscribers;
  DLList<Subscriber> c_dataSubscribers;
  DLList<Subscriber> c_prepDataSubscribers;
  DLList<Subscriber> c_removeDataSubscribers;

  /**
   * Lists
   */
  KeyTable<Table> c_tables;
  DLHashTable<Subscription> c_subscriptions;
  
  /**
   * Pools
   */
  ArrayPool<Subscriber> c_subscriberPool;
  ArrayPool<Table> c_tablePool_;
  ArrayPool<Subscription> c_subscriptionPool;
  ArrayPool<SyncRecord> c_syncPool;
  DataBuffer<15>::DataBufferPool c_dataBufferPool;

  /**
   * Functions
   */
  bool parseTable(Signal* signal, class GetTabInfoConf* conf, Uint32 tableId,
		  SyncRecord* syncPtr_p);
  bool checkTableTriggers(SegmentedSectionPtr ptr);

  void addTableId(Uint32 TableId,
		  SubscriptionPtr subPtr, SyncRecord *psyncRec);

  void sendSubIdRef(Signal* signal, Uint32 errorCode);
  void sendSubCreateConf(Signal* signal, Uint32 sender, SubscriptionPtr subPtr);  
  void sendSubCreateRef(Signal* signal, const SubCreateReq& req, Uint32 errorCode);  
  void sendSubSyncRef(Signal* signal, Uint32 errorCode);  
  void sendSubRemoveRef(Signal* signal, const SubRemoveReq& ref,
			Uint32 errorCode, bool temporary = false);
  void completeSubRemoveReq(Signal* signal, SubscriptionPtr subPtr);

  /**
   * Table admin
   */
  void convertNameToId( SubscriptionPtr subPtr, Signal * signal);


};

class Suma : public SumaParticipant {
  BLOCK_DEFINES(Suma);
public:
  Suma(const Configuration & conf);
  virtual ~Suma();
private:
  /**
   * Public interface
   */
  void execCREATE_SUBSCRIPTION_REQ(Signal* signal);
  void execDROP_SUBSCRIPTION_REQ(Signal* signal);
  
  void execSTART_SUBSCRIPTION_REQ(Signal* signal);
  void execSTOP_SUBSCRIPTION_REQ(Signal* signal);
  
  void execSYNC_SUBSCRIPTION_REQ(Signal* signal);
  void execABORT_SYNC_REQ(Signal* signal);

  /**
   * Framework signals
   */

  void execREAD_CONFIG_REQ(Signal* signal);

  void execSTTOR(Signal* signal);
  void sendSTTORRY(Signal*);
  void execNDB_STTOR(Signal* signal);
  void execDUMP_STATE_ORD(Signal* signal);
  void execREAD_NODESCONF(Signal* signal);
  void execNODE_FAILREP(Signal* signal);
  void execINCL_NODEREQ(Signal* signal);
  void execCONTINUEB(Signal* signal);
  void execSIGNAL_DROPPED_REP(Signal* signal);

  /**
   * Controller interface
   */
  void execSUB_ABORT_SYNC_REF(Signal* signal);
  void execSUB_ABORT_SYNC_CONF(Signal* signal);

  /**
   * Subscription generation interface
   */
  void createSequence(Signal* signal);
  void createSequenceReply(Signal* signal,
			   UtilSequenceConf* conf,
			   UtilSequenceRef* ref);
  void execUTIL_SEQUENCE_CONF(Signal* signal);  
  void execUTIL_SEQUENCE_REF(Signal* signal);
  void execCREATE_SUBID_REQ(Signal* signal);
  
private:
  friend class Restart;
  struct SubCoordinator {
    Uint32 m_subscriberRef;
    Uint32 m_subscriberData;
    
    Uint32 m_subscriptionId;
    Uint32 m_subscriptionKey;
    
    NdbNodeBitmask m_participants;
    
    Uint32 m_outstandingGsn;
    SignalCounter m_outstandingRequests;
    
    Uint32 nextList;
    union { Uint32 prevList; Uint32 nextPool; };
  };
  Ptr<SubCoordinator> SubCoordinatorPtr;
  
  struct Node {
    Uint32 nodeId;
    Uint32 alive;
    Uint32 nextList;
    union { Uint32 prevList; Uint32 nextPool; };
  };
  typedef Ptr<Node> NodePtr;

  /**
   * Variables
   */
  NodeId c_masterNodeId;
  SLList<Node> c_nodes;
  NdbNodeBitmask c_aliveNodes;
  NdbNodeBitmask c_preparingNodes;

  Uint32 RtoI(Uint32 sumaRef, bool dieOnNotFound = true);

  /**
   * for all Suma's to keep track of other Suma's in Node group
   */
  Uint32 c_nodeGroup;
  Uint32 c_noNodesInGroup;
  Uint32 c_idInNodeGroup;
  NodeId c_nodesInGroup[MAX_REPLICAS];

  /**
   * don't seem to be used
   */
  ArrayPool<Node> c_nodePool;
  ArrayPool<SubCoordinator> c_subCoordinatorPool;
  DLList<SubCoordinator> c_runningSubscriptions;
};

#endif
