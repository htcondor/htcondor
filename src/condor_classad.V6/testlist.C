#include "iostream.h"
#include "stdio.h"
#include "condor_classad.h"

extern "C" int SetSyscalls () { return 0; }

int main (void)
{
	ClassAdList *list;
	Source		source;
	Sink		sink;

	source.setSource (stdin);
	sink.setSink (stdout);

	list = ClassAdList::fromSource(source);
	if (!list)
	{
		cerr << "Error" << endl;
		exit (1);
	}

	list->toSink(sink);
	
	exit (0);
}	

	
