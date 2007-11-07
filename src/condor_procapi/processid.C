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
#include "processid.h"
#include "condor_debug.h"

//////////////////////////////////////////////////////////////////////
// Constant static variables
//////////////////////////////////////////////////////////////////////
const char* ProcessId::SIGNATURE_FORMAT = "PPID = %i\nPID = %i\nPRECISION = %i\nTIME_UNITS_IN_SECS = %f\nBDAY = %li\nCONTROL_TIME = %li\n";
const int ProcessId::NR_OF_SIGNATURE_ENTRIES = 6;
const int ProcessId::MIN_NR_OF_SIGNATURE_ENTRIES = 2;
const char* ProcessId::CONFIRMATION_FORMAT = "CONFIRM = %li\nCONTROL_TIME = %li\n";
const int ProcessId::NR_OF_CONFIRM_ENTRIES = 2;
const int ProcessId::UNDEF = -1;
#ifdef WIN32
const int ProcessId::MAX_INIT_PID = 0;
#else //LINUX
const int ProcessId::MAX_INIT_PID = 300;
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ProcessId::ProcessId(pid_t p_id, pid_t pp_id, 
					 int prec_range, 
					 double time_units_sec,
					 long b_day, long ctltime)
{
	init(p_id, pp_id, prec_range, time_units_sec, b_day, ctltime);
}

ProcessId::ProcessId(FILE* fp, int& status)
{
		// assume failure
	status = FAILURE;
	
		// extract the signature
	pid_t tmp_pid = UNDEF;
	pid_t tmp_ppid = UNDEF;
	long tmp_bday = UNDEF;
	int tmp_precision_range = UNDEF;
	double tmp_time_units_in_sec = (double)UNDEF;
	long tmp_id_ctl_time = UNDEF;
	int nr_extracted = extractProcessId( fp, 
										 tmp_ppid, 
										 tmp_pid,
										 tmp_precision_range, 
										 tmp_time_units_in_sec,
										 tmp_bday, tmp_id_ctl_time );

		// check for errors
	if( nr_extracted == FAILURE ){
		dprintf(D_ALWAYS, 
				"ERROR: Failed extract the process id in  "
				"ProcessId::ProcessId(char*, int&)\n");
		status = FAILURE;
		return;
	}
    
		// use the extracted values to initialize this class	
	init(tmp_pid, tmp_ppid, tmp_precision_range, tmp_time_units_in_sec, 
		 tmp_bday, tmp_id_ctl_time);
  
		// attempt to get the latest confirmation
	long tmp_confirm_time = UNDEF;
	long tmp_confirm_ctl_time = UNDEF;	
	if( nr_extracted == NR_OF_SIGNATURE_ENTRIES ){

			// continue reading until the end of the file
			// to ensure we get the latest confirmation
		while( nr_extracted != FAILURE  ){
			
				// extract the confirmation
			nr_extracted = extractConfirmation(fp,
											   tmp_confirm_time,
											   tmp_confirm_ctl_time);

				// only keep the confirmation if it is a complete entry
			if( nr_extracted == NR_OF_CONFIRM_ENTRIES ){
				confirm(tmp_confirm_time, tmp_confirm_ctl_time);
			}

		}
    
	}	
	
		// success
	status = SUCCESS;
}

ProcessId::ProcessId(const ProcessId& orig)
{
	deepCopy(orig);
}

ProcessId& 
ProcessId::operator =(const ProcessId& rhs)
{
	if( this != &rhs ){
		noLeak();
		deepCopy(rhs);
	}

	return( *this );
}

ProcessId::~ProcessId()
{
	noLeak();
}

void 
ProcessId::init(pid_t p_id, pid_t pp_id, 
				int prec_range, 
				double time_units_sec,
				long b_day, long ctltime)
{
	this->pid = p_id;
	this->ppid = pp_id;
	this->precision_range = prec_range;
	this->time_units_in_sec = time_units_sec;
	this->bday = b_day;
	this->ctl_time = ctltime;


	confirmed = false;
	confirm_time = 0;
}

void
ProcessId::deepCopy(const ProcessId& orig)
{
	this->pid = orig.pid;
	this->ppid = orig.ppid;
	this->precision_range = orig.precision_range;
	this->time_units_in_sec = orig.time_units_in_sec;
	this->bday = orig.bday;
	this->ctl_time = orig.ctl_time;

	this->confirmed = orig.confirmed;
	this->confirm_time = orig.confirm_time;
}

void
ProcessId::noLeak()
{
		// No dynamic memory to deallocate
}

