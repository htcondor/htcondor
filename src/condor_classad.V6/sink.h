#ifndef __DUMPER_H__
#define __DUMPER_H__

#include "condor_io.h"

// the dump object
class Sink
{
	public:
		// ctor/dtor
		Sink ();
		~Sink ();

        // Set the stream source for the parse
        void setSink (char *&, int&); 	// strings
        void setSink (Sock &);      	// CEDAR
        void setSink (int);           	// file descriptor
        void setSink (FILE *);        	// FILE structure

		// flush sink --- cleanup protocol
		bool flushSink ();

    private:
		friend class ClassAdList;
		friend class ClassAd;
		friend class ExprTree;
		friend class EvalValue;
		friend class Literal;
		friend class AttributeReference;
		friend class Operation;	
		friend class FunctionCall;
		friend class ExprList;
		friend class ValueList;

		// call back from ExprTree and ClassAd to write the data to the sink
		bool sendToSink (void *, int);

		// sink types
		enum { NO_SINK, STRING_SINK, FILE_SINK, FILE_DESC_SINK, CEDAR_SINK };
		int	 sink;

		// the actual sinks
		FILE 	*file;
		int		fd;
		Sock	*sock;

		// for the string sink
		char 	**bufRef;
		char	*buffer;
		int		*bufLenRef;
		int		bufLen;
		int		last;
};

#endif//__DUMPER_H__
