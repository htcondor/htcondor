/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
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

#ifndef __GRIDFTPMANAGER_H__
#define __GRIDFTPMANAGER_H__

class GridftpServer {
	friend class GridftpManager;
 public:
	const char * GetAddress();
	int GetState();
	

protected:
	GridftpServer();
	~GridftpServer();
	
	int StdoutReady();
	void SetState(int state);

	int m_retry_count;
	int m_state;

};


class GridftpManager {
 public:
	GridftpManager();
	~GridftpManager();

	GridftpServer * CreateNew();
	GrdiftpServer * Restart();
	GridftpServer * FindOrCreate();

	void ShutdownAll();

 protected:
	void Reaper(Service*,int pid,int status);
	void Shutdown (GridftpServer * gridftp);
	
	GridftpServer * m_gridftp;


};



#endif
