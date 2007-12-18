class RefObject 
{
public:
	RefObject();
	virtual ~RefObject;
	virtual void	addRef();
	virtual void	release();
	int				useCount();
};

class SEError
{
public:
	virtual int			getErrorNumber() = 0;
	virtual const char*	getErrorText() = 0;
	virtual void		setError(int errorNumber, const char* errorText) = 0;
};

class SEEngine : public RefObject
{
	virtual SEConnection*	createConnection(SEError *errObject) = 0;
	virtual SETable*		findTable(SEError *errObject, const char *schemaName, const char* tableName, ...) = 0;
	virtual SETable*		createTable(SEError *errObject, const char *schemaName, const char* tableName, ...) = 0;
};

class SETable : public RefObject
{
	virtual int				rename(SEError *errObject, const char* newSchema, const char* newName) = 0;
	virtual int				destroy(SEError *errObject) = 0;
	virtual SEIndex*		findIndex(SEError *errObject, ...) = 0;
	
	// And all sorts of alter methods
};

class SEConnection : public RefObject
{
	virtual int				startTransaction(SEError *errObject, int isolationMode) = 0;
	virtual int				prepareTransaction(SEError *errObject, XID *xid) = 0;
	virtual int				commitTransaction(SEError *errObject) = 0;
	virtual int				rollbackTransaction(SEError *errObject) = 0;
	
	virtual int				startStatement(SEError *errObject)= 0;
	virtual int				endStatement(SEError *errObject)= 0;
	virtual int				rollbackStatement(SEError *errObject)= 0;
	
	virtual void*			createSavePoint(SEError *errObject) = 0;
	virtual int				releaseSavePoint(SEError *errObject, void *savePoint) = 0;
	virtual int				rollbackSavePoint(SEError *errObject, void *savePoint) = 0;
	
	virtual SETableOp*		createTableOperation(SEError *errObject, SETable *table) = 0;
};

class SETableOp : public RefObject
{
	int						reset(SEError *errObject) = 0;
	int						restrictRange(SEError *errObject, SEIndex *index, int mode,
										  int numberLowerSegments, Value **lowerBoundSegments,
										  int numberUpperSegments, Value **upperBoundSegments)
	int						expandRange(SEError *errObject, SEIndex *index, int mode,
										  int numberLowerSegments, Value **lowerBoundSegments,
										  int numberUpperSegments, Value **upperBoundSegments)
	int						open(SEError *errObject) = 0;
	Record*					fetch(SEError *errObject) = 0;
	int						close(SEError *errObject) = 0;
	
	Record*					allocateRecord(SEError *errObject) = 0;
	int						insert(SEError *errObject, Record *newRecord) = 0;
	int						update(SEError *errObject, Record *oldRecord, Record *newRecord) = 0;
	int						deleteRecord(SEError *errObject, Record *record) = 0;
	
	// plus table locking and foreign key operations
};

class Record : public RefObject
{
	virtual int				getIdentifierLength();
	virtual int				getIdentifier(unsigned char *identifier) = 0;
	virtual int				getValue(SEError *errObject, int fieldId, Value *value) = 0;
	virtual int				setValue(SEError *errObject, int fieldId, Value *value) = 0;
	virtual int				setValues(SEError *errObject, Record *existingRecord) = 0;
};

class Value
{
	virtual short		getShort(int scale = 0);
	virtual int			getInt(int scale = 0);
	virtual int64		getQuad(int scale = 0);
	virtual double		getDouble();
	virtual int			getTruncatedString(int bufferSize, char* buffer);
	virtual void		getBigInt(BigInt *bigInt);
	virtual Clob*		getClob();
	virtual TimeStamp	getTimestamp();
	virtual Time		getTime();
	virtual DateTime	getDate();
	virtual void		getStream (Stream *stream, bool copyFlag);

	virtual const char	*getString();
	virtual int			getString (int bufferSize, char *buffer);
	virtual const char* getString (char **tempPtr);
	virtual int			getStringLength();

	virtual void		setValue (double value);
	virtual void		setValue (int32, int scale = 0);
	virtual void		setValue (Value *value, bool copyFlag);
	virtual void		setValue( Blob * blb);
	virtual void		setValue (Clob *blob);
	virtual void		setValue (Type type, Value *value);
	virtual void		setValue (Time value);
	virtual void		setValue (BigInt *value);
	virtual void		setValue (int64 value, int scale = 0);
	virtual void		setValue (short value, int scale = 0);
	virtual void		setValue (DateTime value);
	virtual void		setValue (TimeStamp value);

	virtual void		setString (int length, const char *string, bool copy);
	virtual void		setString (const char *value, bool copy);
	virtual void		setString (const WCString *value);

	virtual void		setNull();
	virtual void		setBinaryBlob (int32 blobId);
	virtual void		setAsciiBlob (int32 blobId);
	virtual void		setDate (int32 value);
	virtual BigInt*		setBigInt(void);
	
	virtual int			compare (Value *value);
	virtual int			compareBlobs (Blob *blob1, Blob *blob2);
	virtual int			compareClobs (Clob *clob1, Clob *clob2);
	
	// etc., etc., etc.
};

