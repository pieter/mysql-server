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

#ifndef NdbOperation_H
#define NdbOperation_H

#include <ndb_types.h>
#include "ndbapi_limits.h"
#include "NdbError.hpp"
#include "NdbReceiver.hpp"

class Ndb;
class NdbApiSignal;
class NdbRecAttr;
class NdbOperation;
class NdbConnection;
class NdbColumnImpl;

/**
 * @class NdbOperation
 * @brief Class of operations for use in transactions.  
 */
class NdbOperation
{
  friend class Ndb;
  friend class NdbConnection;
  friend class NdbScanOperation;
  friend class NdbScanReceiver;
  friend class NdbScanFilter;
  friend class NdbScanFilterImpl;
  
public:
  /** 
   * @name Define Standard Operation Type
   * @{
   */

  /**
   * Define the NdbOperation to be a standard operation of type insertTuple.
   * When calling NdbConnection::execute, this operation 
   * adds a new tuple to the table.
   *
   * @return 0 if successful otherwise -1.
   */		
  virtual int 			insertTuple();
		
  /**
   * Define the NdbOperation to be a standard operation of type updateTuple.
   * When calling NdbConnection::execute, this operation 
   * updates a tuple in the table.
   *
   * @return 0 if successful otherwise -1.
   */  
  virtual int 			updateTuple();

  /**
   * Define the NdbOperation to be a standard operation of type writeTuple.
   * When calling NdbConnection::execute, this operation 
   * writes a tuple to the table.
   * If the tuple exists, it updates it, otherwise an insert takes place.
   *
   * @return 0 if successful otherwise -1.
   */  
  virtual int 			writeTuple();

  /**
   * Define the NdbOperation to be a standard operation of type deleteTuple.
   * When calling NdbConnection::execute, this operation 
   * delete a tuple.
   *
   * @return 0 if successful otherwise -1.
   */
  virtual int 			deleteTuple();
		
  /**
   * Define the NdbOperation to be a standard operation of type readTuple.
   * When calling NdbConnection::execute, this operation 
   * reads a tuple.
   *
   * @return 0 if successful otherwise -1.
   */  
  virtual int 			readTuple();				

  /**
   * Define the NdbOperation to be a standard operation of type 
   * readTupleExclusive.
   * When calling NdbConnection::execute, this operation 
   * read a tuple using an exclusive lock.
   *
   * @return 0 if successful otherwise -1.
   */
  virtual int 			readTupleExclusive();

  /**
   * Define the NdbOperation to be a standard operation of type 
   * simpleRead.
   * When calling NdbConnection::execute, this operation 
   * reads an existing tuple (using shared read lock), 
   * but releases lock immediately after read.
   *
   * @note  Using this operation twice in the same transaction
   *        may produce different results (e.g. if there is another
   *        transaction which updates the value between the
   *        simple reads).
   *
   * Note that simpleRead can read the value from any database node while
   * standard read always read the value on the database node which is 
   * primary for the record.
   *
   * @return 0 if successful otherwise -1.
   */
  virtual int			simpleRead();

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  /**
   * Define the NdbOperation to be a standard operation of type committedRead.
   * When calling NdbConnection::execute, this operation 
   * read latest committed value of the record.
   *
   * This means that if another transaction is updating the 
   * record, then the current transaction will not wait.  
   * It will instead use the latest committed value of the 
   * record.
   * dirtyRead is a deprecated name for committedRead
   *
   * @return 0 if successful otherwise -1.
   * @depricated
   */
  virtual int			dirtyRead();
#endif

  /**
   * Define the NdbOperation to be a standard operation of type committedRead.
   * When calling NdbConnection::execute, this operation 
   * read latest committed value of the record.
   *
   * This means that if another transaction is updating the 
   * record, then the current transaction will not wait.  
   * It will instead use the latest committed value of the 
   * record.
   *
   * @return 0 if successful otherwise -1.
   */
  virtual int			committedRead();

  /**
   * Define the NdbOperation to be a standard operation of type dirtyUpdate.
   * When calling NdbConnection::execute, this operation 
   * updates without two-phase commit.
   *
   * @return 0 if successful otherwise -1.
   */
  virtual int			dirtyUpdate();

  /**
   * Define the NdbOperation to be a standard operation of type dirtyWrite.
   * When calling NdbConnection::execute, this operation 
   * writes without two-phase commit.
   *
   * @return 0 if successful otherwise -1.
   */
  virtual int			dirtyWrite();

  /** @} *********************************************************************/
  /** 
   * @name Define Interpreted Program Operation Type
   * @{
   */

  /**
   * Update a tuple using an interpreted program.
   *
   * @return 0 if successful otherwise -1.
   */  
  virtual int			interpretedUpdateTuple();
		
  /**
   * Delete a tuple using an interpreted program.
   *
   * @return 0 if successful otherwise -1.
   */
  virtual int			interpretedDeleteTuple();

  /**
   * Scan a table to read tuples. 
   * 
   * The operation only sets a temporary read lock while 
   * reading the tuple.
   * The tuple lock is released when the result of the read reaches the
   * application.
   *
   * @param Parallelism   Number of parallel tuple reads are performed 
   *                      in the scan. 
   *                      Currently a maximum of 256 parallel tuple 
   *                      reads are allowed. 
   *                      The parallelism can in reality be lower
   *                      than specified
   *                      depending on the number of nodes
   *                      in the cluster
   * @return              0 if successful otherwise -1.
   */
  int			openScanRead(Uint32 Parallelism = 16 );

