/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#ifndef __DUMPER_H__
#define __DUMPER_H__

#include "condor_io.h"

// the dump object
/**
	Defines an abstraction for the output sink into which expressions and 
	values are unparsed.  The sink may be pointed to a string, a CEDAR
	socket, a file descriptor or a FILE *.
*/
class Sink
{
	public:
		// ctor/dtor
		Sink ();
		~Sink ();

        // Set the stream source for the parse.
		/** Points the sink to a string buffer specified by the user.  
				Operations which put information into the sink fail if the
				buffer is not large enough to store the object.
			@param buf Pointer to a buffer.
			@param sz The size of the buffer.
		*/
        void setSink (char *buf, int sz); // strings

		/** Points the sink to a string buffer which will be allocated by the
				sink object.  This method performs the necessary memory 
				management (i.e., resizing when necessary) to ensure that the 
				buffer is large enough to contain the representation of the 
				object.
			@param buf Reference to a character pointer, which will point to 
				the buffer containing the unparsed representation of the 
				object.  The previous value of the pointer is clobbered.
		*/
        void setSink (char *&buf );	// strings

		/** Points the sink to a CEDAR socket.
			@param sock The CEDAR socket.
		*/
        void setSink (Sock &sock);      // CEDAR

		/** Points the sink to a file descriptor.
			@param file_desc The file descriptor.
		*/
        void setSink (int file_desc);         // file descriptor

		/** Points the sink to a FILE *.
			@param file_ptr Pointer to the FILE structure to which the object
				will be dumped.
		*/
        void setSink (FILE *file_ptr);      // FILE structure

		// flush sink --- cleanup protocol
		/** Performs the cleanup protocol for the medium encapsulated  by the
			sink object, such as terminating the representation by a '\0'.
			@return false if the sink cannot accomodate the cleanup protocol,
				true otherwise.
		*/
		bool flushSink ();

    private:
		friend class ClassAdList;
		friend class ClassAd;
		friend class ExprTree;
		friend class Value;
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
