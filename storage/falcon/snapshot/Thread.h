// Thread.h: interface for the Thread class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_THREAD_H__664784B4_27B7_45BF_A6D1_A7E438066E15__INCLUDED_)
#define AFX_THREAD_H__664784B4_27B7_45BF_A6D1_A7E438066E15__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "JString.h"

class Thread  
{
public:
	void printTrace(const char* prefix);
	bool	find (const char *pattern);
	bool	parse(const char **knownMethods);
	int		getInt();
	bool	match(const char *pattern);
	Thread(Snapshot *snapshot);
	virtual ~Thread();

	Thread		*next;
	Snapshot	*snapshot;
	const char	*text;
	int			threadNumber;
	int			threadId;
	int			methodIndex;
	long		start;
};

#endif // !defined(AFX_THREAD_H__664784B4_27B7_45BF_A6D1_A7E438066E15__INCLUDED_)
