
package edu.wisc.cs.condor.chirp;
import java.io.*;

public class ChirpTest {

	public static void main( String args[] ) {

		try {
			String line;

			BufferedReader in = new BufferedReader(new ChirpReader(new ChirpClient(),"/etc/hosts"));
			PrintWriter out = new PrintWriter(new ChirpWriter(new ChirpClient(),"/tmp/outfile"));

			while(true) {
				line = in.readLine();
				if(line==null) break;
				out.println(line);
			}

			ChirpClient cc = new ChirpClient();

			cc.rename("/tmp/outfile","/tmp/outfile.renamed");
			cc.unlink("/tmp/outfile.renamed");

			System.out.println("done!");

		} catch( IOException e ) {
			System.out.println(e);
		}
	}
}
