/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

/*
Chirp Java Client
*/

package edu.wisc.cs.condor.chirp;

import java.io.*;
import java.net.*;

/*
XXX XXX XXX
WARNING WARNING WARNING (Condor maintainers)
If you change this file, then you must compile it
and check the .jar file into CVS.  Why?  Because
we can't manage a Java installation on all of our platforms,
but we still want compiled Java code distributed with
all platforms.
*/

/**
A ChirpClient object represents the connection between a client and
a Chirp server.  The methods of this object correspond to RPCs in
the Chirp protocol, and are very similar to standard UNIX I/O operations.
Those looking for a more Java-style stream interface to Chirp should
see the ChirpInputStream and ChirpOutputStream objects.
*/

public class ChirpClient {

	private Socket socket=null;
	private OutputStream output=null;
	private InputStream input=null;
	final private String encoding = "US-ASCII";

	/**
	Connect and authenticate to the default Chirp server.
	Determine the "default" from a variety of environmental concerns.
	If running within Condor, then Condor will set up the environment
	to proxy I/O through the starter back to the submit site.
	@throws IOException
	*/

	public ChirpClient() throws IOException {
		ChirpConfig config;
		try {
			String filename = System.getProperty(".chirp.config");
			if(filename==null) throw new ChirpError("system property chirp.config is not defined!");
			config = new ChirpConfig(filename);
		} catch( Exception e ) {
			throw new ChirpError(e);
		}
		connect(config.getHost(),config.getPort());
		cookie(config.getCookie());
	}

	/**
	Connect to a given Chirp server.
	The caller must still pass a cookie before using any other methods.
	Condor users should use the no-argument constructor instead.
	@param host The server host.
	@param port The server port.
	@throws IOException
	*/

	public ChirpClient( String host, int port ) throws IOException {
		connect(host,port);
	}

	private void connect( String host, int port ) throws IOException {
		String line;
		int response;

		socket = new Socket(host,port);
		output = socket.getOutputStream();
		input = socket.getInputStream();
	}

	/**
	Present a 'cookie' string to a Chirp server.
	This call must be done before any other Chirp calls.
	If it is not, other methods are likely to throw exceptions indicating "not authenticated."
	@param c The cookie to present.
	@throws IOException
	*/

	public void cookie( String c ) throws IOException {
		simple_command("cookie "+ChirpWord(c)+"\n");	
	}

	/**
	Open a file.
	@returns An integer file descriptor that may be used with later calls.
	@param file The name of the file to open.
	@param flags A string of characters that state how the file is to be used.
		<ul>
		<li> r - open for reading
		<li> w - open for writing
		<li> t - truncate before use
		<li> c - create if it does not exist, succeed otherwise
		<li> x - modifies 'c' to fail if the file already exists
		<li> a - modifies 'w' to always append
		</ul>
	@param mode If created , the initial UNIX access mode.
	@throws IOException
	*/

	public int open( String path, String flags, int mode ) throws IOException {
		return simple_command("open "+ChirpWord(path)+" "+flags+" "+mode+"\n");
	}

	/**
	Same as the three-argument open, but the server selects a default initial UNIX mode.
	*/

	public int open( String path, String flags ) throws IOException {
		return open(path,flags,0777);
	}

	/**
	Close a file.
	@param fd The file descriptor to close.
	@throws IOException
	*/

	public void close( int fd ) throws IOException {
		simple_command("close "+fd+"\n");
	}

	/**
	Read data from a file.  This method is free to read any number of bytes less than or equal to the parameter 'length'.  A result of zero indicates end of file.
	@param fd The file descriptor to read.
	@param buffer The data buffer to fill.
	@param pos The position in the buffer to start.
	@param length The maximum number of elements to read.
	@returns The number of elements actually read.
	@throws IOException
	*/

	public int read( int fd, byte [] buffer, int pos, int length ) throws IOException {
		int response,actual;

		try {
			String line = "read "+fd+" "+length+"\n";
			byte [] bytes = line.getBytes(encoding);
			output.write(bytes,0,bytes.length);
			output.flush();
			response = getResponse();
			if(response>0) {
				actual = fullRead(buffer,pos,response);
				if(actual!=response) throw new ChirpError("server disconnected");
			}
		} catch( IOException e ) {
			throw new ChirpError(e);
		}
		return returnOrThrow(response);
	}

	/**	
	Write data to a file.  This method is free to write any number of elements less than or equal to the parameter 'length'.  A result of zero indicates end of file.

	@param fd The file descriptor to write.
	@param buffer The data buffer to use.
	@param pos The position in the buffer to start.
	@param length The maximum number of elements to write.
	@returns The number of elements actually written.
	@throws IOException
	*/

	public int write( int fd, byte [] buffer, int pos, int length ) throws IOException {
		int response;

		try {
			String line = "write "+fd+" "+length+"\n";
			byte [] bytes = line.getBytes(encoding);
			output.write(bytes,0,bytes.length);
			output.write(buffer,pos,length);
			output.flush();
			response = getResponse();
		} catch( IOException e ) {
			throw new ChirpError(e);
		}
		return returnOrThrow(response);
	}

	/** Seek from the beginning of a file. */
	public static final int SEEK_SET=0;

	/** Seek from the current position. */
	public static final int SEEK_CUR=1;

	/** Seek from the end of a file. */
	public static final int SEEK_END=2;

