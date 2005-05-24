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
#include "condor_classad.h"
#include "../condor_daemon_core.V6/condor_daemon_core.h"

// Things to include for the stubs
#include "condor_version.h"
#include "condor_attributes.h"
#include "scheduler.h"
#include "condor_qmgr.h"
#include "CondorError.h"
#include "MyString.h"
#include "internet.h"

#include "condor_ckpt_name.h"
#include "condor_config.h"

#include "loose_file_transfer.h"

#include "soap_scheddStub.h"
#include "condorSchedd.nsmap"

#include "schedd_api.h"

#include "../condor_c++_util/soap_helpers.cpp"

#include "qmgmt.h"

// XXX: There should be checks to see if the cluster/job Ids are valid
// in the transaction they are being used...but only when we have
// multiple transactions.
static int current_trans_id = 0;
static int trans_timer_id = -1;

extern Scheduler scheduler;

/* XXX: When we finally have multiple transactions it is important
   that each one has a "jobs" hashtable! */
template class HashTable<MyString, Job *>;
HashTable<MyString, Job *> jobs =
	HashTable<MyString, Job *>(1024, MyStringHash, rejectDuplicateKeys);

static bool
convert_FileInfoList_to_Array(struct soap * soap,
                              List<FileInfo> & list,
                              struct condor__FileInfoArray & array)
{
	array.__size = list.Number();
	if (0 == array.__size) {
		array.__ptr = NULL;
	} else {
		array.__ptr =
			(struct condor__FileInfo *)
			soap_malloc(soap, array.__size * sizeof(struct condor__FileInfo));
		ASSERT(array.__ptr);

		FileInfo *info;
		list.Rewind();
		for (int i = 0; list.Next(info); i++) {
			array.__ptr[i].name =
				(char *) soap_malloc(soap, strlen(info->name) + 1);
			ASSERT(array.__ptr[i].name);
			strcpy(array.__ptr[i].name, info->name);
			array.__ptr[i].size = info->size;
		}
	}

	return true;
}

static
bool
null_transaction(const struct condor__Transaction & transaction)
{
	return !transaction.id;
}

static
bool
valid_transaction(const struct condor__Transaction & transaction)
{
	return current_trans_id == transaction.id;
}

static
int
getJob(int clusterId, int jobId, Job *&job)
{
	MyString key;
	key += clusterId;
	key += ".";
	key += jobId;

	return jobs.lookup(key, job);
}

static
int
insertJob(int clusterId, int jobId, Job *job)
{
		// Having the key on the stack is safe because MyString does a
		// copy on assignment.
	MyString key;
	key += clusterId;
	key += ".";
	key += jobId;

	return jobs.insert(key, job);
}

static
int
removeJob(int clusterId, int jobId)
{
	MyString key;
	key += clusterId;
	key += ".";
	key += jobId;

	return jobs.remove(key);
}

static
int
removeCluster(int clusterId)
{
	MyString currentKey;
	Job *job;
	jobs.startIterations();
	while (jobs.iterate(currentKey, job)) {
		if (job->getClusterID() == clusterId) {
			jobs.remove(currentKey);
		}
	}

	return 0;
}

static
int
extendTransaction(const struct condor__Transaction & transaction)
{
	if (!null_transaction(transaction) &&
		transaction.id == current_trans_id &&
		trans_timer_id != -1) {

		if (transaction.duration < 1) {
			return 1;
		}

		daemonCore->Reset_Timer(trans_timer_id, transaction.duration);
		daemonCore->Only_Allow_Soap(transaction.duration);
	}

	return 0;
}

// TODO : Todd needs to redo all the transaction stuff and get it
// right.  For now it is in horrible "demo" mode with piles of
// assumptions (i.e. only one client, etc).  Once it is redone and
// decent, all the logic should move OUT of the stubs and into the
// schedd proper... since it should all work the same from the cedar
// side as well.
int
condor__transtimeout()
{
	struct condor__abortTransactionResponse result;

	dprintf(D_FULLDEBUG, "SOAP in condor__transtimeout()\n");

	condor__Transaction transaction;
	transaction.id = current_trans_id;
	condor__abortTransaction(NULL, transaction, result);
	return TRUE;
}

int
condor__beginTransaction(struct soap *soap,
						 int duration,
						 struct condor__beginTransactionResponse & result)
{
	static int trans_offset = 0;

