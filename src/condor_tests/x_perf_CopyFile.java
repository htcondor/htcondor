import java.io.*;

public class x_perf_CopyFile
{
	public static int stdinwaiting;
	public static int readcount;
	public static byte[] readbuffer = new byte[4096];
	public static String source;
	public static String destination;
	public static String oneline;
	public static void main( String [] args )
	{
		// get file objects
		source = args[0];
		destination = args[1];
		File source_in = new File(source);
		File destination_out = new File(destination);

		// get stream objects
		FileInputStream in_stream = null;
		FileOutputStream out_stream = null;

		try 
		{
			in_stream = new FileInputStream(source_in);
			out_stream = new FileOutputStream(destination_out);
			while((readcount = in_stream.read(readbuffer)) != -1)
			{
				out_stream.write(readbuffer, 0, readcount);
			}
		}
		catch (IOException e) { ; }

		finally
		{
			if (in_stream != null) try { in_stream.close(); } catch (IOException e) { ; }
			if (out_stream != null) try { out_stream.close(); } catch (IOException e) { ; }
		}
	}
}

