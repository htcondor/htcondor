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

class ProcessId{

 public:

	enum { DIFFERENT, SAME, UNCERTAIN, FAILURE, SUCCESS };

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

 private:
		// Private to prevent uninitialized use
	ProcessId();

		// ProcAPI is a friend of this class
	friend class ProcAPI;
};

#endif