	if ( current_trans_id ) {
			// if there is an existing transaction, abort it.
			// TODO - support more than one active transaction!!!
		struct condor__abortTransactionResponse response;
		struct condor__Transaction transaction;
		transaction.id = current_trans_id;
		condor__abortTransaction(soap, transaction, response);
	}
	if ( duration < 1 ) {
		duration = 1;
	}

	trans_timer_id =
		daemonCore->Register_Timer(duration,
								   (TimerHandler)&condor__transtimeout,
								   "condor_transtimeout");
	daemonCore->Only_Allow_Soap(duration);
		// This ID should allow for 256 transactions per second and
		// only repeat every 2^24 seconds (~= 1 year).
	current_trans_id = time(NULL);
	current_trans_id = (current_trans_id << 8) + trans_offset;
	trans_offset = (trans_offset + 1) % 256;
	result.response.transaction.id = current_trans_id;
	result.response.transaction.duration = duration;
	result.response.status.code = SUCCESS;
	result.response.status.message = "Check out that sweet new transaction.";

	// Tell the qmgmt layer to allow anything -- that is, until we
	// authenticate the client.
	setQSock(NULL);

	BeginTransaction();

	dprintf(D_FULLDEBUG,
			"SOAP leaving condor__beginTransaction() id=%ld\n",
			result.response.transaction.id);

	return SOAP_OK;
}

int
condor__commitTransaction(struct soap *s,
						  struct condor__Transaction transaction,
						  struct condor__commitTransactionResponse & result)
{
	if (!valid_transaction(transaction) ||
		null_transaction(transaction)) {
		result.response.code = INVALIDTRANSACTION;
		result.response.message = "Invalid transaction.";
	} else {
		CommitTransaction();
		current_trans_id = 0;
		transaction.id = 0;
		if ( trans_timer_id != -1 ) {
			daemonCore->Cancel_Timer(trans_timer_id);
			trans_timer_id = -1;
		}
		daemonCore->Only_Allow_Soap(0);

		result.response.code = SUCCESS;
		result.response.message = "As you wish.";
	}

		//jobs.clear(); // XXX: Do the destructors get called?

	dprintf(D_FULLDEBUG,
			"SOAP leaving condor__commitTransaction() res=%d\n",
			result.response.code);
	return SOAP_OK;
}


int
condor__abortTransaction(struct soap *s,
						 struct condor__Transaction transaction,
						 struct condor__abortTransactionResponse & result )
{
	if (!valid_transaction(transaction) ||
		null_transaction(transaction)) {
		result.response.code = INVALIDTRANSACTION;
		result.response.message = "Invalid transaction.";
	} else {
		if (transaction.id && transaction.id == current_trans_id) {
			AbortTransactionAndRecomputeClusters();
			dprintf(D_FULLDEBUG,
					"SOAP cleared file hashtable for transaction: %d\n",
					transaction.id);

				// Let's forget about all the file associated with the
				//transaction.  jobs.clear();

			current_trans_id = 0;
			transaction.id = 0;
			if ( trans_timer_id != -1 ) {
				daemonCore->Cancel_Timer(trans_timer_id);
				trans_timer_id = -1;
			}
			daemonCore->Only_Allow_Soap(0);
		}

		result.response.code = SUCCESS;
		result.response.message = "Don't cry for it.";
	}

	dprintf(D_FULLDEBUG,
			"SOAP leaving condor__abortTransaction() res=%d\n",
			result.response.code);
	return SOAP_OK;
}


int
condor__extendTransaction(struct soap *s,
						  struct condor__Transaction transaction,
						  int duration,
						  struct condor__extendTransactionResponse & result )
{
	if (!valid_transaction(transaction) ||
		null_transaction(transaction)) {
		result.response.status.code = INVALIDTRANSACTION;
		result.response.status.message = "Invalid transaction.";
	} else {
		transaction.duration = duration;
		if (extendTransaction(transaction)) {
			result.response.status.code = FAIL;
			result.response.status.message = "Could not extend transaction.";
		} else {
			result.response.transaction.id = transaction.id;
			result.response.transaction.duration = duration;

			result.response.status.code = SUCCESS;
			result.response.status.message =
				"If it were only this easy to extend one's life.";
		}
	}

