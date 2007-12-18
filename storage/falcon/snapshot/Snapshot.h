// Snapshot.h: interface for the Snapshot class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SNAPSHOT_H__C25B78B7_141E_4698_9892_829F41F131D5__INCLUDED_)
#define AFX_SNAPSHOT_H__C25B78B7_141E_4698_9892_829F41F131D5__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <stdio.h>

#ifndef NULL
#define NULL		0
#endif

class Thread;

class Snapshot  
{
public:
	void seek (long position);
	long getFileOffset();
	const char* getLine();
	void close();
	void analyze (const char *fileName);
	Snapshot();
	virtual ~Snapshot();

	FILE	*inputFile;
	Thread	*threads;
	int		lineNumber;
	char	buffer[1024];
};

#endif // !defined(AFX_SNAPSHOT_H__C25B78B7_141E_4698_9892_829F41F131D5__INCLUDED_)
