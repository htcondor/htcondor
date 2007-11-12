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

