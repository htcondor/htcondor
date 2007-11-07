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

#ifndef PROCESS_ID_H
#define PROCESS_ID_H

//#include "condor_common.h"

/*  A more "concrete" implementation of a process id 
	- Joe Meehean 12/15/05
*/

class ProcessId{
  
		// Variables
 public:
		// constant
	enum { DIFFERENT, SAME, UNCERTAIN, FAILURE, SUCCESS };
 private:
		// constant
	static const char* SIGNATURE_FORMAT;
	static const int NR_OF_SIGNATURE_ENTRIES;
	static const int MIN_NR_OF_SIGNATURE_ENTRIES;  // minimum number of entries to compose a signature
	static const char* CONFIRMATION_FORMAT;
	static const int NR_OF_CONFIRM_ENTRIES;
	static const int UNDEF;
	static const int MAX_INIT_PID;

		// non-constant
	pid_t pid;
	pid_t ppid;
	int precision_range;	
		/* All time fields in a signature must be in the same units.
		   This field specifies how to convert those units back to seconds.
		*/
	double time_units_in_sec;  
	long bday;
	long ctl_time;
	
	bool confirmed;
	long confirm_time;
	
		// Functions
 public:
		/*
		  Constructs a ProcessId from a file where a ProcessId
		  was previously written.  The previous ProcessId must
		  have been written by the same machine during the
		  same boot or the behavior is undefined and probably
		  bad.  The status parameter is an out argument that
		  contains SUCCESS on successful instantion and
		  FAILURE on failure.  Note: Failure still requires
		  that dynamically allocated ProcessIds are deleted.
		*/
	ProcessId(FILE* fp, int& status);
	ProcessId(const ProcessId&);
	ProcessId& operator =(const ProcessId&);
	virtual ~ProcessId();
	
	/*
	  Returns true if this process id has been confirmed unique.
	*/
	int isConfirmed() const;
	
	/*
	  Writes this process id and confirmation to the given file
	  pointer.
	  Returns SUCCESS on success and FAILURE on failure.
	*/
	int write(FILE* fp) const;
	
	/*
	  Writes only the confirmation of this processId.
	  Returns SUCCESS on sucess and FAILURE on failure
	*/
	int writeConfirmationOnly(FILE* fp) const;

	/*
	  Returns the pid of this process.
	*/
	pid_t getPid() const;
	
	/*
	  Calculates the amount of time a parent has to supervise a process
	  before it can guarantee that the process id is unique.  A caller must
	  ensure that a process is not reaped until at least the returned amount
	  of time has passed before confirming the process id.

	  Don't modify this without modifying computeConfirmationBuffer(...)
	*/
	int computeWaitTime() const;
	
	/*
	  Calculates the required amount of time required to prevent an
	  overlap of confirmation time and a new process bday.
 
	  Typically one would subtract the value returned by this function
	  from the confirmation time and use the result of that
	  subtraction as the maximum acceptable birtday of a process.
 
	  Programmer's Note: This function is here instead of just
	  calculated where used because it is dependent on
	  computeWaitTime(...).  If one changes the other must also.
	*/
	int computeConfirmationBuffer() const;

 protected:
		//OCCF helper methods

		/*	
		  Initializes the member data.
		  Helper method for the constructors
		*/
	virtual void init(pid_t pid, pid_t ppid, int precision_range, 
					  double time_units_in_sec, long bday, long ctltime);
		/*
	  Copies the data from the given ProcessId to this one.
		*/
	virtual void deepCopy(const ProcessId&);
		/*
		  Cleans up the memory for this object.
		*/
	virtual void noLeak();

 private:
		// Private to prevent uninitialized use
	ProcessId();
		/* 
		   Constructor that takes a process's pid, its parents pid, it
		   birthday as a long value (in whatever time units is
		   appropriated provided its the same units as the control
		   time), and a control time.  The control time is a time that
		   remains constant as long as a given process's birthday
		   remains constant and shifts in the same direction and
		   magnitude as a birthday.  Essentially used to prevent time
		   anomalies due to admin clock adjustment or ntpd.  All time
		   fields must be in the same units.  These units are
		   specified with the time_units_in_sec parameter which can be
		   used to convert the units back to seconds.

		   The only real constructor, not from disk.  Only 
		   ProcAPI is allowed to call this constructor because
		   it is the only class with the correct (see RAW)
		   information needed to construct an accurate 
		   ProcessId.  Other classes can get ProcessIds by 
		   calling ProcAPI::createProcessId(pid_t pid).
		*/
	ProcessId(pid_t pid, pid_t ppid, int precision_range, double time_units_in_sec, 
			  long bday, long ctltime);
	