///////////////////////////////////////////////////////////////////////
// Methods/Functions
///////////////////////////////////////////////////////////////////////
int
ProcessId::isSameProcess(const ProcessId& rhs) const
{
	
		// assume failure
	int returnVal = FAILURE;

		// Determine what we have
  
		// confirmed with all data
	if( this->confirmed &&
		this->pid != UNDEF && rhs.pid != UNDEF &&
		this->ppid != UNDEF && rhs.ppid != UNDEF &&
		this->precision_range != UNDEF &&
		this->time_units_in_sec != (double)UNDEF &&
		this->bday != UNDEF && rhs.bday != UNDEF &&
		this->ctl_time != UNDEF && rhs.ctl_time != UNDEF ){
		
		bool same = isSameProcessConfirmed(rhs);
			// convert into a return value
		if( same ){
			returnVal = SAME;
		} else{
			returnVal = DIFFERENT;
		}
	}

		// pid + ppid + bday + control time
		// but not confirmed
	else if( this->pid != UNDEF && rhs.pid != UNDEF &&
			 this->ppid != UNDEF && rhs.ppid != UNDEF &&
			 this->precision_range != UNDEF && 
			 this->time_units_in_sec != (double) UNDEF && 
			 this->bday != UNDEF && rhs.bday != UNDEF &&
			 this->ctl_time != UNDEF && rhs.ctl_time != UNDEF ){

		bool possible = possibleSameProcessFromId(rhs);
			// convert into a return value
		if( possible ){
			returnVal = UNCERTAIN;
		} else{
			returnVal = DIFFERENT;
		}

	}

		//  ppid + pid (+ bday - control time)
	else if( this->pid != UNDEF && rhs.pid != UNDEF &&
			 this->ppid != UNDEF && rhs.ppid != UNDEF ){
		
		bool possible = possibleSameProcessFromPpid( rhs );

			// convert into a return value
		if( possible ){
			returnVal = UNCERTAIN;
		}else{
			returnVal = DIFFERENT;
		}
	}

		// pid only
	else if( this->pid != UNDEF && rhs.pid != UNDEF ){
		
		if( this->pid == rhs.pid ){
			returnVal = UNCERTAIN;
		} else{
			returnVal = DIFFERENT;
		}
	}

		// nothing
	else{
			// nothing from nothing leaves nothing
			// or gigo	
		returnVal = UNCERTAIN;
	}

	return( returnVal );
}

bool
ProcessId::isSameProcessConfirmed(const ProcessId& rhs) const
{
		// create a copy of the given process id
		// and shift it into the same control time
		// as this process id
	ProcessId shiftedRhs(rhs);
	shiftedRhs.shift(this->ctl_time);

		// set the largest possible birthday
	int confirm_buffer = computeConfirmationBuffer();
	long right_range_post = this->confirm_time - confirm_buffer;
	
	// only possible if it is also possible from ppid
	bool possible_from_ppid = possibleSameProcessFromPpid( shiftedRhs );
							 
		// only possible if the queried bday is less than the largest acceptable bday
	bool possible_from_bday = shiftedRhs.bday <= right_range_post;

		// combine
	bool same = possible_from_ppid && possible_from_bday;

	return( same );
}

bool
ProcessId::possibleSameProcessFromId(const ProcessId& rhs) const
{
	
		// create a copy of the given process id
		// and shift it into the same control time
		// as this process id
	ProcessId shiftedRhs(rhs);
	shiftedRhs.shift(this->ctl_time);
	
	
		// set the largest acceptable bday
	long right_range_post = this->bday + this->precision_range;
  
		// only possible if it is also possible from ppid.
	bool possible_from_ppid = possibleSameProcessFromPpid( shiftedRhs );

		//only possible if the queried bday is less than the largest acceptable bday
	bool possible_from_bday = rhs.bday <= right_range_post;

		// determine whether it is possible
	bool possible = possible_from_ppid && possible_from_bday;
 
	return( possible );
}

bool
ProcessId::possibleSameProcessFromPpid(const ProcessId& rhs) const
{
	
	bool possible = this->pid == rhs.pid &&
		(this->ppid == rhs.ppid || rhs.ppid	 < MAX_INIT_PID);
	
	return( possible );
}

void
ProcessId::shift(long to_ctl_time)
{
		// shift the birthday
	this->bday = shiftTime(this->bday, to_ctl_time, this->ctl_time);	

	
		// shift the confirmation time
		// if neccessary
	if( confirmed ){
		this->confirm_time = shiftTime(this->confirm_time, 
									   to_ctl_time, 
									   this->ctl_time);
	}

		// reset the control time
	this->ctl_time = to_ctl_time;
}

long 
ProcessId::shiftTime(long time, long ctl_to, long ctl_from)
{
	long diff_time = ctl_to - ctl_from;
	return( time + diff_time );
}

int 
ProcessId::computeWaitTime() const
{

		// convert the precision range to seconds
	double range_sec = ((double)precision_range) / time_units_in_sec;

		// calculate the time to sleep until
		// 1 range for the child
		// 1 range as a buffer
		// 1 range for the process that will reuse this pid
	int sleepLength = (int)ceil(3.0 * range_sec);

		// ensure it is atleast one second
	if( sleepLength < 1 ){
		sleepLength = 1;
	}

	return( sleepLength );
}

