// Snapshot.cpp: implementation of the Snapshot class.
//
//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include "Snapshot.h"
#include "Thread.h"

int main (int argc, const char **argv)
{
	Snapshot snapshot;
	snapshot.analyze(argv[1]);

	return 0;
}

const char *methods[] = {
	"SRLUpdateRecords::append",
	"SRLUpdateIndex::append",
	"Transaction::getRelativeState",
	"Table::databaseFetch",
	"Server::getConnections",
	"__read_nocancel",
	"Database::ticker",
	"PageWriter::writer",
	"SerialLogTransaction::commit",
	"SerialLog::gopherThread",
	"Scheduler::schedule",
	"handle_connections_sockets",
	"Table::retireRecords",
	"SerialLogFile::write",
	"SerialLog::flush",
	"SerialLog::preUpdate",
	"IO::readPage",
	"IO::writePage",
	"Cache::ioThread",
	"Section::insertStub",
	"Index::addToDIHash",
	"Index::scanDIHash",
	"Gopher::gopherThread",
	NULL
	};

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

Snapshot::Snapshot()
{
	inputFile = NULL;
	threads = NULL;
}

Snapshot::~Snapshot()
{
	close();
}

void Snapshot::analyze(const char *fileName)
{
	close();
	lineNumber = 0;

	if ( !(inputFile = fopen(fileName, "rt")) )
		{
		perror(fileName);

		return;
		}

	Thread *thread;
	int count = 0;
	int unaccounted = 0;
	int counts[100];
	memset(counts, 0, sizeof(counts));

	for (;;)
		{
		thread = new Thread(this);

		if (!thread->parse(methods))
			{
			delete thread;
			
			break;
			}

		thread->next = threads;
		threads = thread;
		++count;

		if (thread->methodIndex >= 0 && thread->methodIndex < sizeof(counts) / sizeof(counts[0]))
			++counts[thread->methodIndex];
		else
			++unaccounted;
		}

	printf("Threads unaccounted for:\n\n", unaccounted);

	for (thread = threads; thread; thread = thread->next)
		if (thread->methodIndex < 0)
			thread->printTrace("    ");

	printf("\n%d threads in known state:\n\n", count);

	for (int n = 0; methods[n]; ++n)
		if (counts[n])
			printf( "  %3d  %s\n", counts[n], methods[n]);


	close();
}

void Snapshot::close()
{
	if (inputFile)
		{
		fclose(inputFile);
		inputFile = NULL;
		}

	for (Thread *thread; thread = threads;)
		{
		threads = thread->next;
		delete thread;
		}
}

const char* Snapshot::getLine()
{
	if (!fgets(buffer, sizeof(buffer), inputFile))
		return NULL;

	++lineNumber;

	return buffer;
}

long Snapshot::getFileOffset()
{
	return lineNumber;
}

void Snapshot::seek(long position)
{
	rewind(inputFile);

	for (long n = 0; n < position; ++n)
		getLine();
}
