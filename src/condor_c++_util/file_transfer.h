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

#ifndef _FILE_TRANSFER_H
#define _FILE_TRANSFER_H

#include "MyString.h"
#include "HashTable.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

class FileTransfer;	// forward declatation

typedef HashTable <MyString, FileTransfer *> TranskeyHashTable;

class FileTransfer {

  public:

	FileTransfer::FileTransfer();

	FileTransfer::~FileTransfer();

	int FileTransfer::Init(ClassAd *Ad);

	int FileTransfer::DownloadFiles();

	int FileTransfer::UploadFiles();

	static int FileTransfer::HandleCommands(Service *,int command,Stream *s);

  protected:

	int FileTransfer::Download(ReliSock *s);
	int FileTransfer::Upload(ReliSock *s, StringList *filelist);

  private:

	char* Iwd;
	StringList* InputFiles;
	StringList* OutputFiles;
	char* TransSock;
	char* TransKey;
		
	static TranskeyHashTable* TranskeyTable;

	static int CommandsRegistered;

};

#endif
