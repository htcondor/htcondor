
package edu.wisc.cs.condor.chirp;

import java.io.*;
import java.net.*;

public class ChirpWriter extends java.io.Writer {
	private ChirpClient client;
	private String path;
	private int fd;

	public ChirpWriter( ChirpClient c, String p ) {
		client = c;
		path = p;
		fd = -1;
	}

	public void write( char [] buffer, int pos, int length ) throws IOException {
		if(fd<0) {
			fd = client.open(path,"wtc",0777);
		}
		while( length>0 ) {
			int actual = client.write(fd,buffer,pos,length);
			length -= actual;
			pos += actual;
		}
	}

	public void close() throws IOException {
		client.close(fd);
	}

	public void flush() {
	}

}

