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

public class x_java_ReadStdin
{
	public static int stdinwaiting;
	public static String oneline;
	public static void main( String [] args )
	{
		BufferedReader in = new BufferedReader (new InputStreamReader(System.in));
		try
		{
			while( (oneline = in.readLine()) != null)
			{
				System.out.println( oneline );
			}
		}
		catch ( IOException e )
		{
			System.out.println("Oops.....");
		}
	}
}
