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

import java.io.*;

public class x_java_CopyFilePlus
{
	public static int stdinwaiting;
	public static int readcount;
	public static byte[] readbuffer = new byte[4096];
	public static String source = "job_core_priority_java.data";
	public static String destination = "job_core_priority_java.data.new";
	public static String oneline;
	public static void main( String [] args )
	{
		// get file objects
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
			out_stream.write(args[0].getBytes(), 0, args[0].length());
			oneline = "\n";
			out_stream.write(oneline.getBytes(), 0, oneline.length());
		}
		catch (IOException e) { ; }

		finally
		{
			if (in_stream != null) try { in_stream.close(); } catch (IOException e) { ; }
			if (out_stream != null) try { out_stream.close(); } catch (IOException e) { ; }
		}

		// remove old file
		source_in.delete();
		destination_out.renameTo(source_in);

	}
}

