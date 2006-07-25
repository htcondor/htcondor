/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
/*
Chirp Java Client

This public domain software is provided "as is".  See the Chirp License
for details.
*/

package edu.wisc.cs.condor.chirp;

import java.io.*;
import java.net.*;

/*
XXX XXX XXX
WARNING WARNING WARNING
If you change this file, then you must compile it
and check the .jar file into CVS.  Why?  Because
we can't manage a Java installation on all of our platforms,
but we still want compiled Java code distributed with
all platforms.
*/

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

	private void init( String host, int port, String p, boolean append ) throws IOException {
		if(host!=null) {
			client = new ChirpClient(host,port);
		} else {
			client = new ChirpClient();
		}
		path = p;
		if(append) {
			fd = client.open(path,"wac",0777);
		} else {
			fd = client.open(path,"wtc",0777);
		}
	}

	/**
	Create a new output stream attached to the named file.
	Use the Chirp server implicitly defined by the environment.
	@param p The file name.
	@throws IOException
	*/

	public ChirpOutputStream( String p ) throws IOException {
		init(null,0,p,false);
	}


	/**
	Create a new output stream attached to the named file.
	Use the Chirp server implicitly defined by the environment.
	@param p The file name.
	@param append
		<ul>
		<li> If true - Open file for appending, fail if it does not exist.
		<li> If false - Create and truncate file for writing.
		</ul>
	@throws IOException
	*/

	public ChirpOutputStream( String p, boolean append ) throws IOException {
		init(null,0,p,append);
	}

	/**
	Create a new output stream attached to the named file.
	Use the named Chirp server host and port.
	@param host The server name.
	@param port The server port number.
	@param p The file name.
	@param append
		<ul>
		<li> If true - Open or create file for appending.
		<li> If false - Create and truncate file for writing.
		</ul>
	@throws IOException
	*/

	public ChirpOutputStream( String host, int port, String p, boolean append ) throws IOException {
		init(host,port,p,false);
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

	/**
	Forces all uncommitted data to stable storage.
	*/

	public void fsync( int fd ) throws IOException {
		client.fsync(fd);
	}
}

