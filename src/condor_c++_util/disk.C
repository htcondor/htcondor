#include <stdio.h>
#include <iostream.h>
#include <fstream.h>
#include <string.h>

#include "debug.h"
#include "condor_updown.h"
#include "condor_disk.h"

extern	"C"		dprintf(int, char *, char *);

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
	int  fd;


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
	int  fd;

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