  /**
   * Scan a table to write or update tuples.
   *
   * The operation sets an exclusive lock on the tuple and sends the result 
   * to the application. 
   * Thus when the application reads the data, the tuple is
   * still locked with an exclusive lock.
   *
   * @param parallelism   Number of parallel tuple reads are performed 
   *                      in the scan. 
   *                      Currently a maximum of 256 parallel tuple 
   *                      reads are allowed. 
   *                      The parallelism can in reality be lower
   *                      than specified depending on the number
   *                      of nodes in the cluster
   * @return              0 if successful otherwise -1.
   *
   */
  int			openScanExclusive(Uint32 parallelism = 16);	

  /**
   * Scan a table to read tuples.
   * 
   * The operation only sets a read lock while 
   * reading the tuple.
   * Thus when the application reads the data, the tuple is
   * still locked with a read lock.
   *
   * @param parallelism   Number of parallel tuple reads are performed 
   *                      in the scan. 
   *                      Currently a maximum of 256 parallel tuple 
   *                      reads are allowed. 
   *                      The parallelism can in reality be lower
   *                      than specified
   *                      depending on the number of nodes
   *                      in the cluster
   * @return              0 if successful otherwise -1.
   */
  int                   openScanReadHoldLock(Uint32 parallelism = 16);

  /**
   * Scan a table to read tuples.
   * 
   * The operation does not wait for locks held by other transactions
   * but returns the latest committed tuple instead.
   *
   * @param parallelism   Number of parallel tuple reads are performed 
   *                      in the scan. 
   *                      Currently a maximum of 256 parallel tuple 
   *                      reads are allowed. 
   *                      The parallelism can in reality be lower
   *                      than specified
   *                      depending on the number of nodes
   *                      in the cluster
   * @return              0 if successful otherwise -1.
   */
  int                   openScanReadCommitted(Uint32 parallelism = 16);

  /** @} *********************************************************************/

  /**
   * @name Define Range Scan
   *
   * A range scan is a scan on an ordered index.  The operation is on
   * the index table but tuples are returned from the primary table.
   * The index contains all tuples where at least one index key has not
   * null value.
   *
   * A range scan is currently opened via a normal open scan method.
   * Bounds can be defined for each index key.  After setting bounds,
   * usual scan methods can be used (get value, interpreter, take over).
   * These operate on the primary table.
   *
   * @{
   */

  /**
   * Type of ordered index key bound.  The values (0-4) will not change
   * and can be used explicitly (e.g. they could be computed).
   */
  enum BoundType {
    BoundLE = 0,        ///< lower bound,
    BoundLT = 1,        ///< lower bound, strict
    BoundGE = 2,        ///< upper bound
    BoundGT = 3,        ///< upper bound, strict
    BoundEQ = 4         ///< equality
  };

  /**
   * Define bound on index key in range scan.
   *
   * Each index key can have not null lower and/or upper bound, or can
   * be set equal to not null value.  The bounds can be defined in any
   * order but a duplicate definition is an error.
   *
   * The scan is most effective when bounds are given for an initial
   * sequence of non-nullable index keys, and all but the last one is an
   * equality.  In this case the scan returns a contiguous range from
   * each ordered index fragment.
   *
   * @note      This release implements only the case described above,
   *            except for the non-nullable limitation.  Other sets of
   *            bounds return error or empty result set.
   *
   * @note      In this release a null key value satisfies any lower
   *            bound and no upper bound.  This may change.
   *
   * @param attrName    Attribute name, alternatively:
   * @param anAttrId    Index column id (starting from 0).
   * @param type        Type of bound
   * @param value       Pointer to bound value
   * @param len         Value length in bytes.
   *                    Fixed per datatype and can be omitted
   * @return            0 if successful otherwise -1
   */
  int setBound(const char* anAttrName, int type, const void* aValue, Uint32 len = 0);

  /**
   * Define bound on index key in range scan using index column id.
   * See the other setBound() method for details.
   */
  int setBound(Uint32 anAttrId, int type, const void* aValue, Uint32 len = 0);

  /** @} *********************************************************************/
  
  /**
   * Validate parallelism parameter by checking the number 
   * against number of executing Ndb nodes.
   *
   * @param Parallelism 
   * @return 0 if correct parallelism value, otherwise -1.
   *
   */
  int			checkParallelism(Uint32 Parallelism);  

  /**
   * Transfer scan operation to an updating transaction. Use this function 
   * when a scan has found a record that you want to update. 
   * 1. Start a new transaction.
   * 2. Call the function takeOverForUpdate using your new transaction 
   *    as parameter, all the properties of the found record will be copied 
   *    to the new transaction.
   * 3. When you execute the new transaction, the lock held by the scan will 
   *    be transferred to the new transaction(it's taken over).
   *
   * @note You must have started the scan with openScanExclusive
   *       to be able to update the found tuple.
   *
   * @param updateTrans the update transaction connection.
   * @return an NdbOperation or NULL.
   */
  NdbOperation*	takeOverForUpdate(NdbConnection* updateTrans);