int 
ProcessId::computeConfirmationBuffer() const
{

		// accept anything within 2 precision errors of the 
		// last confirmed alive time 
		// 1 to prevent range overlap
		// 1 as a safety buffer
	int bufferLen = 2 * precision_range;
	return ( bufferLen );
}

int
ProcessId::isConfirmed() const
{
	return( this->confirmed );
}

int
ProcessId::write(FILE* fp) const
{

		// write id
	if( writeId(fp) == FAILURE ){
		return FAILURE;
	}
		// write confirmation, if neccessary
	if( this->confirmed ){
		if( writeConfirmation(fp) == FAILURE ){
			return FAILURE;
		}
	}

		// success
	return SUCCESS;
}

int
ProcessId::writeConfirmationOnly(FILE* fp) const
{
	
		// write confirmation
		// if this id is confirmed
	if( this->confirmed ){
		if( writeConfirmation(fp) == FAILURE ){
			return FAILURE;
		}
	}
		// its an error otherwise
	else{
		dprintf(D_PROCFAMILY, "ERROR: Attempted to write a confirmation for a process id that was not confirmed");
		return FAILURE;
	}

	return SUCCESS;
}

int
ProcessId::writeId(FILE* fp) const
{
	
		// write the id
    int retval = fprintf(fp, 
						 SIGNATURE_FORMAT,
						 this->ppid,
						 this->pid,
						 this->precision_range,
						 this->time_units_in_sec,
						 this->bday,
						 this->ctl_time);

    if( retval < 0 ){
		dprintf(D_ALWAYS, "ERROR: Could not write the process signature: %s",
				strerror( ferror(fp) )
				);
		return FAILURE;
    }

		// flush the write
    fflush(fp);

		// success
    return SUCCESS;
}

int
ProcessId::writeConfirmation(FILE* fp) const
{
	
		// write the confirmation
	int retval = fprintf(fp,
						 CONFIRMATION_FORMAT,
						 this->confirm_time,
						 this->ctl_time);

		// error
	if( retval < 0 ){
		dprintf(D_ALWAYS, "ERROR: Could not write the confirmation: %s",
				strerror( ferror(fp) )
				);
		return FAILURE;
	}

		//flush the write
	fflush(fp);
  
		// success
	return SUCCESS;
}

int
ProcessId::extractProcessId( FILE* fp,
							 pid_t& extracted_ppid,
							 pid_t& extracted_pid,
							 int& extracted_precision,
							 double& extracted_units_in_sec,
							 long& extracted_bday,
							 long& extracted_ctl_time)
{
	
		// 1. Run the scan
	int nr_extracted = fscanf( fp, 
							   SIGNATURE_FORMAT, 
							   &extracted_ppid,
							   &extracted_pid,
							   &extracted_precision,
							   &extracted_units_in_sec,
							   &extracted_bday,
							   &extracted_ctl_time );

		// 2. Check for errors

		// ensure there was not an end of file
	if( nr_extracted == EOF ){
		dprintf(D_ALWAYS, 
				"ERROR: Failed to match any entries in "
				"ProcessId::extractProcessId(...)\n");
		return FAILURE;
	}

		// ensure we got the minimum
	else if( nr_extracted < MIN_NR_OF_SIGNATURE_ENTRIES ){
		dprintf(D_ALWAYS, 
				"ERROR: Failed to match sufficient entries in "
				"ProcessId::extractProcessId(...)\n");
		return FAILURE;
	}
	
	
		// return the number of entries extracted
	return( nr_extracted );
}

int
ProcessId::extractConfirmation( FILE* fp, 
								long& extracted_confirm_time, 
								long& extracted_ctl_time )
{
		// 1. Run the scan
	int nr_extracted = fscanf( fp, 
							   CONFIRMATION_FORMAT,
							   &extracted_confirm_time,
							   &extracted_ctl_time );

		// 2. Check for errors

	if( nr_extracted == EOF || nr_extracted == 0 ){
		dprintf(D_PROCFAMILY, 
				"ERROR: Failed to match any entries in "
				"ProcessId::extractConfirmation(char*, int&)\n");
		return FAILURE;
	}
	
	return( nr_extracted );
}

pid_t
ProcessId::getPid() const
{
	return(this->pid);
}

int
ProcessId::confirm(long confirmtime, long ctltime)
{
		// ensure all the fields are filled
		// it is an error to confirm a partial id
	if( this->pid == UNDEF || this->ppid == UNDEF || 
		this->precision_range == UNDEF || 
		this->time_units_in_sec == (double) UNDEF ||
		this->bday == UNDEF || this->ctl_time == UNDEF ){
		dprintf(D_ALWAYS,
				"ProcessId: Cannot confirm a partially filled process id: %d\n",
				this->pid);
		return FAILURE;
	}

		// shift the confirm time into this process ids
		// control time that way we don't need to store
		// 2 control times
	this->confirm_time = shiftTime(confirmtime, this->ctl_time, ctltime);

		// set this process id confirmed
	this->confirmed = true;

		// success
	return SUCCESS;
}