	dprintf(D_FULLDEBUG,
			"SOAP leaving condor__extendTransaction() res=%d\n",
			result.response.status.code);
	return SOAP_OK;
}


int
condor__newCluster(struct soap *s,
				   struct condor__Transaction transaction,
				   struct condor__newClusterResponse & result)
{
	if (!valid_transaction(transaction) ||
		null_transaction(transaction)) {
		result.response.status.code = INVALIDTRANSACTION;
		result.response.status.message = "Invalid transaction.";
	} else {
		extendTransaction(transaction);

		result.response.integer = NewCluster();
		if (result.response.integer == -1) {
			result.response.status.code = FAIL;
			result.response.status.message = "Could not create new cluster.";
		} else {
			result.response.status.code = SUCCESS;
			result.response.status.message =
				"It's new, it's hip, it's unimproved.";
		}
	}

	dprintf(D_FULLDEBUG,
			"SOAP leaving condor__newCluster() res=%d\n",
			result.response.status.code);
	return SOAP_OK;
}


int
condor__removeCluster(struct soap *s,
					  struct condor__Transaction transaction,
					  int clusterId,
					  char* reason,
					  struct condor__removeClusterResponse & result)
{
    if ( !valid_transaction(transaction) &&
         !null_transaction(transaction) ) {
        result.response.code = INVALIDTRANSACTION;
		result.response.message = "Invalid transaction.";
    } else {
        extendTransaction(transaction);

        MyString constraint;
        constraint.sprintf("%s==%d", ATTR_CLUSTER_ID, clusterId);
        if ( abortJobsByConstraint(constraint.GetCStr(),
								   reason,
								   transaction.id ? false : true) ) {
            if ( removeCluster(clusterId) ) {
                result.response.code = FAIL;
				result.response.message = "Failed to remove cluster.";
            } else {
                result.response.code = SUCCESS;
				result.response.message = "It worked, you removed it!";
            }
        } else {
            result.response.code = FAIL;
			result.response.message = "Failed to abort jobs in the cluster.";
        }
    }

    dprintf(D_FULLDEBUG,
			"SOAP leaving condor__removeCluster() res=%d\n",
			result.response.code);
    return SOAP_OK;
}


int
condor__newJob(struct soap *soap,
			   struct condor__Transaction transaction,
			   int clusterId,
			   struct condor__newJobResponse & result)
{
	if (!valid_transaction(transaction) ||
		null_transaction(transaction)) {
		result.response.status.code = INVALIDTRANSACTION;
		result.response.status.message = "Invalid transaction.";
	} else {
		extendTransaction(transaction);

		result.response.integer = NewProc(clusterId);
		if (result.response.integer == -1) {
			result.response.status.code = FAIL;
			result.response.status.message = "Could not create new job.";
		} else {
				// Create a Job for this new job.
			Job *job = new Job(clusterId, result.response.integer);
			ASSERT(job);
			CondorError errstack;
			if (job->initialize(errstack)) {
				result.response.integer = -1;
				result.response.status.code =
					(condor__StatusCode) errstack.code();
				result.response.status.message =
					(char *) soap_malloc(soap, strlen(errstack.message()) + 1);
				strcpy(result.response.status.message,
					   errstack.message());
			} else {
				if (insertJob(clusterId, result.response.integer, job)) {
					result.response.status.code = FAIL;
					result.response.status.message =
						"Could not record new job.";
				} else {
					result.response.status.code = SUCCESS;
					result.response.status.message = "Do your worst.";
				}
			}
		}
	}

	dprintf(D_FULLDEBUG,
			"SOAP leaving condor__newJob() res=%d\n",
			result.response.status.code);
	return SOAP_OK;
}


int
condor__removeJob(struct soap *s,
				  struct condor__Transaction transaction,
				  int clusterId,
				  int jobId,
				  char* reason,
				  bool force_removal,
				  struct condor__removeJobResponse & result)
{
		// TODO --- do something w/ force_removal flag; it is ignored for now.

	if (!valid_transaction(transaction) &&
		!null_transaction(transaction)) {
		result.response.code = INVALIDTRANSACTION;
		result.response.message = "Invalid transaction.";
	} else {
		extendTransaction(transaction);

		if (!abortJob(clusterId,jobId,reason,transaction.id ? false : true)) {
			result.response.code = FAIL;
			result.response.message = "Failed to abort job.";
		} else {
			if (removeJob(clusterId, jobId)) {
				result.response.code = FAIL;
				result.response.message = "Failed to remove job.";
			} else {
				result.response.code = SUCCESS;
				result.response.message = "Poor thing, it just wanted to run.";
			}
		}
	}

