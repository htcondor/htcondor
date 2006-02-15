/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

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