  /**
   * Transfer scan operation to a deleting transaction. Use this function 
   * when a scan has found a record that you want to delete. 
   * 1. Start a new transaction.
   * 2. Call the function takeOverForDelete using your new transaction 
   *    as parameter, all the properties of the found record will be copied 
   *    to the new transaction.
   * 3. When you execute the new transaction, the lock held by the scan will 
   *    be transferred to the new transaction(its taken over).
   *
   * @note You must have started the scan with openScanExclusive
   *       to be able to delete the found tuple.
   *
   * @param deleteTrans the delete transaction connection.
   * @return an NdbOperation or NULL.
   */
  NdbOperation*	takeOverForDelete(NdbConnection* deleteTrans);

  /** 
   * @name Specify Search Conditions
   * @{
   */
  /**
   * Define a search condition with equality.
   * The condition is true if the attribute has the given value.
   * To set search conditions on multiple attributes,
   * use several equals (then all of them must be satisfied for the
   * tuple to be selected).
   *
   * @note There are 10 versions of NdbOperation::equal with
   *       slightly different parameters.
   *
   * @note When using NdbOperation::equal with a string (char *) as
   *       second argument, the string needs to be padded with 
   *       zeros in the following sense:
   *       @code
   *       // Equal needs strings to be padded with zeros
   *       strncpy(buf, str, sizeof(buf));
   *       NdbOperation->equal("Attr1", buf);
   *       @endcode
   * 
   * @param   anAttrName   Attribute name 
   * @param   aValue       Attribute value.
   * @param   len          Attribute length expressed in bytes.
   * @return               -1 if unsuccessful. 
   */
  int  equal(const char* anAttrName, const char* aValue, Uint32 len = 0);
  int  equal(const char* anAttrName, Uint32 aValue);	
  int  equal(const char* anAttrName, Int32 aValue);	
  int  equal(const char* anAttrName, Int64 aValue);	
  int  equal(const char* anAttrName, Uint64 aValue);
  int  equal(Uint32 anAttrId, const char* aValue, Uint32 len = 0);
  int  equal(Uint32 anAttrId, Int32 aValue);	
  int  equal(Uint32 anAttrId, Uint32 aValue);	
  int  equal(Uint32 anAttrId, Int64 aValue);	
  int  equal(Uint32 anAttrId, Uint64 aValue);
	
  /**
   * Generate a tuple id and set it as search argument.
   *
   * The Tuple id has NDB$TID as attribute name and 0 as attribute id.
   *
   * The generated tuple id is returned by the method.
   * If zero is returned there is an error.
   *
   * This is mostly used for tables without any primary key 
   * attributes.
   * 
   * @return    Generated tuple id if successful, otherwise 0.
   */
  Uint64       setTupleId();			

  /** @} *********************************************************************/
  /** 
   * @name Specify Attribute Actions for Operations
   * @{
   */

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
   * @note There are two versions of NdbOperation::getValue with
   *       slightly different parameters.
   *
   * @note This method does not fetch the attribute value from 
   *       the database!  The NdbRecAttr object returned by this method 
   *       is <em>not</em> readable/printable before the 
   *       transaction has been executed with NdbConnection::execute.
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
  NdbRecAttr* 		getValue(const char* anAttrName, char* aValue = 0);
  NdbRecAttr* 		getValue(Uint32 anAttrId, char* aValue = 0);
  
  /**
   * Define an attribute to set or update in query.
   *
   * To set a NULL value, use the following construct:
   * @code
   *   setValue("ATTR_NAME", (char*)NULL);
   * @endcode
   * 
   * There are a number of NdbOperation::setValue methods that 
   * take a certain type as input
   * (pass by value rather than passing a pointer). 
   * As the interface is currently implemented it is the responsibility 
   * of the application programmer to use the correct types.
   *
   * The NDB API will however check that the application sends
   * a correct length to the interface as given in the length parameter.  
   * The passing of char* as the value can contain any type or 
   * any type of array. 
   * If length is not provided or set to zero, 
   * then the API will assume that the pointer
   * is correct and not bother with checking it.
   *
   * @note There are 14 versions of NdbOperation::setValue with
   *       slightly different parameters.
   * 
   * @param anAttrName     Name (or Id) of attribute.
   * @param aValue         Attribute value to set.
   * @param len            Attribute length expressed in bytes.
   * @return               -1 if unsuccessful.
   */
  virtual int  setValue(const char* anAttrName, const char* aValue, 
			Uint32 len = 0);
  virtual int  setValue(const char* anAttrName, Int32 aValue);
  virtual int  setValue(const char* anAttrName, Uint32 aValue);
  virtual int  setValue(const char* anAttrName, Uint64 aValue);
  virtual int  setValue(const char* anAttrName, Int64 aValue);
  virtual int  setValue(const char* anAttrName, float aValue);
  virtual int  setValue(const char* anAttrName, double aValue);

  virtual int  setValue(Uint32 anAttrId, const char* aValue, Uint32 len = 0);
  virtual int  setValue(Uint32 anAttrId, Int32 aValue);
  virtual int  setValue(Uint32 anAttrId, Uint32 aValue);
  virtual int  setValue(Uint32 anAttrId, Uint64 aValue);
  virtual int  setValue(Uint32 anAttrId, Int64 aValue);
  virtual int  setValue(Uint32 anAttrId, float aValue);
  virtual int  setValue(Uint32 anAttrId, double aValue);
 
