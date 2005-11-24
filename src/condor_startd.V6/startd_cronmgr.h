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
#ifndef _STARTD_CRONMGR_H
#define _STARTD_CRONMGR_H

#include "condor_cronmgr.h"
#include "enum_utils.h"

// Define a simple class to run child tasks periodically.
class StartdCronMgr : public CronMgrBase
{
  public:
	StartdCronMgr( void );
	virtual ~StartdCronMgr( void );
	virtual int Initialize( void );
	virtual int Reconfig( void );
	int Shutdown( bool force );
	bool ShutdownOk( void );
	CronAutoPublish_t getAutoPublishValue() { return auto_publish; };

  protected:
	virtual CronJobBase *NewJob( const char *name );

  private:
	bool ShuttingDown;
	void JobEvent( CronJobBase *job, CondorCronEvent event );
	void ParamAutoPublish( void );
	CronAutoPublish_t auto_publish;
};

#endif /* _STARTD_CRONMGR_H */
