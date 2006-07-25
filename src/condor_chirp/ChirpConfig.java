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
import java.util.*;

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
ChirpConfig represents the client configuration information needed
for a Chirp connection.  The constructor parses a configuration
file for a host, port, and cookie.  Inspector methods simply return
those values.
*/

class ChirpConfig {

	private String host, cookie;
	private int port;

	/**
	Load configuration data from a file.
	@param The name of the file.
	@throws IOException
	*/

	public ChirpConfig( String filename ) throws IOException {
		BufferedReader br = new BufferedReader(new FileReader(filename));
		StringTokenizer st = new StringTokenizer(br.readLine());

		host = st.nextToken();
		String portstr = st.nextToken();
		port = Integer.parseInt(portstr);
		cookie = st.nextToken();
	}

	/**
	@returns The name of the server host.
	*/

	public String getHost() {
		return host;
	}

	/**
	@returns The port on which the server is listening
	*/

	public int getPort() {
		return port;
	}

	/**
	@returns The cookie expected by the server.
	*/

	public String getCookie() {
		return cookie;
	}
}

