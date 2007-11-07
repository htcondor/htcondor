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


#include "condor_common.h"
#include "condor_debug.h"

#include "JobLogReader.h"

#include <string>


JobLogReader::JobLogReader() {
}

void
JobLogReader::SetJobLogFileName(char const *fname) {
	parser.setJobQueueName((char *)fname);
}

char const *
JobLogReader::GetJobLogFileName() {
	return parser.getJobQueueName();
}

void
JobLogReader::poll(classad::ClassAdCollection *ad_collection) {
	ProbeResultType probe_st;
	FileOpErrCode fst;

	fst = parser.openFile();
	if(fst == FILE_OPEN_ERROR) {
		dprintf(D_ALWAYS,"Failed to open %s: errno=%d\n",parser.getJobQueueName(),errno);
		return;
	}

	probe_st = prober.probe(parser.getLastCALogEntry(),parser.getFileDescriptor());

	bool success = true;
	switch(probe_st) {
	case INIT_QUILL:
	case COMPRESSED:
	case PROBE_ERROR:
		success = BulkLoad(ad_collection);
		break;
	case ADDITION:
		success = IncrementalLoad(ad_collection);
		break;
	case NO_CHANGE:
		break;
	}

	parser.closeFile();

	if(success) {
		// update prober to most recent observations about the job log file
		prober.incrementProbeInfo();
	}
}
static void ClearCollection(classad::ClassAdCollection *ad_collection) {
	classad::LocalCollectionQuery query;
	std::string key;

	query.Bind(ad_collection);
	query.Query("root", NULL);
	query.ToFirst();

	if(query.Current(key)) {
		do {
			ad_collection->RemoveClassAd(key);
		}while(query.Next(key));
	}
}
bool JobLogReader::BulkLoad(classad::ClassAdCollection *ad_collection) {
	parser.setNextOffset(0);
	ClearCollection(ad_collection);
	return IncrementalLoad(ad_collection);
}
bool JobLogReader::IncrementalLoad(classad::ClassAdCollection *ad_collection) {
	FileOpErrCode err;
	do {
		int op_type;

		err = parser.readLogEntry(op_type);
		if (err == FILE_READ_SUCCESS) {
			//dprintf(D_ALWAYS, "Read op_type %d\n",op_type);
			bool processed = ProcessLogEntry(parser.getCurCALogEntry(), ad_collection, &parser);
			if(!processed) {
				dprintf(D_ALWAYS, "ERROR reading %s: Failed to process log entry.\n",GetJobLogFileName());
				return false;
			}
		}
	}while(err == FILE_READ_SUCCESS);
	if (err != FILE_READ_EOF) {
		dprintf(D_ALWAYS, "Error reading from %s: %d, %d\n",GetJobLogFileName(),err,errno);
		return false;
	}
	return true;
}

/*! read the body of a log Entry; update a new ClassAd collection.
 */
bool
JobLogReader::ProcessLogEntry( ClassAdLogEntry *log_entry, classad::ClassAdCollection *ad_collection, ClassAdLogParser *caLogParser )
{

	switch(log_entry->op_type) {
	case CondorLogOp_NewClassAd: {
		classad::ClassAd* ad;
		bool using_existing_ad = false;

		ad = ad_collection->GetClassAd(log_entry->key);
		if(ad && ad->size() == 0) {
				//This is a cluster ad created in advance so that children
				//could be chained from it.
			using_existing_ad = true;
		}
		else {
			ad = new classad::ClassAd();
		}

		//The following classad methods are deprecated.
		//We may not need these anyway, so leave them commented out for now.
		//ad->SetMyTypeName((const char *)log_entry->mytype);
		//ad->SetTargetTypeName((const char *)log_entry->targettype);

		//Chain this ad to its parent, if any.
		PROC_ID proc = getProcByString(log_entry->key);
		if(proc.proc >= 0) {
			char cluster_key[50];
			//NOTE: cluster keys start with a 0: e.g. 021.-1
			sprintf(cluster_key,"0%d.-1",proc.cluster);
			classad::ClassAd* cluster_ad = ad_collection->GetClassAd(cluster_key);
			if(!cluster_ad) {
					// The cluster ad doesn't exist yet.  This is expected.
					// For example, once the job queue is rewritten (i.e.
					// truncated), the order of entries in it is arbitrary.
				cluster_ad = new classad::ClassAd();
				if(!ad_collection->AddClassAd(cluster_key,cluster_ad)) {
					dprintf(D_ALWAYS,"ERROR processing %s: failed to add '%s' to ClassAd collection.\n",GetJobLogFileName(),cluster_key);
					break;
				}
			}

			ad->ChainToAd(cluster_ad);
		}

		if( !using_existing_ad ) {
			if(!ad_collection->AddClassAd(log_entry->key,ad)) {
				dprintf(D_ALWAYS,"ERROR processing %s: failed to add '%s' to ClassAd collection.\n",GetJobLogFileName(),log_entry->key);
			}
		}

		break;
	}
	case CondorLogOp_DestroyClassAd: {
		ad_collection->RemoveClassAd(log_entry->key);
		break;
	}
	case CondorLogOp_SetAttribute: {	
		classad::ClassAd *ad = ad_collection->GetClassAd(log_entry->key); //pointer to classad in collection
		if(!ad) {
			dprintf(D_ALWAYS, "ERROR reading %s: no such ad in collection: %s\n",GetJobLogFileName(),log_entry->key);
			return false;
		}
		//NOTE: assuming here that we can parse the old-style classad expression using the new classad parser
		classad::ClassAdParser parser;

		classad::ExprTree *expr = parser.ParseExpression(log_entry->value);
		if(!expr) {
			dprintf(D_ALWAYS, "ERROR reading %s: failed to parse expression: %s\n",GetJobLogFileName(),log_entry->value);
			ASSERT(expr);
			return false;
		}
		ad->Insert(log_entry->name,expr);

		break;
	}
	case CondorLogOp_DeleteAttribute: {
		classad::ClassAd *ad = ad_collection->GetClassAd(log_entry->key);
		if(!ad) {
			dprintf(D_ALWAYS, "ERROR reading %s: no such ad in collection: %s\n",GetJobLogFileName(),log_entry->key);
			return false;
		}
		ad->Delete(log_entry->name);

		// The above will return false if the attribute doesn't exist
		// in the ad.  However, this is expected, because the schedd
		// sometimes will blindly delete attributes that do not exist
		// (e.g. RemoteSlotID).  Therefore, we ignore the return
		// value.

		break;
	}
	case CondorLogOp_BeginTransaction:
		// Transactions may be ignored, because the transaction will either
		// be completed eventually, or the log writer will backtrack
		// and wipe out the transaction, which will cause us to do a
		// bulk reload.
		break;
	case CondorLogOp_EndTransaction:
		break;
	case CondorLogOp_LogHistoricalSequenceNumber:
		break;
	default:
		dprintf(D_ALWAYS, "ERROR reading %s: Unsupported Job Queue Command\n",GetJobLogFileName());
		return false;
	}
	return true;
}
