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


/*
XXX XXX XXX
WARNING WARNING WARNING
If you change this file, then you must compile it
and check the .class file into CVS.  Why?  Because
we can't manage a Java installation on all of our platforms,
but we still want compiled Java code distributed with
all platforms.
*/

/*
This program queries the JVM for system properties and
displays them as a ClassAd.  The first argument
must be either "new" or "old" and controls whether the
output is in the format of new or old ClassAds.  The
second argument controls how long to run the scimark2
benchmark, measured in seconds.
*/

import java.lang.*;
import java.util.*;
import java.io.*;
import jnt.scimark2.*;

public class CondorJavaInfo {

	public static void main( String[] args ) {
		boolean newmode=false;
		double btime;

		if( args.length!=2 ) {
			System.err.println("use: CondorJavaInfo <new|old> <benchmark-time>");
			System.exit(1);
		}

		if( args[0].equals("new") ) {
			newmode = true;
		} else if( args[0].equals("old") ) {
			newmode = false;
		} else {
			System.err.println("argument must be either 'new' or 'old'");
			System.exit(1);
		}

		Properties p = System.getProperties();
		Enumeration e = p.propertyNames();
		String name, value;

		if( newmode ) {
			System.out.println("[");
		}

		btime = Float.valueOf(args[1]).doubleValue()/5.0;
		if(btime>0) {
			double result = 0;
	                jnt.scimark2.Random r = new jnt.scimark2.Random(Constants.RANDOM_SEED);
			result += kernel.measureFFT(Constants.FFT_SIZE,btime,r);
			result += kernel.measureSOR(Constants.SOR_SIZE,btime,r);
			result += kernel.measureMonteCarlo(btime,r);
			result += kernel.measureSparseMatmult(Constants.SPARSE_SIZE_M,Constants.SPARSE_SIZE_nz,btime,r);
			result += kernel.measureLU( Constants.LU_SIZE, btime, r);
			result /= 5.0;
			System.out.print( "JavaMFlops = "+result);
			if( newmode ) {
				System.out.println(";");
			} else {
				System.out.print("\n");
			}
		}

		while( e.hasMoreElements() ) {
			name = (String) e.nextElement();
			value = p.getProperty(name);

			if( name.equals("line.separator") ) continue;

			// This is a bit evil, but because old classads don't
			// deal with escaped double quotes very well. This might be
			// revisited in the future.
			System.out.print( UnDotString(name) + " = \"" + 
				value.replace('"', '\'') + "\"");

			if( newmode ) {
				System.out.println(";");
			} else {
				System.out.print("\n");
			}
		}

		if( newmode ) {
			System.out.println("]");
		}

		System.out.flush();
	}

	/*
	Remove the dots from the input string, and replace
	the succeeding characters with uppercase versions.
	For example, java.version becomes JavaVersion
	*/

	static String UnDotString( String s ) {

		String result = new String("");
		boolean capNext = true;
		int i;

		for( i=0; i<s.length(); i++ ) {
			char c = s.charAt(i);
			if(c=='.') {
				capNext = true;
			} else {
				if(capNext) {
					c = Character.toUpperCase(c);
					capNext = false;
				}
				result = result + c;
			}
		}

		return result;
	}

}