	dprintf(D_FULLDEBUG,
			"SOAP leaving condor__removeJob() res=%d\n",
			result.response.code);
	return SOAP_OK;
}


int
condor__holdJob(struct soap *s,
				struct condor__Transaction transaction,
				int clusterId,
				int jobId,
				char* reason,
				bool email_user,
				bool email_admin,
				bool system_hold,
				struct condor__holdJobResponse & result)
{
	if (!valid_transaction(transaction) &&
		!null_transaction(transaction)) {
		result.response.code = INVALIDTRANSACTION;
		result.response.message = "Invalid transaction.";
	} else {
		extendTransaction(transaction);

		if (!holdJob(clusterId,jobId,reason,transaction.id ? false : true,
					 email_user, email_admin, system_hold)) {
			result.response.code = FAIL;
			result.response.message = "Failed to hold job.";
		} else {
			result.response.code = SUCCESS;
			result.response.message =
				"Release soon, before the job runs out of air.";
		}
	}

	dprintf(D_FULLDEBUG,
			"SOAP leaving condor__holdJob() res=%d\n",
			result.response.code);
	return SOAP_OK;
}


int
condor__releaseJob(struct soap *s,
				   struct condor__Transaction transaction,
				   int clusterId,
				   int jobId,
				   char* reason,
				   bool email_user,
				   bool email_admin,
				   struct condor__releaseJobResponse & result)
{
	if (!valid_transaction(transaction) &&
		!null_transaction(transaction)) {
		result.response.code = INVALIDTRANSACTION;
		result.response.message = "Invalid transaction.";
	} else {
		extendTransaction(transaction);

		if (!releaseJob(clusterId,jobId,reason,transaction.id ? false : true,
						email_user, email_admin)) {
			result.response.code = FAIL;
			result.response.message = "Failed to release job.";
		} else {
			result.response.code = SUCCESS;
			result.response.message = "Free, free as the wind!";
		}
	}

	dprintf(D_FULLDEBUG,
			"SOAP leaving condor__releaseJob() res=%d\n",
			result.response.code);
	return SOAP_OK;
}


int
condor__submit(struct soap *soap,
			   struct condor__Transaction transaction,
			   int clusterId,
			   int jobId,
			   struct condor__ClassAdStruct * jobAd,
			   struct condor__submitResponse & result)
{
	if (!valid_transaction(transaction) ||
		null_transaction(transaction)) {
		result.response.status.code = INVALIDTRANSACTION;
		result.response.status.message = "Invalid transaction.";
	} else {
		extendTransaction(transaction);

		Job *job;
		if (getJob(clusterId, jobId, job)) {
			result.response.status.code = UNKNOWNJOB;
			result.response.status.message = "Unknown job.";
		} else {
			ClassAd realJobAd;
			if (!convert_adStruct_to_ad(soap, &realJobAd, jobAd)) {
				result.response.status.code = FAIL;
				result.response.status.message = "Failed to parse job ad.";
			} else {
				CondorError errstack;
				if (job->submit(*jobAd, errstack)) {
					result.response.status.code =
						(condor__StatusCode) errstack.code();
					result.response.status.message =
						(char *)
						soap_malloc(soap, strlen(errstack.message()) + 1);
					strcpy(result.response.status.message,
						   errstack.message());
				} else {
					result.response.status.code = SUCCESS;
					result.response.status.message = "It's off to the races.";
				}
			}
		}
	}

	dprintf(D_FULLDEBUG,
			"SOAP leaving condor__submit() res=%d\n",
			result.response.status.code);
	return SOAP_OK;
}


int
condor__getJobAds(struct soap *soap,
				  struct condor__Transaction transaction,
				  char *constraint,
				  struct condor__getJobAdsResponse & result )
{
	dprintf(D_FULLDEBUG,"SOAP entering condor__getJobAds() \n");

