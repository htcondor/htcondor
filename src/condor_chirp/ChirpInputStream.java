
package edu.wisc.cs.condor.chirp;

import java.io.*;
import java.net.*;

public class ChirpInputStream extends java.io.InputStream {
	private ChirpClient client;
	private int fd;
	private String path;

	public ChirpInputStream( String p ) throws IOException {
		client = new ChirpClient();
		path = p;
		fd = client.open(path,"r",0);
	}

	public int read( byte [] buffer, int pos, int length ) throws IOException {
		int response;
		response = client.read(fd,buffer,pos,length);
		if(response==0) {
			return -1;
		} else {
			return response;
		}
	}

	public int read( byte [] buffer ) throws IOException {
		return read(buffer,0,buffer.length);
	}

	public int read() throws IOException {
		byte [] temp = new byte[1];
		int actual = read(temp,0,1);
		if( actual>0 ) {
			return temp[0];
		} else {
			return -1;
		}
	}

	public void close() throws IOException {
		client.close(fd);
	}

	public boolean ready() {
		return true;
	}
}


