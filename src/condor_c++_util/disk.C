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

#include "condor_common.h"
#include "debug.h"
#include "condor_updown.h"
#include "condor_disk.h"

// constructor
File::File(const char* filename)
{
	fileName = new char[strlen(filename)+1];
	strcpy(fileName, filename);
}

// destructior
File::~File()
{
	delete []fileName;
}

// this method writes an UpDown object onto disk file
int File::operator << (const UpDown & upDown)
{
	ofstream fout(fileName);   // open the output file
	if ( fout.fail() )
	{
		dprintf(D_FULLDEBUG, "Cannot open file for writing: %s\n",fileName);
		return Error;
	}

	header.activeUsers 	= upDown.activeUsers;
	fout.write((char*) &header, sizeof header);
	if ( fout.fail() )
	{
		dprintf(D_FULLDEBUG, "Cannot write into file: %s\n", fileName);
		return Error;
	}
	
	data = new Data[header.activeUsers];
	for ( int i=0; i < upDown.activeUsers; i++)
	{
		strncpy(data[i].userName, upDown.table[i].name,
		   min(MaxUserNameSize-1,strlen(upDown.table[i].name)+1));
		data[i].priority = upDown.table[i].priority;
	}
	fout.write((char*) data, sizeof (Data)*header.activeUsers);
	delete []data;
	if ( fout.fail() )
	{
		dprintf(D_FULLDEBUG, "Cannot write into file: %s\n", fileName);
		return Error;
	}
	fout.close();
	return Success;
}
		
// reading in the file 
int File::operator >> (UpDown & upDown)
{
	ifstream fin(fileName);   // open the input file
	if ( fin.fail() )
	{
		dprintf(D_FULLDEBUG, "Cannot open file for reading: %s\n",fileName);
		return Error;
	}

	fin.read((char*) &header, sizeof header);
	if ( fin.fail() )
	{
		dprintf(D_FULLDEBUG, "Cannot read from file: %s\n", fileName);
		return Error;
	}

	if ( header.activeUsers > upDown.maxUsers )
		upDown.AllocateSpaceMoreUsers(header.activeUsers);
	
	data = new Data[header.activeUsers];
	fin.read((char*) data, sizeof(Data)*header.activeUsers);
	if ( fin.fail() )
	{
		dprintf(D_FULLDEBUG, "Cannot read from file: %s\n", fileName);
		delete data;
		return Error;
	}
	// copying from temp data structure to upDown object
	for ( int i=0; i < header.activeUsers; i++)
	{
		// check if some name is already there
		if ( i<upDown.activeUsers && upDown.table[i].name)
				delete upDown.table[i].name;
			
		upDown.table[i].name= new char[strlen(data[i].userName)+1];
		
		strcpy(upDown.table[i].name, data[i].userName);
		upDown.table[i].priority = data[i].priority;
	}
	upDown.activeUsers 	= header.activeUsers;
	fin.close();	
	delete []data;
	return Success;
}
