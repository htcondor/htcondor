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

#include "common.h"
#include <string>

BEGIN_NAMESPACE( classad )

/** The ByteSource class serves as an abstract input source for classad 
	expressions, and can be extended for custom situations.  A derived class 
	only needs to implement the 'virtual bool _GetChar(int &)' method to
	customize the source.
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

/** The ByteSink class serves as an abstract output sink for classad 
	expressions, and can be extended for custom situations.  A derived class 
	only needs to implement the 'virtual bool _PutBytes( const void*, int )'
	method to customize the sink.
*/
class ByteSink {
	public:
		/// Destructor
		virtual ~ByteSink( ){ };

		/** Sets the terminal character to be written to the sink.  The 
		 	terminal character is written to the sink just before it is flushed.
		*/
		inline void SetTerminal( int t=0 ) { terminal = (char) t; };

		/** Sets the maximum length (size) of the sink.  If more bytes are
		 	attempted to be written to the sink than the length, further
			PutBytes() operations return with failure.  If the length is -1,
			there is no limit imposed on the sink size.
		*/
		inline void SetLength( int l=-1 ) { length = l; };

		/** Resets the byte count for the sink.  The counter keeps track of
		 	the number of bytes written to the sink.
		*/
		inline void Reset( ) { index = 0; }

		/** Pushes the buffer into the sink.
		 	@param buf The buffer to be written to the sink
			@param buflen The size of the buffer
		*/
		bool PutBytes( const void* buf, int buflen );

		/** Performs the clean-up protocol for the sink
		 	@return true if the operation succeeded, false otherwise
		*/
		bool Flush( );

	protected:
		/// Constructor
		ByteSink( ) { terminal = 0; length = -1; index = 0; };

		/** The main method to be implemented by classes that extend ByteSink
		 	@param buf The buffer to be transferred to the sink
			@param buflen The length of the buffer
			@return true if the operation succeeded, false otherwise
		*/
		virtual bool _PutBytes( const void *buf, int buflen )=0;

		/** This method will be called when the serialization is complete.
		 	Clean-up protocols should be implemented here.
			@return true if the operation succeeded, false otherwise
		*/
		virtual bool _Flush( ) { return true; };

		/// The terminal character (initialized to '\0')
		char terminal;

		/// The maximum size of the sink (initialized to -1)
		int length;

		/// The number of bytes written to the sink thus far
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

//------------------------------------------------------------------------
/** ByteStream is a container for ByteSource and SyteSink.  It transforms
    two one-way channels into a single duplex channel.  The public
    meathods of ByteSource and ByteSink are exposed through this
    interface, unless it is appriopriate to combine them into one meathod.
    */
class ByteStream {
  public:
    ///
    virtual ~ByteStream () { delete m_src; delete m_snk; }
    ///
    inline void SetSentinel  (int s = 0)  { m_src->SetSentinel(s); }
    ///
    inline void SetTerminal  (int t = 0)  { m_snk->SetTerminal(t); }
    ///
    inline void SetLengthSrc (int L = -1) { m_src->SetLength(L); }
    ///
    inline void SetLengthSnk (int L = -1) { m_snk->SetLength(L); }
    ///
    inline void ResetSrc () { m_src->Reset(); }
    ///
    inline void ResetSnk () { m_snk->Reset(); }
    ///
    inline bool GetChar  (int & ch) { return m_src->GetChar(ch); }
    ///
    inline bool PutBytes (const void * buf, int buflen) {
        return m_snk->PutBytes(buf,buflen);
    }
    ///
    inline bool Flush () { return m_snk->Flush(); }

    ///
    enum SockTypeEnum {
        /** */ Sock_NULL    = 0,
        /** */ Sock_STREAM  = 1,
        /** */ Sock_DGRAM   = 2
    };

    ///
    bool Connect (std::string url, int socktype = Sock_STREAM) {
        m_socktype = socktype;
        return _Connect (url);
    }

    ///
    inline int SockType () const { return m_socktype; }

    ///
    bool Close () {
        m_socktype = Sock_NULL;
        return _Close();
    }

  protected:
    ///
    ByteStream () : m_socktype(Sock_NULL) {}
    ///
    ByteSource * m_src;
    ///
    ByteSink   * m_snk;

    ///
    virtual bool _Connect (std::string url) = 0;

    ///
    virtual bool _Close () = 0;

    ///
    int m_socktype;
};

///
class FileStream : public ByteStream {
  public:
    ///
    FileStream () {
        m_src = new FileSource;
        m_snk = new FileSink;
    }
    ///
    virtual ~FileStream () {}
    ///
    inline void InitSrc (FILE *fp, int len    = -1) {
        ((FileSource*)m_src)->Initialize(fp,len);
    }
    ///
    inline void InitSnk (FILE *fp, int maxlen = -1) {
        ((FileSink*)m_snk)->Initialize(fp,maxlen);
    }

  protected:
    ///
    virtual bool _Connect (std::string url);

    ///
    virtual bool _Close ();
};

///
class FileDescStream : public ByteStream {
  public:
    ///
    FileDescStream () {
        m_src = new FileDescSource;
        m_snk = new FileDescSink;
    }
    ///
    virtual ~FileDescStream () {}
    ///
    inline void InitSrc (int fd, int len) {
        ((FileDescSource*) m_src)->Initialize (fd, len);
    }
    ///
    inline void InitSnk (int fd, int maxlen) {
        ((FileDescSink*) m_snk)->Initialize (fd, maxlen);
    }

  protected:
    ///
    virtual bool _Connect (std::string url);

    ///
    virtual bool _Close ();
};

END_NAMESPACE // classad

#endif
