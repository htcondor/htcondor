
package edu.wisc.cs.condor.chirp;

import java.io.*;
import java.net.*;

public class ChirpReader extends java.io.Reader {
	private ChirpClient client;
	private int fd;
	private String path;

	public ChirpReader( ChirpClient c, String p ) {
		client = c;
		path = p;
		fd = -1;
	}

	public int read( char [] buffer, int pos, int length ) throws IOException {
		int response;

		if(fd<0) {
			fd = client.open(path,"r",0);
		}

		response = client.read(fd,buffer,pos,length);
		if(response==0) {
			return -1;
		} else {
			return response;
		}
	}

	public void close() throws IOException {
		client.close(fd);
	}

	public boolean ready() {
		return true;
	}
}


