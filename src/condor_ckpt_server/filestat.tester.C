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
#include <fstream.h>
#include <stdlib.h>
#include <stdio.h>


const int MAX_FILES = 250;
const int MAX_OWNERS = 15;
const int MAX_HOSTS = 5;


extern "C" int random();


int main(void)
{
  ofstream outfile("testfilestat.dat");
  ifstream infile("/etc/hosts");
  char*    machine[MAX_HOSTS] = {
                                  "grumpy",
				  "wiley",
				  "picard",
				  "jackal",
				  "miss-grabople"
                                };
  char*    owner[MAX_OWNERS] = {
                                 "tsao",
				 "kirkd",
				 "tjung",
				 "jjason",
				 "livny",
				 "pruyne",
				 "litzkow",
				 "solomon",
				 "ramanath",
				 "ajitk",
				 "jordan",
				 "pimmel",
				 "goodman",
				 "yannis",
				 "parshore"
                               };
  int count;
  int r;

  if (infile.fail())
    {
      cerr << "ERROR: Cannot open /etc/hosts file" << endl;
      exit(-1);
    }
  if (outfile.fail())
    {
      cerr << "ERROR: Cannot open output file" << endl;
      exit(-1);
    }
  for (count=0; count<MAX_FILES; count++)
    {
      r = random()%MAX_HOSTS;
      outfile << machine[r] << ' ';
      r = random()%MAX_OWNERS;
      outfile << owner[r] << ' ';
      outfile << "testfile.";
      r = random()%3;
      outfile << r << endl;
    }
  infile.close();
}





