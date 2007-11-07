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


public class x_java_Property
{
	public static void main( String [] args )
	{
		int i = 0;
		System.out.println("Property Checking\n");
		//System.out.println("Args to print:" + args.length);
		while(i < args.length)
		{
			String prop = System.getProperty(args[i]);
			System.out.println(args[i] + " = " + prop);
			i++;
		}
		System.out.println();
	}
}
