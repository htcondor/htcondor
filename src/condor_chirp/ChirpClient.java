
package edu.wisc.cs.condor.chirp;

import java.io.*;
import java.net.*;

public class ChirpClient {

	private File file=null;
	private Socket socket=null;
	private PrintWriter output=null;
	private BufferedReader input=null;

	/**
	Connect and authenticate to the default Chirp server.
	Determine the "default" from a variety of environmental concerns.
	@throws IOException
	*/

	public ChirpClient() throws IOException {
		ChirpConfig config = new ChirpConfig("chirp.config");
		connect(config.getHost(),config.getPort());
		cookie(config.getCookie());
	}

	/**
	Connect to a given Chirp server.
	The caller must still pass a cookie before using any other methods.
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
		output = new PrintWriter(socket.getOutputStream());
		input = new BufferedReader(new InputStreamReader(socket.getInputStream()));
	}

	/**
	Present a 'cookie' string to a Chirp server.
	This call must be done before any other Chirp calls.
	If it is not, other methods are likely to throw exceptions indicating "not authenticated."
	@param c The cookie to present.
	@throws IOException, ChirpException
	*/

	public void cookie( String c ) throws IOException {
		int response;
		try {
			output.println("cookie "+c);
			output.flush();
			response = getResponse();
		} catch( IOException e ) {
			throw new ChirpException(e);
		}
		returnOrThrow(response);
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
		<ul>
	@param mode If created , the initial UNIX access mode.
	@throws IOException, ChirpException
	*/

	public int open( String path, String flags, int mode ) throws IOException {
		int response;
		try {
			output.println("open "+path+" "+flags+" "+mode);
			output.flush();
			response = getResponse();
		} catch( IOException e ) {
			throw new ChirpException(e);
		}
		return returnOrThrow(response);
	}

	/**
	Close a file.
	@param fd The file descriptor to close.
	@throws IOException, ChirpException
	*/

	public void close( int fd ) throws IOException {
		int response;
		try {
			output.println("close "+fd);
			output.flush();
			response = getResponse();
		} catch( IOException e ) {
			throw new ChirpException(e);
		}
		returnOrThrow(response);
	}

	/**
	Read data from a file.  This method is free to read any number of bytes less than or equal to the parameter 'length'.  A result of zero indicates end of file.
	@param fd The file descriptor to read.
	@param buffer The data buffer to fill.
	@param pos The position in the buffer to start.
	@param length The maximum number of elements to read.
	@returns The number of elements actually read.
	@throws IOException, ChirpException
	*/

	public int read( int fd, char [] buffer, int pos, int length ) throws IOException {
		int response,actual;

		try {
			output.println("read "+fd+" "+length);
			output.flush();
			response = getResponse();
			if(response>0) {
				actual = fullRead(buffer,pos,length);
				if(actual!=length) throw new ChirpException();
			}
		} catch( IOException e ) {
			throw new ChirpException(e);
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
	@throws IOException, ChirpException
	*/

	public int write( int fd, char [] buffer, int pos, int length ) throws IOException {
		int response;
		try {
			output.println("write "+fd+" "+length);
			output.write(buffer,pos,length);
			output.flush();
			response = getResponse();
		} catch( IOException e ) {
			throw new ChirpException(e);
		}
		return returnOrThrow(response);
	}

	public static final int SEEK_SET=0;
	public static final int SEEK_CUR=1;
	public static final int SEEK_END=2;

	/**	
	Seek within an open file.
	@param fd The file descriptor to modify.
	@param offset The number of bytes to change.
	@param whence The source of the seek: SEEK_SET, SEEK_CUR, or SEEK_END.
	@returns The new file position, measured from the beginning of the file.
	@throws IOException, ChirpException
	*/

	public int lseek( int fd, int offset, String whence ) throws IOException {
		int response;
		try {
			output.println("seek "+fd+" "+offset+" "+whence);
			output.flush();
			response = getResponse();
		} catch( IOException e ) {
			throw new ChirpException(e);
		}
		return returnOrThrow(response);
	}

	/**	
	Delete a file.
	@param name The name of the file.
	@throws IOException, ChirpException
	*/

	public void unlink( String name ) throws IOException {
		int response;
		try {
			output.println("unlink "+name);
			output.flush();
			response = getResponse();
		} catch( IOException e ) {
			throw new ChirpException(e);
		}
		returnOrThrow(response);
	}

	/**	
	Rename a file.
	@param name The old name.
	@param newname The new name.
	@throws IOException, ChirpException
	*/

	public void rename( String name, String newname ) throws IOException {
		int response;
		try {
			output.println("rename "+name+" "+newname);
			output.flush();
			response = getResponse();
		} catch( IOException e ) {
			throw new ChirpException(e);
		}
		returnOrThrow(response);
	}

	private int returnOrThrow( int code ) throws IOException {
		if(code>=0) {
			return code;
		} else switch(code) {
			case -1:
				throw new IOException("couldn't authenticate");
			case -2:
			case -3:
				throw new FileNotFoundException();
			case -4:
				throw new IOException("file already exists");
			case -5:
				throw new IOException("argument too big");
			case -6:
				throw new IOException("out of space");
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
				throw new IOException("unknown error");
		}
	}

	private int fullRead( char [] buffer, int offset, int length ) throws IOException {
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
		String line;

		line = input.readLine();
		if(line==null) throw new ProtocolException("server disconnected unexpectedly");
		return Integer.parseInt(line);
	}
}

class ChirpException extends RuntimeException {
	ChirpException( String s ) {
		super("Chirp: exception while performing RPC: "+ e.toString());
	}
}



