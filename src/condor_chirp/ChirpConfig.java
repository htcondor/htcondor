
package edu.wisc.cs.condor.chirp;

import java.io.*;
import java.net.*;
import java.util.*;

public class ChirpConfig {

	private String host, cookie;
	private int port;

	public ChirpConfig( String filename ) throws IOException {
		String portstr;
		BufferedReader br = new BufferedReader(new FileReader(filename));
		StringTokenizer st = new StringTokenizer(br.readLine());

		host = st.nextToken();
		portstr = st.nextToken();
		cookie = st.nextToken();
		port = Integer.parseInt(portstr);
	}

	public String getHost() {
		return host;
	}

	public int getPort() {
		return port;
	}

	public String getCookie() {
		return cookie;
	}
}

