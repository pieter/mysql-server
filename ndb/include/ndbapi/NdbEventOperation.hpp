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

/*****************************************************************************
 * Name:          NdbEventOperation.hpp
 * Include:
 * Link:
 * Author:        Tomas Ulin MySQL AB
 * Date:          2003-11-21
 * Version:       0.1
 * Description:   Event support
 * Documentation:
 * Adjust:  2003-11-21  Tomas Ulin   First version.
 * Adjust:  2003-12-11  Tomas Ulin   Alpha Release.
 ****************************************************************************/

#ifndef NdbEventOperation_H
#define NdbEventOperation_H

class NdbGlobalEventBuffer;
class NdbEventOperationImpl;

/**
 * @class NdbEventOperation
 * @brief Class of operations for getting change events from database.  
 *
 * An NdbEventOperation object is instantiated by 
 * NdbEventOperation *Ndb::createEventOperation(const char *eventName,
 * int bufferLength)
 *
 * Prior to that an event must have been created in the Database through
 * int NdbDictionary::createEvent(NdbDictionary::Event)
 * 
 * bufferLength indicates size of circular buffer to store event info as
 * they occur.
 *
 * The instance is removed by Ndb::dropEventOperation(NdbEventOperation*)
 *
 * For more info see:
 * ndbapi_example5.cpp
 * Ndb.hpp
 * NdbDictionary.hpp
 *
 * Known limitations:
 *
 * Maximum number of active NdbEventOperations are now set at compile time.
 * Today 100.  This will become a configuration parameter later.
 *
 * Maximum number of NdbEventOperations tied to same event are maximum 16
 * per process.
 *
 * Known issues:
 *
 * When several NdbEventOperation s are tied to the same event in the same
 * process they will share the circular buffer. The BufferLength will then
 * be the same for all and decided by the first NdbEventOperation 
 * instantiation. Just make sure to instantiate the "largest" one first.
 *
 * Today all events INSERT/DELETE/UPDATE and all changed attributes are
 * sent to the API, even if only specific attributes have been specified.
 * These are however hidden from the user and only relevant data is shown
 * after next().  However false exits from pollEvents() may occur and thus
 * the subsequent next() will return zero, since there was no available
 * data. Just do pollEvents() again. Will be fixed in later versions.
 *
 * Event code does not check table schema version. Make sure to drop events
 * after table is dropped. Will be fixed in later
 * versions.
 *
 * On a replicated system one will receive each event 2 times, one for each
 * replica.  If a node fails events will not be received twice anymore
 * for data in corresponding fragment. Will be optimized in later versions.
 *
 * If a nodefailiure has occured not all events will be recieved
 * anymore. Drop NdbEventOperation and Create again after nodes are up
 * again. Will be fixed in later versions.
 *
 * Test status:
 * Tests have been run on 1-node and 2-node systems
 *
 * Known bugs:
 *
 * None, except if we can call some of the "isses" above bugs
 *
 * Useful API programs:
 *
 * select_all -d sys 'NDB$EVENTS_0'
 * Will show contents in the system table containing created events.
 *
 */
class NdbEventOperation {
public:
  enum State {CREATED,EXECUTING,ERROR};

  State getState();

  /**
   * Activates the NdbEventOperation to start receiving events. The
   * changed attribute values may be retrieved after next() has returned
   * a value greater than zero. The getValue() methods below must be called
   * prior to execute().
   *
   * @return 0 if successful otherwise -1.
   */
  int execute();

  // about the event operation
  // getting data
  //  NdbResultSet* getResultSet();

  /**
   * Defines a retrieval operation of an attribute value.
   * The NDB API allocate memory for the NdbRecAttr object that
   * will hold the returned attribute value. 
   *
   * @note Note that it is the applications responsibility
   *       to allocate enough memory for aValue (if non-NULL).
   *       The buffer aValue supplied by the application must be
   *       aligned appropriately.  The buffer is used directly
   *       (avoiding a copy penalty) only if it is aligned on a
   *       4-byte boundary and the attribute size in bytes
   *       (i.e. NdbRecAttr::attrSize times NdbRecAttr::arraySize is
   *       a multiple of 4).
   *
   * @note There are two versions, NdbOperation::getValue and
   *       NdbOperation::getPreValue for retrieving the current and
   *       previous value repectively.
   *
   * @note This method does not fetch the attribute value from 
   *       the database!  The NdbRecAttr object returned by this method 
   *       is <em>not</em> readable/printable before the 
   *       NdbEventConnection::execute has been made and
   *       NdbEventConnection::next has returned a value greater than
   *       zero. If a specific attribute has not changed the corresponding 
   *       NdbRecAttr will be in state UNDEFINED.  This is checked by 
   *       NdbRecAttr::isNull which then returns -1.
   *
   * @param anAttrName  Attribute name 
   * @param aValue      If this is non-NULL, then the attribute value 
   *                    will be returned in this parameter.<br>
   *                    If NULL, then the attribute value will only 
   *                    be stored in the returned NdbRecAttr object.
   * @return            An NdbRecAttr object to hold the value of 
   *                    the attribute, or a NULL pointer 
   *                    (indicating error).
   */
  NdbRecAttr *getValue(const char *anAttrName, char *aValue = NULL);
  NdbRecAttr *getPreValue(const char *anAttrName, char *aValue = NULL);

  /**
   * Retrieves event resultset if available, inserted into the NdbRecAttrs
   * specified in getValue() and getPreValue(). To avoid polling for
   * a resultset, one can use Ndb::pollEvents(int millisecond_timeout)
   * which will wait on a mutex until an event occurs or the specified
   * timeout occurs.
   *
   * @return >=0 if successful otherwise -1. Return value inicates number
   * of available events. By sending pOverRun one may query for buffer
   * overflow and *pOverRun will indicate the number of events that have
   * overwritten.
   */
  int next(int *pOverRun=NULL);

  /**
   * In the current implementation a nodefailiure may cause loss of events,
   * in which case isConsistent() will return false
   */
  bool isConsistent();

  /**
   * Query for occured event type.
   * NdbDictionary::Event::{TE_INSERT,TE_UPDATE,TE_DELETE}
   * Only valid after next() has returned value >= 0
   */
  NdbDictionary::Event::TableEvent getEventType();

  Uint32 getGCI();
  Uint32 getLatestGCI();
  void print();

private:
  friend class NdbEventOperationImpl;
  friend class Ndb;
  NdbEventOperation(Ndb *theNdb, const char* eventName,int bufferLength);
  ~NdbEventOperation();
  static int wait(void *p, int aMillisecondNumber);
  class NdbEventOperationImpl &m_impl;
  NdbEventOperation(NdbEventOperationImpl& impl);
};

typedef void (* NdbEventCallback)(NdbEventOperation*, Ndb*, void*);
#endif
