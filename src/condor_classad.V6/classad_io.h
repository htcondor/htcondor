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

#ifndef CLASSAD_IO
#define CLASSAD_IO

/** The ByteSource class serves as an abstract input source for classad 
	expressions, and can be extended for custom situations.  The class
	a derived class only needs to implement the 'virtual bool _GetChar(int &)'
	method
*/
class ByteSource {
	public:
		/// Destructor
		virtual ~ByteSource( ){ };

		/** Sets the sentinel character for the byte source.  The sentinel is
		 	treated like an EOF for the stream.
			@param s The sentinel character
			@see Source::SetSentinelChar
		*/
		inline void SetSentinel( int s=0 ) { sentinel = s; };
		
		/** Sets the maximum number of characters to be read from the stream.
		 	If this parameter is set to -1, the source will be read till a
			'\0' or EOF (or the sentinel character (if sepcified)) is 
			encountered.
			@param l The maximum length of the stream
		*/
		inline void SetLength( int l=-1 ) { length = l; };
		
		/** Resets the byte count of the stream.  This counter keeps track of
		 	the number of characters read from the stream.
		*/
		inline void Reset( ) { index = 0; }
		
		/** Gets the next character from the stream.  This is the external
		 	interface function to obtain a character from the stream.
			@param ch The next character from the stream, or undefined if
				an error was encountered.  (EOF counts as success, with
				ch==EOF
			@return true if the operation succeeded, or false if it failed.
		*/
		bool GetChar( int &ch );

	protected:
		/// Constructor
		ByteSource( ) { sentinel = 0; length = -1; index = 0; };
		
		/** The method to be implemented by classes that extend ByteSource
		 	@param ch The character read from the source
			@return true if the operation succeeded, or false if it failed.
			  (EOF counts as success with ch==EOF)
		*/
		virtual bool _GetChar( int &ch )=0;

		/// The sentinel character (initialized to '\0')
		int sentinel;
		
		/// The maximum length of the stream (initialized to -1)
		int length;
		
		/// The number of characters read from the stream thus far
		int index;
};


class ByteSink {
	public:
		virtual ~ByteSink( ){ };
		inline void SetTerminal( int t=0 ) { terminal = (char) t; };
		inline void SetLength( int l=-1 ) { length = l; };
		bool PutBytes( const void* buf, int buflen );
		bool Flush( );

	protected:
		ByteSink( ) { terminal = 0; length = -1; index = 0; };
		virtual bool _PutBytes( const void *, int )=0;
		virtual bool _Flush( ) { return true; };

		char terminal;
		int length;
		int index;
};


class StringSource : public ByteSource {
	public:
		StringSource( );
		virtual ~StringSource( );

		void Initialize( const char *, int=-1 );
		bool _GetChar( int & );

	private:
		const char *buffer;
};

class FileSource : public ByteSource {
	public:
		FileSource( );
		virtual ~FileSource( );

		void Initialize( FILE*, int=-1 );
		bool _GetChar( int & );

	private:
		FILE	*file;
};

class FileDescSource : public ByteSource {
	public:
		FileDescSource( );
		virtual ~FileDescSource( );

		void Initialize( int, int=-1 );
		bool _GetChar( int & );

	private:
		int	fd;
};

class StringSink : public ByteSink {
	public:
		StringSink( );
		virtual ~StringSink( );

		void Initialize( char *buf, int maxlen );
		bool _PutBytes( const void *, int );
		bool _Flush( );

	private:
		char *buffer;
};


class StringBufferSink : public ByteSink {
	public:
		StringBufferSink( );
		virtual ~StringBufferSink( );

		void Initialize( char *&buf, int &sz, int maxlen=-1 );
		bool _PutBytes( const void *, int );
		bool _Flush( );

	private:
		char **bufref;
		int  *bufSize;
};


class FileSink : public ByteSink {
	public:
		FileSink( );
		virtual ~FileSink( );

		void Initialize( FILE *f, int maxlen=-1 );
		bool _PutBytes( const void *, int );
		bool _Flush( );

	private:
		FILE *file;
};


class FileDescSink : public ByteSink {
	public:
		FileDescSink( );
		virtual ~FileDescSink( );

		void Initialize( int fd, int maxlen=-1 );
		bool _PutBytes( const void *, int );

	private:
		int fd;
};

#endif
