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

#include "common.h"
#include "classad_io.h"
#include "formatOptions.h"

BEGIN_NAMESPACE( classad )

class ClassAdCollection;

/**
	Defines an abstraction for the output sink into which expressions and 
	values are unparsed.  The sink may be pointed to a string, a CEDAR
	socket, a file descriptor or a FILE *.
*/
class Sink
{
	public:
		/// Constructor
		Sink ();

		/// Destructor
		~Sink ();

        // Set the stream source for the parse.
		/** Points the sink to a string buffer specified by the user.  
				Operations which put information into the sink fail if the
				buffer is not large enough to store the object.
			@param buf Pointer to a buffer.
			@param sz The size of the buffer.
		*/
        void SetSink (char *buf, int sz); // strings

		/** Points the sink to a string buffer which will be allocated by the
				sink object.  This method performs the necessary memory 
				management (i.e., resizing when necessary) to ensure that the 
				buffer is large enough to contain the representation of the 
				object.
			@param buf Reference to a character pointer, which will point to 
				the buffer containing the unparsed representation of the 
				object.  If useInitial is true, buf is assumed to point to
				an initial buffer of size len which may be later resized.
			@param len Reference to an integer, which contains the size of
				the buffer buf.  If useInitial is true, this parameter should
				contain the size of the buffer buf.
			@param useInitial true if the initial values of buf and len are
				to be used as an initial buffer.  Otherwise, these values are
				discarded.
		*/
        void SetSink (char *&buf, int &len, bool useInitial );	// strings

		/** Points the sink to a file descriptor.
			@param file_desc The file descriptor.
		*/
        void SetSink (int file_desc);         // file descriptor

		/** Points the sink to a FILE *.
			@param file_ptr Pointer to the FILE structure to which the object
				will be dumped.
		*/
        void SetSink (FILE *file_ptr);      // FILE structure

		/** Points the sink at a ByteSink object
		 	@param s Pointer to a ByteSink object
			@see ByteSink
		*/
		void SetSink( ByteSink *s );

		ByteSink *GetSink( ) { return( sink ); }

		/** Set the format options for the sink
		 	@param fo Pointer to a FormatOptions object
			@see FormatOptions
		*/
		void SetFormatOptions( FormatOptions *fo ) { pp = fo; }

		/** Get the FormatOptions for this sink
			@return Pointer to the current FormatOptions object.  This pointer
				should not be deleted.
		*/
		FormatOptions *GetFormatOptions( ) { return pp; }

		/** Set the terminating character which will be pushed to the sink
		    when the flushSink() method is called.  (This character is '\0'
			by default.)  When not '\0', this character is usually the 
			sentinel character required by the Source that will parse this
			Sink's output.  In the case of string sinks, the terminating 
			character is followed with a '\0'.
			@param c The terminal character.
		*/
		void SetTerminalChar( int c );
			
		/** Performs the cleanup protocol for the medium encapsulated  by the
			sink object, such as terminating the representation by a '\0'.
			@return false if the sink cannot accomodate the cleanup protocol,
				true otherwise.
		*/
		bool FlushSink ();

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
		friend class FormatOptions;
		friend class ClassAdCollection;

		FormatOptions *pp;

		// call back from ExprTree and ClassAd to write the data to the sink
		bool SendToSink( void *, int );

		// sink types
		ByteSink	*sink;
};

END_NAMESPACE // classad

#endif//__DUMPER_H__
