/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

/*
This program queries the JVM for system properties and
displays them as a ClassAd.  The single argument
must be either "new" or "old" and controls whether the
output is in the format of new or old ClassAds.
*/

import java.lang.*;
import java.util.*;
import java.io.*;

public class CondorJavaInfo {

	public static void main( String[] args ) {
		boolean newmode=false;

		if( args.length!=1 ) {
			System.err.println("use: CondorJavaInfo <new|old>");
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

			System.out.print( UnDotString(name) + " = \"" + value + "\"");

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
