
package edu.wisc.cs.condor.chirp;

import java.io.*;
import java.net.*;

/**
A ChirpOutputStream gives a sequential binary interface to a write-only
file. Users that require random-access I/O should see ChirpClient.
Users requiring a character-oriented interface to a Chirp file
should make use of an OutputStreamWriter like so:
<pre>
OutputStreamWriter in = new OutputStreamWriter(new ChirpOutputStream(filename));
</pre> 
*/

public class ChirpOutputStream extends java.io.OutputStream {
	private ChirpClient client;
	private String path;
	private int fd;

	/**
	Create a new output stream attached to the named file.
	Use the Chirp server implicitly defined by the environment.
	@param The file name.
	@throws IOException
	*/

	public ChirpOutputStream( String p ) throws IOException {
		client = new ChirpClient();
		path = p;
		fd = client.open(path,"wtc",0777);
	}

	/**
	Create a new output stream attached to the named file.
	Use the named Chirp server host and port.
	Condor users should use the single-argument constructor instead.
	@param host The server name.
	@param port The server port number.
	@param p The file name.
	@throws IOException
	*/

	public ChirpOutputStream( String host, int port, String p ) throws IOException {
		client = new ChirpClient(host,port);
		path = p;
		fd = client.open(path,"wtc",0777);
	}

	/**
	Write bytes to the stream.
	@param buffer The buffer to write.
	@param pos The starting position in the buffer.
	@param length The number of bytes to write.
	@returns The number of bytes actually written.
	@throws IOException
	*/
	
	public void write( byte [] buffer, int pos, int length ) throws IOException {
		while( length>0 ) {
			int actual = client.write(fd,buffer,pos,length);
			length -= actual;
			pos += actual;
		}
	}

	/**
	Write bytes to the stream.
	@param buffer The buffer to write.
	@returns The number of bytes actually written.
	@throws IOException
	*/

	public void write( byte [] buffer ) throws IOException {
		write(buffer,0,buffer.length);
	}

	/**
	Write one byte to the stream.
	@throws IOException
	*/

	public void write( int b ) throws IOException {
		byte [] temp = new byte[1];
		temp[0] = (byte)b;
		write(temp,0,1);
	}

	/**
	Complete access to this file.
	@throws IOException
	*/

	public void close() throws IOException {
		client.close(fd);
	}

	/**
	Flushes any remaining buffered data.
	*/

	public void flush() {
	}
}