	if (!valid_transaction(transaction) &&
		!null_transaction(transaction)) {
		result.response.status.code = INVALIDTRANSACTION;
		result.response.status.message = "Invalid transaction.";
	} else {
		extendTransaction(transaction);

		List<ClassAd> adList;
		ClassAd *ad = GetNextJobByConstraint(constraint, 1);
		while (ad) {
			adList.Append(ad);
			ad = GetNextJobByConstraint(constraint, 0);
		}

			// fill in our soap struct response
		if (!convert_adlist_to_adStructArray(soap,
											 &adList,
											 &result.response.classAdArray) ) {
			dprintf(D_FULLDEBUG,
					"condor__getJobAds: adlist to adStructArray failed!\n");

			result.response.status.code = FAIL;
			result.response.status.message = "Failed to serialize job ads.";
		} else {
			result.response.status.code = SUCCESS;
			result.response.status.message = "Careful, they bite.";
		}
	}

	return SOAP_OK;
}


int
condor__getJobAd(struct soap *soap,
				 struct condor__Transaction transaction,
				 int clusterId,
				 int jobId,
				 struct condor__getJobAdResponse & result )
{
		// TODO : deal with transaction consistency; currently, job ad is
		// invisible until a commit.  not very ACID compliant, is it? :(

	dprintf(D_FULLDEBUG,"SOAP entering condor__getJobAd() \n");

	if (!valid_transaction(transaction) &&
		!null_transaction(transaction)) {
		result.response.status.code = INVALIDTRANSACTION;
		result.response.status.message = "Invalid transaction.";
	} else {
		extendTransaction(transaction);

		ClassAd *ad = GetJobAd(clusterId,jobId);
		if (!ad) {
			result.response.status.code = UNKNOWNJOB;
			result.response.status.message = "Specified job is unknown.";
		} else {
			if (!convert_ad_to_adStruct(soap,
										ad,
										&result.response.classAd,
										false)) {
				dprintf(D_FULLDEBUG,
						"condor__getJobAd: adlist to adStructArray failed!\n");

				result.response.status.code = FAIL;
				result.response.status.message = "Failed to serialize job ad.";
			} else {
				result.response.status.code = SUCCESS;
				result.response.status.message = "Remember to love it.";
			}
		}
	}

	return SOAP_OK;
}

int
condor__declareFile(struct soap *soap,
					struct condor__Transaction transaction,
					int clusterId,
					int jobId,
					char * name,
					int size,
					enum condor__HashType hashType,
					char * hash,
					struct condor__declareFileResponse & result)
{
	dprintf(D_FULLDEBUG, "SOAP entering condor__declareFile() \n");

	if (!valid_transaction(transaction) ||
		null_transaction(transaction)) {
		result.response.code = INVALIDTRANSACTION;
		result.response.message = "Invalid transaction.";
	} else {
		extendTransaction(transaction);

		Job *job;
		if (getJob(clusterId, jobId, job)) {
			result.response.code = UNKNOWNJOB;
			result.response.message = "Unknown job.";
		} else {
			CondorError errstack;
			if (job->declare_file(MyString(name), size, errstack)) {
				result.response.code =
					(condor__StatusCode) errstack.code();
				result.response.message =
					(char *) soap_malloc(soap, strlen(errstack.message()) + 1);
				strcpy(result.response.message,
					   errstack.message());
			} else {
				result.response.code = SUCCESS;
				result.response.message = "What a declaration!";
			}
		}
	}

	return SOAP_OK;
}

int
condor__sendFile(struct soap *soap,
				 struct condor__Transaction transaction,
				 int clusterId,
				 int jobId,
				 char * filename,
				 int offset,
				 struct xsd__base64Binary *data,
				 struct condor__sendFileResponse & result)
{
	dprintf(D_FULLDEBUG, "SOAP entering condor__sendFile() \n");

	if (!valid_transaction(transaction) ||
		null_transaction(transaction)) {
		result.response.code = INVALIDTRANSACTION;
		result.response.message = "Invalid transaction.";
	} else {
		extendTransaction(transaction);

		Job *job;
		if (getJob(clusterId, jobId, job)) {
			result.response.code = INVALIDTRANSACTION;
			result.response.message = "Invalid transaction.";
		} else {
			CondorError errstack;
			if (0 == job->put_file(MyString(filename),
								   offset,
								   (char *) data->__ptr,
								   data->__size,
								   errstack)) {
				result.response.code = SUCCESS;
				result.response.message = "That's quite a fastball.";
			} else {
				result.response.code =
					(condor__StatusCode) errstack.code();
				result.response.message =
					(char *) soap_malloc(soap, strlen(errstack.message()) + 1);
				strcpy(result.response.message,
					   errstack.message());
			}
		}
	}