  /** @} *********************************************************************/
  /** 
   * @name Specify Interpreted Program Instructions
   * @{
   */

  /**
   * Interpreted program instruction: Add a value to an attribute.
   *
   * @note Destroys the contents of registers 6 and 7.
   *       (The instruction uses these registers for its operation.)
   *
   * @note There are four versions of NdbOperation::incValue with
   *       slightly different parameters.
   *
   * @param anAttrName     Attribute name.
   * @param aValue         Value to add.
   * @return               -1 if unsuccessful.
   */
  int   incValue(const char* anAttrName, Uint32 aValue);
  int   incValue(const char* anAttrName, Uint64 aValue);
  int   incValue(Uint32 anAttrId, Uint32 aValue);
  int   incValue(Uint32 anAttrId, Uint64 aValue);

  /**
   * Interpreted program instruction:
   * Subtract a value from an attribute in an interpreted operation.
   *
   * @note Destroys the contents of registers 6 and 7.
   *       (The instruction uses these registers for its operation.)
   *
   * @note There are four versions of NdbOperation::subValue with
   *       slightly different parameters.
   *
   * @param anAttrName    Attribute name.
   * @param aValue        Value to subtract.
   * @return              -1 if unsuccessful.
   */
  int   subValue(const char* anAttrName, Uint32 aValue);
  int   subValue(const char* anAttrName, Uint64 aValue);
  int   subValue(Uint32 anAttrId, Uint32 aValue);
  int   subValue(Uint32 anAttrId, Uint64 aValue);

  /**
   * Interpreted program instruction:
   * Define a jump label in an interpreted operation.
   *
   * @note The labels are automatically numbered starting with 0.  
   *       The parameter used by NdbOperation::def_label should 
   *       match the automatic numbering to make it easier to 
   *       debug the interpreted program.
   * 
   * @param labelNumber   Label number.
   * @return              -1 if unsuccessful.
   */
  int   def_label(int labelNumber);

  /**
   * Interpreted program instruction:
   * Add two registers into a third.
   *
   * @param RegSource1   First register.
   * @param RegSource2   Second register.
   * @param RegDest      Destination register where the result will be stored.
   * @return -1 if unsuccessful.
   */
  int   add_reg(Uint32 RegSource1, Uint32 RegSource2, Uint32 RegDest);

  /**
   * Interpreted program instruction:
   * Substract RegSource1 from RegSource2 and put the result in RegDest.
   *
   * @param RegSource1   First register.
   * @param RegSource2   Second register.
   * @param RegDest      Destination register where the result will be stored.
   * @return             -1 if unsuccessful.
   */
  int   sub_reg(Uint32 RegSource1, Uint32 RegSource2, Uint32 RegDest);

  /**
   * Interpreted program instruction:
   * Load a constant into a register.
   *
   * @param RegDest      Destination register.
   * @param Constant     Value to load.
   * @return             -1 if unsuccessful.
   */
  int   load_const_u32(Uint32 RegDest, Uint32 Constant);
  int   load_const_u64(Uint32 RegDest, Uint64 Constant);

  /**
   * Interpreted program instruction:
   * Load NULL value into a register.
   *
   * @param RegDest      Destination register.
   * @return             -1 if unsuccessful.
   */ 
  int   load_const_null(Uint32 RegDest);

  /**
   * Interpreted program instruction:
   * Read an attribute into a register.
   *
   * @param anAttrName   Attribute name.
   * @param RegDest      Destination register.
   * @return             -1 if unsuccessful.
   */
  int   read_attr(const char* anAttrName, Uint32 RegDest);

  /**
   * Interpreted program instruction:
   * Write an attribute from a register. 
   *
   * @param anAttrName   Attribute name.
   * @param RegSource    Source register.
   * @return             -1 if unsuccessful.
   */
  int   write_attr(const char* anAttrName, Uint32 RegSource);

  /**
   * Interpreted program instruction:
   * Read an attribute into a register.
   *
   * @param anAttrId the attribute id.
   * @param RegDest the destination register.
   * @return -1 if unsuccessful.
   */
  int   read_attr(Uint32 anAttrId, Uint32 RegDest);

  /**
   * Interpreted program instruction:
   * Write an attribute from a register. 
   *
   * @param anAttrId the attribute id.
   * @param RegSource the source register.
   * @return -1 if unsuccessful.
   */
  int   write_attr(Uint32 anAttrId, Uint32 RegSource);

