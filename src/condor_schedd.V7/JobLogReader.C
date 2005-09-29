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

	probe_st = prober.probe(parser.getLastCALogEntry(),parser.getJobQueueName());

	bool success = true;
	switch(probe_st) {
	case INIT_QUILL:
	case COMPRESSED:
	case ERROR:
		success = BulkLoad(ad_collection);
		break;
	case ADDITION:
		success = IncrementalLoad(ad_collection);
		break;
	case NO_CHANGE:
		break;
	}
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
			dprintf(D_ALWAYS, "Read op_type %d\n",op_type);
			bool processed = ProcessLogEntry(parser.getCurCALogEntry(), ad_collection, &parser);
			if(!processed) {
				dprintf(D_ALWAYS, "Failed to process log entry.\n");
				return false;
			}
		}
	}while(err == FILE_READ_SUCCESS);
	if (err != FILE_READ_EOF) {
		dprintf(D_ALWAYS, "Error reading from %s: %d, %d\n",parser.getJobQueueName(),err,errno);
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
		classad::ClassAd* ad = new classad::ClassAd();

		//The following classad methods are deprecated.
		//We may not need these anyway, so leave them commented out for now.
		//ad->SetMyTypeName((const char *)log_entry->mytype);
		//ad->SetTargetTypeName((const char *)log_entry->targettype);

		ad_collection->AddClassAd(log_entry->key,ad);

		break;
	}
	case CondorLogOp_DestroyClassAd: {
		ad_collection->RemoveClassAd(log_entry->key);
		break;
	}
	case CondorLogOp_SetAttribute: {	
		classad::ClassAd *ad = ad_collection->GetClassAd(log_entry->key); //pointer to classad in collection
		if(!ad) {
			dprintf(D_ALWAYS, "ERROR: no such ad in collection: %s\n",log_entry->key);
			return false;
		}
		//NOTE: assuming here that we can parse the old-style classad expression using the new classad parser
		classad::ClassAdParser parser;

		classad::ExprTree *expr = parser.ParseExpression(log_entry->value);
		if(!expr) {
			dprintf(D_ALWAYS, "ERROR: failed to parse expression: %s\n",log_entry->value);
			ASSERT(expr);
			return false;
		}
		ad->Insert(log_entry->name,expr);

		break;
	}
	case CondorLogOp_DeleteAttribute: {
		classad::ClassAd *ad = ad_collection->GetClassAd(log_entry->key);
		if(!ad) {
			dprintf(D_ALWAYS, "ERROR: no such ad in collection: %s\n",log_entry->key);
			return false;
		}
		if(!ad->Delete(log_entry->name)) {
			dprintf(D_ALWAYS, "ERROR: no such attribute name '%s' in %s\n",log_entry->name,log_entry->key);
			return false;
		}
		break;
	}
	case CondorLogOp_BeginTransaction:
		dprintf(D_ALWAYS, "DEBUG: BeginTransaction event encountered in ClassAd log.  Not implemented.\n");
		break;
	case CondorLogOp_EndTransaction:
		dprintf(D_ALWAYS, "DEBUG: EndTransaction event encountered in ClassAd log.  Not implemented.\n");
		break;
	default:
		dprintf(D_ALWAYS, "[QUILL] Unsupported Job Queue Command\n");
		return false;
	}
	return true;
}