	return SOAP_OK;
}

int condor__getFile(struct soap *soap,
					struct condor__Transaction transaction,
					int clusterId,
					int jobId,
					char * name,
					int offset,
					int length,
					struct condor__getFileResponse & result)
{
	dprintf(D_FULLDEBUG, "SOAP entering condor__getFile() \n");
	bool destroy_job = false;

	if (!valid_transaction(transaction) &&
		!null_transaction(transaction)) {
		result.response.status.code = INVALIDTRANSACTION;
		result.response.status.message = "Invalid transaction.";
	} else {
		extendTransaction(transaction);

		Job *job;
		if (getJob(clusterId, jobId, job)) {
				// Very similar code is in condor__listSpool()
			job = new Job(clusterId, jobId);
			ASSERT(job);
			CondorError errstack;
			if (job->initialize(errstack)) {
				result.response.status.code =
					(condor__StatusCode) errstack.code();
				result.response.status.message =
					(char *) soap_malloc(soap, strlen(errstack.message()) + 1);
				strcpy(result.response.status.message,
					   errstack.message());

				if (job) {
					delete job;
					job = NULL;
				}

				return SOAP_OK;
			}

			destroy_job = true;
		}

		if (0 >= length) {
			result.response.status.code = FAIL;
			result.response.status.message = "LENGTH must be >= 0";
			dprintf(D_FULLDEBUG, "length is <= 0: %d\n", length);
		} else {
			unsigned char * data =
				(unsigned char *) soap_malloc(soap, length);
			ASSERT(data);

			int status;
			CondorError errstack;
			if (0 == (status = job->get_file(MyString(name),
											 offset,
											 length,
											 data,
											 errstack))) {
				result.response.status.code = SUCCESS;
				result.response.status.message =
					"I hope you can handle the truth.";

				result.response.data.__ptr = data;
				result.response.data.__size = length;
			} else {
				result.response.status.code =
					(condor__StatusCode) errstack.code();
				result.response.status.message =
					(char *)
					soap_malloc(soap, strlen(errstack.message()) + 1);
				strcpy(result.response.status.message,
					   errstack.message());

				dprintf(D_FULLDEBUG, "get_file failed: %d\n", status);
			}
		}

		if (destroy_job && job) {
			delete job;
			job = NULL;
		}
	}

	return SOAP_OK;
}

int condor__closeSpool(struct soap *soap,
					   struct condor__Transaction transaction,
					   xsd__int clusterId,
					   xsd__int jobId,
					   struct condor__closeSpoolResponse & result)
{
	dprintf(D_FULLDEBUG, "SOAP entering condor__closeSpool() \n");

	if (!valid_transaction(transaction) &&
		!null_transaction(transaction)) {
		result.response.code = INVALIDTRANSACTION;
		result.response.message = "Invalid transaction.";
	} else {
		extendTransaction(transaction);

		if (SetAttribute(clusterId, jobId, "FilesRetrieved", "TRUE")) {
			result.response.code = FAIL;
			result.response.message = "Failed to set FilesRetrieved attribute.";
		} else {
			result.response.code = SUCCESS;
			result.response.message = "I'm sure it was a nice existence.";
		}
	}

	return SOAP_OK;
}

int
condor__listSpool(struct soap * soap,
				  struct condor__Transaction transaction,
				  int clusterId,
				  int jobId,
				  struct condor__listSpoolResponse & result)
{
	dprintf(D_FULLDEBUG, "SOAP entering condor__listSpool() \n");

	bool destroy_job = false;

