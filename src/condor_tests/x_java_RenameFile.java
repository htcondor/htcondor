import java.io.*;
import java.util.*;

public class x_java_RenameFile
{
	public static int stdinwaiting;
	public static int readcount;
	public static byte[] readbuffer = new byte[4096];
	public static String source;
	public static String destination;
	public static String oneline;
	public static Calendar now = Calendar.getInstance();
	public static void main( String [] args )
	{
		// get file objects
		source = args[0];
		destination = args[1];
		File source_in = new File(source);
		File destination_out = new File(destination);

		//long oldtime = source_in.lastModified();
		long oldtime = System.currentTimeMillis();
		long newtime = oldtime + 5;
		boolean fRes = true;
		System.out.println(oldtime);
		System.out.println(newtime);
		source_in.renameTo(destination_out);
		fRes = destination_out.setLastModified(newtime);
		System.out.println(fRes);
	}
}