		/*
		  Confirms that this process id is unique, and that the
		  process was not reaped as of the confirm time.  This method
		  is only callable by ProcAPI.
  
		  A process MUST ensure that this process is not reaped for
		  the amount of time returned by computeWaitTime() before
		  confirming a this id.  Typically, the only process that can
		  confirm another process is its parent, since it is the only
		  one that can ensure that the process is not reaped.
		  Although, any process that has fail safe mechanism of ensure
		  the process hasn't been reaped, outside of the functions
		  provided in this class, can confirm it.  Think hard before
		  you confirm a process you are not the parent of.

		  Again only ProcAPI can call this function.  Because
		  it is the only class authorized to correctly generate
		  a linked confirm and control time.  Other classes can
		  confirm a ProcessId by calling ProcAPI::confirmProcessId.
		*/
	int confirm(long confirm_time, long ctl_time);

		/*
		   Determines whether the given process id represents
		   the same process.  Returns DIFFERENT, SAME,
		   UNCERTAIN, or FAILURE on error.
 
		   Programmers Note: Assumes that this instance is a stored id
		   that we are comparing against a recently queried process
		   (rhs) with the same pid.  This assumption allows us to use
		   a more forgiving and accurate model of process pid usage.
		   If this assumptions does not hold don't use this function.

		   Again only ProcAPI can call this function.  This is
		   primarily because of the special semantics of the call.  A
		   looser defined version of this call could be implemented
		   that could be public, but it would not be as accurate as
		   this call.
		 */
	int isSameProcess(const ProcessId& rhs) const;
	
		/*	HELPERS, not even ProAPI should call these methods */

		/*
		  Determines whether the given id is from the same process as
		  this confirmed id.  Definitively determines whether the
		  processes are the same. View this as a stronger version of
		  possibleSameProcessId(...) that requires a confirmation.
  
		  Programmers Note: Assumes that this is a stored
		  id that we are comparing against a recently queried process
		  id with the same pid.  This assumption allows us to
		  use a more forgiving and accurate model of process pid usage.  If
		  this assumptions does not hold don't use this function.
		*/
	bool isSameProcessConfirmed(const ProcessId& rhs) const;
	
		/*
		  Determines whether it is possible for these ids to belong to
		  the same process. At most can declare "possible".
 
		  Programmers Note: Assumes that 'this' is a stored id that we
		  are comparing against a recently queried process id with the
		  same pid.  This assumption allows us to use a more forgiving
		  and accurate model of process pid usage.  If this
		  assumptions does not hold don't use this function.
		*/
	bool possibleSameProcessFromId(const ProcessId& rhs) const;
	
		/*
		  Determines whether it is possible for these process ids to
		  belong to the same process based solely on pid and parent
		  pid.  At most can declare "possible".
 
		  Programmers Note: Assumes that this is a stored id that we
		  are comparing against a recently queried process id with the
		  same pid.  This assumption allows us to use a more forgiving
		  and accurate model of process pid usage.  If this
		  assumptions does not hold don't use this function.
		*/
	bool possibleSameProcessFromPpid(const ProcessId& rhs) const;

		/*
		  Shifts this process id into the control time given.
		*/
	void shift(long ctl_time);
	
		/*
		  Shifts the given time from the 'to' control to the 'from'
		  control time.
		*/
	static long shiftTime(long time, long ctl_to, long ctl_from);
	
		/*
		  Extracts the process id information from the given file
		  pointer.  Returns the number of items extracted or EOF if an
		  error occured before the first assignment.
		*/
	static int extractProcessId( FILE* fp,
								 pid_t& extracted_pid,
								 pid_t& extracted_ppid,
								 int& extracted_precision,
								 double& extracted_units_in_sec,
								 long& extracted_bday,
								 long& extracted_ctl_time
								 );
	
		/*
		  Extracts process confirmation information from the given
		  file pointer.  Returns the number of items extracted or EOF
		  if an error occured before the first assignment.
		*/
	static int extractConfirmation( FILE* fp, long& confirm_time, long& ctl_time );

	/*
	  Writes the process id to the given file pointer.
	*/
	int writeId(FILE* fp) const;
	
	/*
	  Writes the confirmation to the given file pointer.
	*/
	int writeConfirmation(FILE* fp) const;


		// ProcAPI is a friend of this class
	friend class ProcAPI;
};

#endif 
