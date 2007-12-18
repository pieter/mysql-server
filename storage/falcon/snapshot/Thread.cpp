// Thread.cpp: implementation of the Thread class.
//
//////////////////////////////////////////////////////////////////////

#include "Snapshot.h"
#include "Thread.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Thread::Thread(Snapshot *snap)
{
	snapshot = snap;
	methodIndex = -1;
}

Thread::~Thread()
{

}

bool Thread::match(const char *pattern)
{
	const char *p, *q;

	for (p = text, q = pattern; *p && *q;)
		if (*p++ != *q++)
			return false;

	
	if (*q)
		return false;

	text = p;

	return true;
}

int Thread::getInt()
{
	int n = 0;

	while (*text >= '0' && *text <= '9')
		n = n * 10 + *text++ - '0';

	return n;
}

bool Thread::parse(const char **knownMethods)
{

	// Find thread definition line


	for (;;)
		{
		start = snapshot->getFileOffset();

		if ( !(text = snapshot->getLine()) )
			return false;

		if (match("Thread "))
			{
			threadNumber = getInt();

			if (match(" (Thread "))
				{
				threadId = getInt();

				break;
				}
			}
		}


	// Looking for something familiar

	for (bool hit = false; !hit && (text = snapshot->getLine()); )
		{
		if (match("   "))
			continue;

		if (text[0] != '#')
			break;

		for (const char **method = knownMethods; *method; ++method)
			if (find(*method))
				{
				methodIndex = method - knownMethods;
				hit = true;

				break;
				}
		}

	return true;
}

bool Thread::find(const char *pattern)
{
	const char *start = text;

	for (; *text; ++text)
		if (match(pattern))
			return true;

	text = start;

	return false;
}

void Thread::printTrace(const char *prefix)
{
	snapshot->seek(start);

	if ( !(text = snapshot->getLine()) )
		return;

	printf("%s\n", text);

	for (;;)
		{
		if ( !(text = snapshot->getLine()) )
			return;

		if (text[0] != '#' && text[0] != ' ')
			break;

		printf("%s%s", prefix, text);
		}

	printf("\n");

}