	/**	
	Seek within an open file.
	@param fd The file descriptor to modify.
	@param offset The number of bytes to change.
	@param whence The source of the seek: SEEK_SET, SEEK_CUR, or SEEK_END.
	@returns The new file position, measured from the beginning of the file.
	@throws IOException
	*/

	public int lseek( int fd, int offset, int whence ) throws IOException {
		return simple_command("seek "+fd+" "+offset+" "+whence+"\n");
	}

	/**	
	Delete a file.
	@param name The name of the file.
	@throws IOException
	*/

	public void unlink( String name ) throws IOException {
		simple_command("unlink "+ChirpWord(name)+"\n");
	}

	/**	
	Rename a file.
	@param name The old name.
	@param newname The new name.
	@throws IOException
	*/

	public void rename( String name, String newname ) throws IOException {
		simple_command("rename "+ChirpWord(name)+" "+ChirpWord(newname)+"\n");
	}


	/**	
	Create a directory.
	@param name The directory name.
	@param mode The initial UNIX access mode.
	@throws IOException
	*/

	public void mkdir( String name, int mode ) throws IOException {
		simple_command("mkdir "+ChirpWord(name)+" "+mode+"\n");
	}

	/**	
	Create a directory.
	The server selects default initial permissions for the directory.
	@param name The directory name.
	@throws IOException
	*/

	public void mkdir( String name ) throws IOException {
		mkdir(name,0777);
	}

	/**	
	Delete a directory.
	@param name The directory name.
	@throws IOException
	*/

	public void rmdir( String name ) throws IOException {
		simple_command("rmdir "+ChirpWord(name)+"\n");
	}

	/**
	This call blocks until all outstanding operations have
	been committed to stable storage.
	@param fd The file descriptor to sync.
	@throws IOException
	*/

	public void fsync( int fd ) throws IOException {
		simple_command("fsync "+fd+"\n");
	}

	public int version() throws IOException {
		return simple_command("version\n");
	}

	public String lookup( String path ) throws IOException {
		String url = null;
		int response = simple_command("lookup "+ChirpWord(path)+"\n");
		if(response>0) {
			byte [] buffer = new byte[response];
			int actual = fullRead(buffer,0,response);
			if(actual!=response) throw new ChirpError("server disconnected");
			url = new String(buffer,0,response,encoding);
		}
		returnOrThrow(response);
		return url;
	}

	public void constrain( String expr ) throws IOException {
		simple_command("constrain "+" "+ChirpWord(expr)+"\n");
	}

	public String get_job_attr( String name ) throws IOException {
		String value = null;
		int response = simple_command("get_job_attr "+ChirpWord(name)+"\n");
		if(response>0) {
			byte [] buffer = new byte[response];
			int actual = fullRead(buffer,0,response);
			if(actual!=response) throw new ChirpError("server disconnected");
			value = new String(buffer,0,response,encoding);
		}
		returnOrThrow(response);
		return value;
	}

	public void set_job_attr( String name, String expr ) throws IOException {
		simple_command("set_job_attr "+ChirpWord(name)+" "+ChirpWord(expr)+"\n");	
	}

	private int simple_command( String cmd ) throws IOException {
		int response;
		try {
			byte [] bytes = cmd.getBytes(encoding);
			output.write(bytes,0,bytes.length);
			output.flush();
			response = getResponse();
		} catch( IOException e ) {
			throw new ChirpError(e);
		}
		return returnOrThrow(response);
	}

	public String ChirpWord( String cmd ) {
		StringBuffer buf = new StringBuffer();

		for(int i=0; i<cmd.length(); i++) {
			char ch = cmd.charAt(i);
			switch(ch) {
			case '\\':
			case ' ':
			case '\n':
			case '\t':
			case '\r':
				buf.append("\\");
				//fall through
			default:
			    buf.append(ch);
			}
		}

		return buf.toString();
        }

	private int returnOrThrow( int code ) throws IOException {
		if(code>=0) {
			return code;
		} else switch(code) {
			case -1:
				throw new ChirpError("couldn't authenticate");
			case -2:
				throw new IOException("permission denied");
			case -3:
				throw new FileNotFoundException();
			case -4:
				throw new IOException("file already exists");
			case -5:
				throw new IOException("argument too big");
			case -6:
				throw new ChirpError("out of space");
			case -7:
				throw new OutOfMemoryError();
			case -8:
				throw new IOException("invalid request");
			case -9:
				throw new IOException("too many files open");
			case -10:
				throw new IOException("file is busy");
			case -11:
				throw new IOException("try again");
			default:
				throw new ChirpError("unknown error");
		}
	}

	private int fullRead( byte [] buffer, int offset, int length ) throws IOException {
		int total=0;
		int actual;

		while(length>0) {
			actual = input.read(buffer,offset,length);
			if(actual==0) {
				break;
			} else {
				offset += actual;
				length -= actual;
				total += actual;
			}
		}
		return total;
	}

	private int getResponse() throws IOException {
		int response;
		String line="";
		String digit;
		byte [] b = new byte[1];

		while(true) {
			b[0] = (byte) input.read();
			digit = new String(b,0,1,encoding);

			if(digit.charAt(0)=='\n') {
				if(line.length()>0) {
					return Integer.parseInt(line);
				} else {
					continue;
				}
			} else {
				line = line+digit;
			}
		}
	}
}

class ChirpError extends Error {
	ChirpError( String s ) {
		super("Chirp: "+ s);
	}
	ChirpError( Exception e ) {
		super("Chirp: " +e.toString());
	}

}



