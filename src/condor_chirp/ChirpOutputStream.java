
package edu.wisc.cs.condor.chirp;

import java.io.*;
import java.net.*;

public class ChirpOutputStream extends java.io.OutputStream {
	private ChirpClient client;
	private String path;
	private int fd;

	public ChirpOutputStream( String p ) throws IOException {
		client = new ChirpClient();
		path = p;
		fd = client.open(path,"wtc",0777);
	}

	public void write( byte [] buffer, int pos, int length ) throws IOException {
		while( length>0 ) {
			int actual = client.write(fd,buffer,pos,length);
			length -= actual;
			pos += actual;
		}
	}

	public void write( byte [] buffer ) throws IOException {
		write(buffer,0,buffer.length);
	}

	public void write( int b ) throws IOException {
		byte [] temp = new byte[1];
		temp[0] = (byte)b;
		write(temp,0,1);
	}

	public void close() throws IOException {
		client.close(fd);
	}

	public void flush() {
	}

}

