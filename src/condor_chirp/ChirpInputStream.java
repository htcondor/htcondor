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
A ChirpInputStream gives a sequential binary interface to a read-only
file.  Users that require random-access I/O should see ChirpClient.
Users requiring a character-oriented interface to a Chirp file
should make use of an InputStreamReader like so:
<pre>
InputStreamReader in = new InputStreamReader(new ChirpInputStream(filename));
</pre> 
*/

public class ChirpInputStream extends java.io.InputStream {
	private ChirpClient client;
	private int fd;
	private String path;

	/**
	Create a new input stream attached to the named file.
	Use the Chirp server implicitly defined by the environment.
	@param p The file name.
	@throws IOException
	*/

	public ChirpInputStream( String p ) throws IOException {
		client = new ChirpClient();
		path = p;
		fd = client.open(path,"r",0);
	}

	/**
	Create a new input stream attached to the named file.
	Use the named Chirp server host and port.
	Condor users should use the single-argument constructor instead.
	@param host The server name.
	@param port The server port number.
	@param p The file name.
	@throws IOException
	*/

	public ChirpInputStream( String host, int port, String p ) throws IOException {
		client = new ChirpClient(host,port);
		path = p;
		fd = client.open(path,"r",0);
	}

	/**
	Read bytes from the stream.
	@param buffer The buffer to fill.
	@param pos The starting position in the buffer.
	@param length The number of bytes to read.
	@returns The number of bytes actually read, or -1 at end-of-file.
	@throws IOException
	*/

	public int read( byte [] buffer, int pos, int length ) throws IOException {
		int response;
		response = client.read(fd,buffer,pos,length);
		if(response==0) {
			return -1;
		} else {
			return response;
		}
	}

	/**
	Read bytes from the stream.
	@param buffer The buffer to fill.
	@returns The number of bytes actually read, or -1 at end-of-file.
	@throws IOException
	*/

	public int read( byte [] buffer ) throws IOException {
		return read(buffer,0,buffer.length);
	}

	/**
	Read one byte from the stream.
	@returns The byte read, or -1 at end-of-file.
	@throws IOException
	*/

	public int read() throws IOException {
		byte [] temp = new byte[1];
		int actual = read(temp,0,1);
		if( actual>0 ) {
		    /* byte is signed, so convert to unsigned */
		    return (int) temp[0] & 0xFF;
		} else {
			return -1;
		}
	}

	/**
	Complete access to this file.
	@throws IOException
	*/

	public void close() throws IOException {
		client.close(fd);
	}
}