	if (!valid_transaction(transaction) &&
		!null_transaction(transaction)) {
		result.response.status.code = INVALIDTRANSACTION;
		result.response.status.message = "Invalid transaction.";
	} else {
		extendTransaction(transaction);

		Job *job;
		if (getJob(clusterId, jobId, job)) {
			job = new Job(clusterId, jobId);
			ASSERT(job);
			CondorError errstack;
			if (job->initialize(errstack)) {
				result.response.status.code =
					(condor__StatusCode) errstack.code();
				result.response.status.message =
					(char *) soap_malloc(soap, strlen(errstack.message()) + 1);
				strcpy(result.response.status.message,
					   errstack.message());

				if (job) {
					delete job;
					job = NULL;
				}

				return SOAP_OK;
			}

			destroy_job = true;
		}

		List<FileInfo> files;
		int code;
		CondorError errstack;
		if (code = job->get_spool_list(files, errstack)) {
			result.response.status.code =
				(condor__StatusCode) errstack.code();
			result.response.status.message =
				(char *) soap_malloc(soap, strlen(errstack.message()) + 1);
			strcpy(result.response.status.message,
				   errstack.message());
			dprintf(D_FULLDEBUG,
					"listSpool: get_spool_list FAILED -- %d\n",
					code);
		} else {
			if (convert_FileInfoList_to_Array(soap,
											  files,
											  result.response.info)) {
				result.response.status.code = SUCCESS;
				result.response.status.message = "When you wish upon a star...";
				dprintf(D_FULLDEBUG, "listSpool: SUCCESS\n");
			} else {
				result.response.status.code = FAIL;
				result.response.status.message = "Failed to serialize list.";
				dprintf(D_FULLDEBUG,
						"listSpool: FileInfoList to Array FAILED\n");
			}
		}

			// Clean up the files.
		FileInfo *info;
		files.Rewind();
		while (files.Next(info)) {
			if (info) {
				delete info;
				info = NULL;
			}
		}

		if (destroy_job && job) {
			delete job;
			job = NULL;
		}
	}

	return SOAP_OK;
}

int
condor__requestReschedule(struct soap *soap,
						  void *,
						  struct condor__requestRescheduleResponse & result)
{
	if (Reschedule()) {
		result.response.code = SUCCESS;
		result.response.message = "Time for some chocolate.";
	} else {
		result.response.code = FAIL;
		result.response.message = "Failed to request reschedule.";
	}

	return SOAP_OK;
}

int
condor__discoverJobRequirements(struct soap *soap,
								struct condor__ClassAdStruct * jobAd,
								struct condor__discoverJobRequirementsResponse & result)
{
	LooseFileTransfer fileTransfer;

	ClassAd ad;
	StringList inputFiles;
	char *buffer;

	convert_adStruct_to_ad(soap, &ad, jobAd);

	fileTransfer.SimpleInit(&ad, false, false);

	fileTransfer.getInputFiles(inputFiles);

	result.response.requirements.__size = inputFiles.number();
	result.response.requirements.__ptr =
		(condor__Requirement *)
		soap_malloc(soap,
					result.response.requirements.__size *
					   sizeof(condor__Requirement));
	ASSERT(result.response.requirements.__ptr);

	inputFiles.rewind();
	int i = 0;
	while ((buffer = inputFiles.next()) &&
		   (i < result.response.requirements.__size)) {
		result.response.requirements.__ptr[i] =
			(char *) soap_malloc(soap, strlen(buffer) + 1);
		ASSERT(result.response.requirements.__ptr[i]);
		strcpy(result.response.requirements.__ptr[i], buffer);
		i++;
	}

	result.response.status.code = SUCCESS;
	result.response.status.message = "What a boring discovery.";

	return SOAP_OK;
}

int
condor__createJobTemplate(struct soap *soap,
						  int clusterId,
						  int jobId,
						  char * owner,
						  condor__UniverseType universe,
						  char * cmd,
						  char * args,
						  char * requirements,
						  struct condor__createJobTemplateResponse & result)
{
	MyString attribute;

	ClassAd job;

	job.SetMyTypeName(JOB_ADTYPE);
	job.SetTargetTypeName(STARTD_ADTYPE);

