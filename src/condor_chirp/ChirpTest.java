
package edu.wisc.cs.condor.chirp;

import java.io.*;

public class ChirpTest {

	public static void main( String args[] ) {

		try {
			String line;

			BufferedReader in = new BufferedReader(new InputStreamReader(new ChirpInputStream(args[0])));
			PrintWriter out = new PrintWriter(new OutputStreamWriter(new ChirpOutputStream(args[1])));

			while(true) {
				line = in.readLine();
				if(line==null) break;
				out.println(line);
			}

			System.out.println("done!");

		} catch( IOException e ) {
			System.out.println(e);
		}
	}
}