  /**
   * Interpreted program instruction:
   * Define a search condition. Last two letters in the function name 
   * describes the search condition.
   * The condition compares RegR with RegL and therefore appears
   * to be reversed.
   *
   * - ge RegR >= RegL
   * - gt RegR >  RegL
   * - le RegR <= RegL
   * - lt RegR <  RegL
   * - eq RegR =  RegL
   * - ne RegR <> RegL
   *
   * @param RegLvalue left value. 
   * @param RegRvalue right value.
   * @param Label the label to jump to.
   * @return -1 if unsuccessful.
   */
  int   branch_ge(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  int   branch_gt(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  int   branch_le(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  int   branch_lt(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  int   branch_eq(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  int   branch_ne(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);

  /**
   * Interpreted program instruction:
   * Jump to Label if RegLvalue is not NULL.
   *
   * @param RegLvalue the value to check.
   * @param Label the label to jump to.
   * @return -1 if unsuccessful. 
   */
  int   branch_ne_null(Uint32 RegLvalue, Uint32 Label);

  /**
   * Interpreted program instruction:
   * Jump to Label if RegLvalue is equal to NULL.
   *
   * @param  RegLvalue  Value to check.
   * @param  Label      Label to jump to.
   * @return -1 if unsuccessful. 
   */
  int   branch_eq_null(Uint32 RegLvalue, Uint32 Label);

  /**
   * Interpreted program instruction:
   * Jump to Label.
   *
   * @param  Label  Label to jump to.
   * @return -1 if unsuccessful.
   */
  int   branch_label(Uint32 Label);

  /**
   * Interpreted program instruction:  branch after memcmp
   * @param  ColId   Column to check
   * @param  Label   Label to jump to
   * @return -1 if unsuccessful
   */
  int branch_col_eq_null(Uint32 ColId, Uint32 Label);
  int branch_col_ne_null(Uint32 ColId, Uint32 Label);

  /**
   * Interpreted program instruction:  branch after memcmp
   * @param  ColId   column to check
   * @param  val     search value
   * @param  len     length of search value   
   * @param  nopad   force non-padded comparison for a Char column
   * @param  Label   label to jump to
   * @return -1 if unsuccessful
   */
  int branch_col_eq(Uint32 ColId, const char * val, Uint32 len, 
		    bool nopad, Uint32 Label);
  int branch_col_ne(Uint32 ColId, const char * val, Uint32 len, 
		    bool nopad, Uint32 Label);
  int branch_col_lt(Uint32 ColId, const char * val, Uint32 len, 
		    bool nopad, Uint32 Label);
  int branch_col_le(Uint32 ColId, const char * val, Uint32 len, 
		    bool nopad, Uint32 Label);
  int branch_col_gt(Uint32 ColId, const char * val, Uint32 len, 
		    bool nopad, Uint32 Label);
  int branch_col_ge(Uint32 ColId, const char * val, Uint32 len, 
		    bool nopad, Uint32 Label);
  int branch_col_like(Uint32 ColId, const char *, Uint32 len, 
		      bool nopad, Uint32 Label);
  int branch_col_notlike(Uint32 ColId, const char *, Uint32 len, 
			 bool nopad, Uint32 Label);
  
  /**
   * Interpreted program instruction: Exit with Ok
   *
   * For scanning transactions,
   * end interpreted operation and return the row to the application.
   *
   * For non-scanning transactions,
   * exit interpreted program.
   *
   * @return -1 if unsuccessful.
   */
  int	interpret_exit_ok();

  /**
   * Interpreted program instruction: Exit with Not Ok
   *
   * For scanning transactions, 
   * continue with the next row without returning the current row.
   *
   * For non-scanning transactions,
   * abort the whole transaction.
   *
   * @note A method also exists without the error parameter.
   * 
   * @param ErrorCode   An error code given by the application programmer.
   * @return            -1 if unsuccessful.
   */
  int   interpret_exit_nok(Uint32 ErrorCode);
  int   interpret_exit_nok();

  /**
   * Interpreted program instruction:
   * Define a subroutine in an interpreted operation.
   *
   * @param SubroutineNumber the subroutine number.
   * @return -1 if unsuccessful.
   */
  int   def_subroutine(int SubroutineNumber);

  /**
   * Interpreted program instruction:
   * Call a subroutine.
   *
   * @param Subroutine the subroutine to call.
   * @return -1 if unsuccessful. 
   */
  int   call_sub(Uint32 Subroutine);

  /**
   * Interpreted program instruction:
   * End a subroutine.
   *
   * @return -1 if unsuccessful. 
   */
  int   ret_sub();

  /** @} *********************************************************************/

  /** 
   * @name Error Handling
   * @{
   */

  /**
   * Get the latest error code.
   *
   * @return error code.
   */
  const NdbError & getNdbError() const;

  /**
   * Get the method number where the error occured.
   * 
   * @return method number where the error occured.
   */
  int getNdbErrorLine();

  /** @} *********************************************************************/

  /**
   * Type of operation
   */
  enum OperationType { 
    ReadRequest = 0,              ///< Read operation
    UpdateRequest = 1,            ///< Update Operation
    InsertRequest = 2,            ///< Insert Operation
    DeleteRequest = 3,            ///< Delete Operation
    WriteRequest = 4,             ///< Write Operation
    ReadExclusive = 5,            ///< Read exclusive
    OpenScanRequest,              ///< Scan Operation
    OpenRangeScanRequest,         ///< Range scan operation
    NotDefined2,                  ///< Internal for debugging
    NotDefined                    ///< Internal for debugging
  };

protected:
/******************************************************************************
 * These are the methods used to create and delete the NdbOperation objects.
 *****************************************************************************/
  			NdbOperation(Ndb* aNdb);	
  			virtual ~NdbOperation();

  bool                  needReply();
/******************************************************************************
 * These methods are service routines used by the other NDB API classes.
 *****************************************************************************/
//--------------------------------------------------------------
// Initialise after allocating operation to a transaction		      
//--------------------------------------------------------------
  int init(class NdbTableImpl*, NdbConnection* aCon);
  
  void  initScan();		// Initialise after allocating operation
				       	// to a scan transaction
  virtual void	releaseScan();		// Release scan parts of transaction
  void releaseSignals();
  void releaseScanSignals();
  void prepareNextScanResult();

  // Common part for Read and Exclusive
  int openScan(Uint32 aParallelism, bool, bool, bool); 

  void	next(NdbOperation*);		// Set next pointer		      

  NdbOperation*	    next();	        // Get next pointer		       

  enum OperationStatus{ 
    Init,                       
    OperationDefined,
    TupleKeyDefined,
    GetValue,
    SetValue,
    ExecInterpretedValue,
    SetValueInterpreted,
    FinalGetValue,
    SubroutineExec,
    SubroutineEnd,
    SetBound,
    WaitResponse,
    WaitCommitResponse,
    Finished,
    ReceiveFinished
  };

  OperationStatus   Status();	         	// Read the status information
  
  void		    Status(OperationStatus);    // Set the status information

  void		    NdbCon(NdbConnection*);	// Set reference to connection
  						// object.

  virtual void	    release();			// Release all operations 
                                                // connected to
					      	// the operations object.      
  void		    setStartIndicator();

/******************************************************************************
 * The methods below is the execution part of the NdbOperation
 * class. This is where the NDB signals are sent and received. The
 * operation can send TC[KEY/INDX]REQ, [INDX]ATTRINFO. 
 * It can receive TC[KEY/INDX]CONF, TC[KEY/INDX]REF, [INDX]ATTRINFO. 
 * When an operation is received in its fulness or a refuse message 
 * was sent, then the connection object is told about this situation.
 *****************************************************************************/

  int    doSend(int ProcessorId, Uint32 lastFlag);
  int	 doSendScan(int ProcessorId);

  int	 prepareSendScan(Uint32 TC_ConnectPtr,	
			 Uint64 TransactionId);

  virtual int	 prepareSend(Uint32  TC_ConnectPtr,
                             Uint64  TransactionId);
  virtual void   setLastFlag(NdbApiSignal* signal, Uint32 lastFlag);
    
  int	 prepareSendInterpreted();            // Help routine to prepare*
   
  void	 TCOPCONF(Uint32 anNdbColumnImplLen);      // Handle TC[KEY/INDX]CONF signal
  
  int	 receiveTCKEYREF(NdbApiSignal*); 


  int    receiveTRANSID_AI(const Uint32* aDataPtr, Uint32 aDataLength); 
  int    receiveREAD_CONF(const Uint32* aDataPtr, Uint32 aDataLength); 


  int	 checkMagicNumber();		// Verify correct object

  int    checkState_TransId(NdbApiSignal* aSignal);

/******************************************************************************
 *	These are support methods only used locally in this class.
******************************************************************************/

  virtual int equal_impl(const NdbColumnImpl* anAttrObject, 
                         const char* aValue, 
                         Uint32 len);
  NdbRecAttr* getValue(const NdbColumnImpl* anAttrObject, char* aValue = 0);
  int setValue(const NdbColumnImpl* anAttrObject, const char* aValue, Uint32 len);
  int incValue(const NdbColumnImpl* anAttrObject, Uint32 aValue);
  int incValue(const NdbColumnImpl* anAttrObject, Uint64 aValue);
  int subValue(const NdbColumnImpl* anAttrObject, Uint32 aValue);
  int subValue(const NdbColumnImpl* anAttrObject, Uint64 aValue);
  int read_attr(const NdbColumnImpl* anAttrObject, Uint32 RegDest);
  int write_attr(const NdbColumnImpl* anAttrObject, Uint32 RegSource);
  int branch_reg_reg(Uint32 type, Uint32, Uint32, Uint32);
  int branch_col(Uint32 type, Uint32, const char *, Uint32, bool, Uint32 Label);
  int branch_col_null(Uint32 type, Uint32 col, Uint32 Label);
  int setBound(const NdbColumnImpl* anAttrObject, int type, const void* aValue, Uint32 len);
  
  // Handle ATTRINFO signals   
  int         receiveREAD_AI(Uint32* aDataPtr, Uint32 aLength); 
				
  int 	      insertATTRINFO(Uint32 aData);
  int         insertATTRINFOloop(const Uint32* aDataPtr, Uint32 aLength);
  int         getFirstATTRINFOScan();
  int         saveBoundATTRINFO();

  int 	      insertKEYINFO(const char* aValue,	
			    Uint32 aStartPosition,	
			    Uint32 aKeyLenInByte,	
			    Uint32 anAttrBitsInLastWord);
  
  virtual void setErrorCode(int aErrorCode);
  virtual void setErrorCodeAbort(int aErrorCode);

  void        handleFailedAI_ElemLen();	   // When not all attribute data
                                           // were received

  int	      incCheck(const NdbColumnImpl* anAttrObject);
  int	      initial_interpreterCheck();
  int	      intermediate_interpreterCheck();
  int	      read_attrCheck(const NdbColumnImpl* anAttrObject);
  int	      write_attrCheck(const NdbColumnImpl* anAttrObject);
  int	      labelCheck();
  int	      insertCall(Uint32 aCall);
  int	      insertBranch(Uint32 aBranch);

  Uint32 ptr2int() { return theReceiver.getId(); };

  NdbOperation*	 
  takeOverScanOp(OperationType opType, NdbConnection* updateTrans);

/******************************************************************************
 * These are the private variables that are defined in the operation objects.
 *****************************************************************************/

  NdbReceiver theReceiver;

  NdbError theError;			// Errorcode	       
  int 	   theErrorLine;		// Error line       

  Ndb*		   theNdb;	      	// Point back to the Ndb object.
  NdbConnection*   theNdbCon;	       	// Point back to the connection object.
  NdbOperation*	   theNext;	       	// Next pointer to operation.
  NdbOperation*    theNextScanOp;
  NdbApiSignal*	   theTCREQ;		// The TC[KEY/INDX]REQ signal object
  NdbApiSignal*	   theFirstATTRINFO;	// The first ATTRINFO signal object 
  NdbApiSignal*	   theCurrentATTRINFO;	// The current ATTRINFO signal object  
  Uint32	   theTotalCurrAI_Len;	// The total number of attribute info
  		      			// words currently defined    
  Uint32	   theAI_LenInCurrAI;	// The number of words defined in the
		      		     	// current ATTRINFO signal
  NdbApiSignal*	   theFirstKEYINFO;	// The first KEYINFO signal object 
  NdbApiSignal*	   theLastKEYINFO;	// The first KEYINFO signal object 

  NdbRecAttr*	    theFirstRecAttr;	 // The first receive attribute object  
  NdbRecAttr*	    theCurrentRecAttr;	 // The current receive attribute object

  class NdbLabel*	    theFirstLabel;
  class NdbLabel*	    theLastLabel;
  class NdbBranch*	    theFirstBranch;
  class NdbBranch*	    theLastBranch;
  class NdbCall*	    theFirstCall;
  class NdbCall*	    theLastCall;
  class NdbSubroutine*    theFirstSubroutine;
  class NdbSubroutine*    theLastSubroutine;
  Uint32	    theNoOfLabels;
  Uint32	    theNoOfSubroutines;

  Uint32*           theKEYINFOptr;       // Pointer to where to write KEYINFO
  Uint32*           theATTRINFOptr;      // Pointer to where to write ATTRINFO

  Uint32    	    theTotalRecAI_Len;	 // The total length received according
				         // to the TCKEYCONF signal    
  Uint32	    theCurrRecAI_Len;	 // The currently received length   
  Uint32	    theAI_ElementLen;	 // How many words long is this element 
  Uint32*	    theCurrElemPtr;   	 // The current pointer to the element  
  class NdbTableImpl* m_currentTable;      // The current table
  class NdbTableImpl* m_accessTable;

  // Set to TRUE when a tuple key attribute has been defined. 
  Uint32	    theTupleKeyDefined[NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY][3];

  Uint32	    theTotalNrOfKeyWordInSignal;     // The total number of
  						     // keyword in signal.

  Uint32	    theTupKeyLen;	   // Length of the tuple key in words
  Uint32	    theNoOfTupKeyDefined;  // The number of tuple key attributes
		       			   // currently defined   
  OperationType	  theOperationType;        // Read Request, Update Req......   

  Uint8        theLockMode;	   // Can be set to WRITE if read operation 
  OperationStatus theStatus;	   // The status of the operation.	
  Uint32         theMagicNumber;  // Magic number to verify that object 
                                   // is correct
  Uint32 theScanInfo;      	   // Scan info bits (take over flag etc)
  Uint32 theDistrKeySize;         // Distribution Key size if used
  Uint32 theDistributionGroup;    // Distribution Group if used

  Uint32 theSubroutineSize;	   // Size of subroutines for interpretation
  Uint32 theInitialReadSize;	   // Size of initial reads for interpretation
  Uint32 theInterpretedSize;	   // Size of interpretation
  Uint32 theFinalUpdateSize;	   // Size of final updates for interpretation
  Uint32 theFinalReadSize;	   // Size of final reads for interpretation

  Uint8  theStartIndicator;	   // Indicator of whether start operation
  Uint8  theCommitIndicator;	   // Indicator of whether commit operation
  Uint8  theSimpleIndicator;	   // Indicator of whether simple operation
  Uint8  theDirtyIndicator;	   // Indicator of whether dirty operation
  Uint8  theInterpretIndicator;   // Indicator of whether interpreted operation
  Uint8  theDistrGroupIndicator;  // Indicates whether distribution grp is used
  Uint8  theDistrGroupType;       // Type of distribution group used
  Uint8  theDistrKeyIndicator;    // Indicates whether distr. key is used

  Uint16 m_tcReqGSN;
  Uint16 m_keyInfoGSN;
  Uint16 m_attrInfoGSN;

  // Scan related variables
  Uint32            theParallelism;
  NdbScanReceiver** theScanReceiversArray;
  NdbApiSignal*     theSCAN_TABREQ;
  NdbApiSignal*     theFirstSCAN_TABINFO_Send;
  NdbApiSignal*     theLastSCAN_TABINFO_Send;
  NdbApiSignal*     theFirstSCAN_TABINFO_Recv;
  NdbApiSignal*     theLastSCAN_TABINFO_Recv;
  NdbApiSignal*     theSCAN_TABCONF_Recv;
  // saveBoundATTRINFO() moves ATTRINFO here when setBound() is ready
  NdbApiSignal*     theBoundATTRINFO;
  Uint32            theTotalBoundAI_Len;

};

inline
int
NdbOperation::checkMagicNumber()
{
  if (theMagicNumber != 0xABCDEF01){
#ifdef NDB_NO_DROPPED_SIGNAL
    abort();
#endif
    return -1;
  }
  return 0;
}

inline
void
NdbOperation::setStartIndicator()
{
  theStartIndicator = 1;
}

inline
int
NdbOperation::getNdbErrorLine()
{
  return theErrorLine;
}

/******************************************************************************
void next(NdbOperation* aNdbOperation);

Parameters:    aNdbOperation: Pointers to the NdbOperation object.
Remark:        Set the next variable of the operation object.
******************************************************************************/
inline
void
NdbOperation::next(NdbOperation* aNdbOperation)
{
  theNext = aNdbOperation;
}

/******************************************************************************
NdbOperation* next();

Return Value:  	Return  next pointer to NdbOperation object.
Remark:         Get the next variable of the operation object.
******************************************************************************/
inline
NdbOperation*
NdbOperation::next()
{
  return theNext;
}

/******************************************************************************
OperationStatus  Status();

Return Value    Return the OperationStatus.	
Parameters:     aStatus:  The status.
Remark:         Sets Operation status. 
******************************************************************************/
inline
NdbOperation::OperationStatus			
NdbOperation::Status()
{
  return theStatus;
}

/******************************************************************************
void  Status(OperationStatus aStatus);

Parameters:     aStatus: The status.
Remark:         Sets Operation
 status. 
******************************************************************************/
inline
void			
NdbOperation::Status( OperationStatus aStatus )
{
  theStatus = aStatus;
}

/******************************************************************************
void NdbCon(NdbConnection* aNdbCon);

Parameters:    aNdbCon: Pointers to NdbConnection object.
Remark:        Set the reference to the connection in the operation object.
******************************************************************************/
inline
void
NdbOperation::NdbCon(NdbConnection* aNdbCon)
{
  theNdbCon = aNdbCon;
}

inline
int
NdbOperation::equal(const char* anAttrName, Int32 aPar)
{
  return equal(anAttrName, (const char*)&aPar, (Uint32)4);
}

inline
int
NdbOperation::equal(const char* anAttrName, Uint32 aPar)
{
  return equal(anAttrName, (const char*)&aPar, (Uint32)4);
}

inline
int
NdbOperation::equal(const char* anAttrName, Int64 aPar)
{
  return equal(anAttrName, (const char*)&aPar, (Uint32)8);
}

inline
int
NdbOperation::equal(const char* anAttrName, Uint64 aPar)
{
  return equal(anAttrName, (const char*)&aPar, (Uint32)8);
}

inline
int
NdbOperation::equal(Uint32 anAttrId, Int32 aPar)
{
  return equal(anAttrId, (const char*)&aPar, (Uint32)4);
}

inline
int
NdbOperation::equal(Uint32 anAttrId, Uint32 aPar)
{
  return equal(anAttrId, (const char*)&aPar, (Uint32)4);
}

inline
int
NdbOperation::equal(Uint32 anAttrId, Int64 aPar)
{
  return equal(anAttrId, (const char*)&aPar, (Uint32)8);
}

inline
int
NdbOperation::equal(Uint32 anAttrId, Uint64 aPar)
{
  return equal(anAttrId, (const char*)&aPar, (Uint32)8);
}

inline
int
NdbOperation::setValue(const char* anAttrName, Int32 aPar)
{
  return setValue(anAttrName, (const char*)&aPar, (Uint32)4);
}

inline
int
NdbOperation::setValue(const char* anAttrName, Uint32 aPar)
{
  return setValue(anAttrName, (const char*)&aPar, (Uint32)4);
}

inline
int
NdbOperation::setValue(const char* anAttrName, Int64 aPar)
{
  return setValue(anAttrName, (const char*)&aPar, (Uint32)8);
}

inline
int
NdbOperation::setValue(const char* anAttrName, Uint64 aPar)
{
  return setValue(anAttrName, (const char*)&aPar, (Uint32)8);
}

inline
int
NdbOperation::setValue(const char* anAttrName, float aPar)
{
  return setValue(anAttrName, (const char*)&aPar, (Uint32)4);
}

inline
int
NdbOperation::setValue(const char* anAttrName, double aPar)
{
  return setValue(anAttrName, (const char*)&aPar, (Uint32)8);
}

inline
int
NdbOperation::setValue(Uint32 anAttrId, Int32 aPar)
{
  return setValue(anAttrId, (const char*)&aPar, (Uint32)4);
}

inline
int
NdbOperation::setValue(Uint32 anAttrId, Uint32 aPar)
{
  return setValue(anAttrId, (const char*)&aPar, (Uint32)4);
}

inline
int
NdbOperation::setValue(Uint32 anAttrId, Int64 aPar)
{
  return setValue(anAttrId, (const char*)&aPar, (Uint32)8);
}

inline
int
NdbOperation::setValue(Uint32 anAttrId, Uint64 aPar)
{
  return setValue(anAttrId, (const char*)&aPar, (Uint32)8);
}

inline
int
NdbOperation::setValue(Uint32 anAttrId, float aPar)
{
  return setValue(anAttrId, (char*)&aPar, (Uint32)4);
}

inline
int
NdbOperation::setValue(Uint32 anAttrId, double aPar)
{
  return setValue(anAttrId, (const char*)&aPar, (Uint32)8);
}

#endif