	attribute = MyString(ATTR_VERSION) + " = \"" + CondorVersion() + "\"";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_PLATFORM) + " = \"" + CondorPlatform() + "\"";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_CLUSTER_ID) + " = " + clusterId;
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_PROC_ID) + " = " + jobId;
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_Q_DATE) + " = " + ((int) time((time_t *) 0));
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_COMPLETION_DATE) + " = 0";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_OWNER) + " = \"" + owner + "\"";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_JOB_REMOTE_WALL_CLOCK) + " = 0.0";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_JOB_LOCAL_USER_CPU) + " = 0.0";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_JOB_LOCAL_SYS_CPU) + " = 0.0";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_JOB_REMOTE_USER_CPU) + " = 0.0";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_JOB_REMOTE_SYS_CPU) + " = 0.0";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_JOB_EXIT_STATUS) + " = 0";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_NUM_CKPTS) + " = 0";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_NUM_RESTARTS) + " = 0";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_NUM_SYSTEM_HOLDS) + " = 0";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_JOB_COMMITTED_TIME) + " = 0";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_TOTAL_SUSPENSIONS) + " = 0";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_LAST_SUSPENSION_TIME) + " = 0";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_CUMULATIVE_SUSPENSION_TIME) + " = 0";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_ON_EXIT_BY_SIGNAL) + " = FALSE";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_JOB_ROOT_DIR) + " = \"/\"";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_JOB_UNIVERSE) + " = " + universe;
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_JOB_CMD) + " = \"" + cmd + "\"";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_MIN_HOSTS) + " = 1";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_MAX_HOSTS) + " = 1";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_CURRENT_HOSTS) + " = 0";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_WANT_REMOTE_SYSCALLS) + " = FALSE";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_WANT_CHECKPOINT) + " = FALSE";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_JOB_STATUS) + " = 1";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_ENTERED_CURRENT_STATE) + " = " +
		(((int) time((time_t *) 0)) / 1000);
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_JOB_PRIO) + " = 0";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_JOB_ENVIRONMENT) + " = \"\"";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_JOB_NOTIFICATION) + " = 2";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_KILL_SIG) + " = \"SIGTERM\"";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_IMAGE_SIZE) + " = 0";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_JOB_INPUT) + " = \"/dev/null\"";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_TRANSFER_INPUT) + " = FALSE";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_JOB_OUTPUT) + " = \"/dev/null\"";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_JOB_ERROR) + " = \"/dev/null\"";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_BUFFER_SIZE) + " = " + (512 * 1024);
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_BUFFER_BLOCK_SIZE) + " = " + (32 * 1024);
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_SHOULD_TRANSFER_FILES) + " = TRUE";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_TRANSFER_FILES) + " = \"ONEXIT\"";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_WHEN_TO_TRANSFER_OUTPUT) + " = \"ON_EXIT\"";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_REQUIREMENTS) + " = (";
	if (requirements) {
		attribute = attribute + requirements;
	} else {
		attribute = attribute + " = TRUE";
	}
	attribute = attribute + ")";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_PERIODIC_HOLD_CHECK) + " = FALSE";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_PERIODIC_RELEASE_CHECK) + " = FALSE";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_PERIODIC_REMOVE_CHECK) + " = FALSE";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_ON_EXIT_HOLD_CHECK) + " = FALSE";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_ON_EXIT_REMOVE_CHECK) + " = TRUE";
	job.Insert(attribute.GetCStr());

	attribute = MyString("FilesRetrieved") + " = FALSE";
	job.Insert(attribute.GetCStr());

	attribute = MyString(ATTR_JOB_LEAVE_IN_QUEUE) +
		" = FilesRetrieved=?=FALSE";
	char *soapLeaveInQueue = param("SOAP_LEAVE_IN_QUEUE");
	if (soapLeaveInQueue) {
		attribute = attribute + " && (" + soapLeaveInQueue + ")";
	}

		// XXX: This is recoverable!
	ASSERT(job.Insert(attribute.GetCStr()));

	attribute = MyString(ATTR_JOB_ARGUMENTS) + " = \"" + args + "\"";
	job.Insert(attribute.GetCStr());

		// Need more attributes! Skim submit.C more.

	result.response.status.code = SUCCESS;
	result.response.status.message = "What a mess!";
	convert_ad_to_adStruct(soap, &job, &result.response.classAd, true);

	return SOAP_OK;
}

bool
Reschedule()
{
		// XXX: Abstract this, it was stolen from Scheduler::reschedule_negotiator!

	scheduler.timeout();		// update the central manager now

	dprintf(D_FULLDEBUG, "Called Reschedule()\n");

	scheduler.sendReschedule();

	scheduler.StartLocalJobs();

	return true;
}

///////////////////////////////////////////////////////////////////////////////
// TODO : This should move into daemonCore once we figure out how we wanna link
///////////////////////////////////////////////////////////////////////////////

#include "../condor_daemon_core.V6/soap_daemon_core.cpp"
