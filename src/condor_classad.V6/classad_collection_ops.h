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

#if !defined(_CLASSAD_COLLECTION_OPS)
#define _CLASSAD_COLLECTION_OPS

#include "exprTree.h"
#include "log.h"

#define CondorLogOp_CollNewClassAd          201
#define CondorLogOp_CollDestroyClassAd      202
#define CondorLogOp_CollUpdateClassAd       203
#define CondorLogOp_CollModifyClassAd		204
#define CondorLogOp_BeginTransaction        205
#define CondorLogOp_EndTransaction          206

//----------------------------------------------------------

class LogCollNewClassAd : public LogRecord {
public:
    LogCollNewClassAd(const char *key, ClassAd* ad);
    virtual ~LogCollNewClassAd();
    bool Play(void *data_structure);

	char* get_key() { return key; }
	ClassAd* get_ad() { return Ad; }

protected:
    virtual bool WriteBody(FILE*);
    virtual bool ReadBody(FILE*);

	bool 	Executed;
	char	*key;
	ClassAd	*Ad;
};

//----------------------------------------------------------

class LogCollUpdateClassAd : public LogCollNewClassAd {
public:
    LogCollUpdateClassAd(const char *key, ClassAd* ad);
    virtual ~LogCollUpdateClassAd();
    bool Play(void* data_structure);
};

//----------------------------------------------------------

class LogCollModifyClassAd : public LogCollNewClassAd {
public:
	LogCollModifyClassAd(const char *key, ClassAd *ad);
	virtual ~LogCollModifyClassAd();
	bool Play(void *data_structure);
};

//----------------------------------------------------------

class LogCollDestroyClassAd : public LogRecord {
public:
    LogCollDestroyClassAd(const char *key);
    virtual ~LogCollDestroyClassAd();
    bool Play(void *data_structure);

	char * get_key() { return( key ); }

protected:
    virtual bool WriteBody(FILE *fp);
    virtual bool ReadBody(FILE *);

private:
	char *key;
};

//----------------------------------------------------------

class LogBeginTransaction : public LogRecord {
public:
	LogBeginTransaction() { op_type = CondorLogOp_BeginTransaction; }
};

//----------------------------------------------------------

class LogEndTransaction : public LogRecord {
public:
	LogEndTransaction() { op_type = CondorLogOp_EndTransaction; }
};

#endif
