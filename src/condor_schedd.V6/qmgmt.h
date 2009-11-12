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

#if !defined(_QMGMT_H)
#define _QMGMT_H

#include "condor_classad.h"
#include "condor_io.h"
#include "log_transaction.h" // for Transaction
#include "prio_rec.h"

#define NEW_BORN	1
#define DEATHS_DOOR	2


void PrintQ();
class Service;

class QmgmtPeer {
	
	friend QmgmtPeer* getQmgmtConnectionInfo();
	friend bool setQmgmtConnectionInfo(QmgmtPeer*);
	friend void unsetQmgmtConnection();

	public:
		QmgmtPeer();
		~QmgmtPeer();

		bool set(ReliSock *sock);
		bool set(const struct sockaddr_in *sock, const char *fqOwnerAndDomain);
		void unset();

		ReliSock *getReliSock() const { return sock; };

		const char *endpoint_ip_str() const;
		const struct sockaddr_in *endpoint() const;
		const char* getOwner() const;
		const char* getFullyQualifiedUser() const;
		int isAuthenticated() const;

	protected:

		char *owner;  
		char *fquser;  // owner@domain
		char *myendpoint; 
		struct sockaddr_in sockaddr;
		ReliSock *sock; 

		Transaction *transaction;
		int next_proc_num, active_cluster_num, old_cluster_num;
		time_t xact_start_time;

	private:
		// we do not allow deep-copies via copy ctor or assignment op,
		// so disable there here by making them private.
		QmgmtPeer(QmgmtPeer&);	
		QmgmtPeer & operator=(const QmgmtPeer & rhs);
};




void CloseJobHistoryFile();
void SetMaxHistoricalLogs(int max_historical_logs);
time_t GetOriginalJobQueueBirthdate();
void DestroyJobQueue( void );
int handle_q(Service *, int, Stream *sock);
void dirtyJobQueue( void );
bool isQueueSuperUser( const char* user );
bool OwnerCheck( ClassAd *ad, const char *test_owner );
bool OwnerCheck2( ClassAd *ad, const char *test_owner );
bool BuildPrioRecArray(bool no_match_found=false);
void DirtyPrioRecArray();
extern ClassAd *dollarDollarExpand(int cid, int pid, ClassAd *job, ClassAd *res, bool persist_expansions);
ClassAd* GetJobAd(int cluster_id, int proc_id, bool expStartdAd, bool persist_expansions);
ClassAd* GetNextJobByCluster( int, int );

int get_myproxy_password_handler(Service *, int, Stream *sock);

QmgmtPeer* getQmgmtConnectionInfo();
bool OwnerCheck(int,int);
bool OwnerCheck(ClassAd *, const char *);
bool OwnerCheck2(ClassAd *, const char *);

// priority records
extern prio_rec *PrioRec;
extern int N_PrioRecs;
extern HashTable<int,int> *PrioRecAutoClusterRejected;
extern int grow_prio_recs(int);

extern void	FindRunnableJob(PROC_ID & jobid, ClassAd* my_match_ad, 
					 char * user);
extern int Runnable(PROC_ID*);
extern int Runnable(ClassAd*);

extern class ForkWork schedd_forker;

#endif /* _QMGMT_H */
