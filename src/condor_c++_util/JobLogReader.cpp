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


JobLogReader::JobLogReader(JobLogConsumer *consumer):
	m_consumer(consumer)
{
	m_consumer->SetJobLogReader(this);
}

JobLogReader::~JobLogReader()
{
	if (m_consumer) {
		delete m_consumer;
		m_consumer = NULL;
	}
}

void
JobLogReader::SetJobLogFileName(char const *fname)
{
	parser.setJobQueueName(fname);
}


char const *
JobLogReader::GetJobLogFileName()
{
	return parser.getJobQueueName();
}


void
JobLogReader::Poll() {
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
		success = BulkLoad();
		break;
	case ADDITION:
		success = IncrementalLoad();
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


bool
JobLogReader::BulkLoad()
{
	parser.setNextOffset(0);
	m_consumer->Reset();
	return IncrementalLoad();
}


bool
JobLogReader::IncrementalLoad()
{
	FileOpErrCode err;
	do {
		int op_type;

		err = parser.readLogEntry(op_type);
		if (err == FILE_READ_SUCCESS) {
			//dprintf(D_ALWAYS, "Read op_type %d\n",op_type);
			bool processed = ProcessLogEntry(parser.getCurCALogEntry(), &parser);
			if(!processed) {
				dprintf(D_ALWAYS, "error reading %s: Failed to process log entry.\n",GetJobLogFileName());
				return false;
			}
		}
	}while(err == FILE_READ_SUCCESS);
	if (err != FILE_READ_EOF) {
		dprintf(D_ALWAYS, "error reading from %s: %d, %d\n",GetJobLogFileName(),err,errno);
		return false;
	}
	return true;
}


/*! read the body of a log Entry.
 */
bool
JobLogReader::ProcessLogEntry(ClassAdLogEntry *log_entry,
							  ClassAdLogParser */*caLogParser*/)
{

	switch(log_entry->op_type) {
	case CondorLogOp_NewClassAd:
		return m_consumer->NewClassAd(log_entry->key,
									  log_entry->mytype,
									  log_entry->targettype);
	case CondorLogOp_DestroyClassAd:
		return m_consumer->DestroyClassAd(log_entry->key);
	case CondorLogOp_SetAttribute:
		return m_consumer->SetAttribute(log_entry->key,
										log_entry->name,
										log_entry->value);
	case CondorLogOp_DeleteAttribute:
		return m_consumer->DeleteAttribute(log_entry->key,
										   log_entry->name);
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
		dprintf(D_ALWAYS, "error reading %s: Unsupported Job Queue Command\n",GetJobLogFileName());
		return false;
	}
	return true;
}
