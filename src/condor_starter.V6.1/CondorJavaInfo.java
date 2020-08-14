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
second argument, now ignored, used to control an old
benchmarking option.
*/

import java.lang.*;
import java.util.*;
import java.io.*;

public class CondorJavaInfo {

	public static void main( String[] args ) {
		boolean newmode=false;

		if( args.length == 0 || args.length > 2 ) {
			System.err.println("use: CondorJavaInfo <new|old> [<benchmark-time>]");
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

		while( e.hasMoreElements() ) {
			name = (String) e.nextElement();
			value = p.getProperty(name);

			if( name.equals("line.separator") ) continue;

			// This is a bit evil, but because old classads don't
			// deal with escaped double quotes very well. This might be
			// revisited in the future.
			System.out.print( UnDotString(name) + " = \"" + 
				value.replace('"', '\'').replace('\n',' ').replace('\r', ' ') + "\"");

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
