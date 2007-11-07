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


// Import some other classes we'll use in this example.
// Once we import a class, we don't have to type its full name.
import java.math.BigInteger;  // Import BigInteger from java.math package
import java.util.*;  // Import all classes (including ArrayList) from java.util

public class x_perf_Busy {
	protected static ArrayList table = new ArrayList(); // create cache
	static { // Initialize the first element of the cache with !0 = 1.
	table.add(BigInteger.valueOf(1));
	}

	/** The factorial() method, using BigIntegers cached in a ArrayList */
	public static synchronized BigInteger factorial(int x) {
		if (x<0) throw new IllegalArgumentException("x must be non-negative.");
		for(int size = table.size(); size <= x; size++) {
			BigInteger lastfact = (BigInteger)table.get(size-1);
			BigInteger nextfact = lastfact.multiply(BigInteger.valueOf(size));
			table.add(nextfact);
		
		}
		return (BigInteger) table.get(x);
	}

	/**
	* A simple main() method that we can use as a standalone test program
	* for our factorial() method.
	**/
	public static Date  RightNow = new Date();
	public static Date  Later;
	public static long start = RightNow.getTime() / 1000;
	public static void main(String[] args) 
		throws Exception
	{
		long lapsed = 0;
		/*System.out.println("Time as a long " + start);*/
		while( lapsed <= Integer.parseInt(args[0]) )
		{
			/*System.out.println("Busy for this long " + args[0]);*/
			/* Thread.sleep(2 * 1000); */
			Later = new Date();
			lapsed = (Later.getTime()/1000) - start;
			//System.out.println("Lapsed " + lapsed);
			for(int i = 0; i <= 5000; i++) factorial(i);
			/*System.out.println(i + "! = " + factorial(i));
			*/
		}
	}
}

