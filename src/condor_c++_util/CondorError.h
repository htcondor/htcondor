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
#ifndef CONDORERROR_H_INCLUDE
#define CONDORERROR_H_INCLUDE

#include "condor_common.h"
#include "MyString.h"

#include "condor_error_codes.h"

class CondorError {

	public:
		CondorError();
		~CondorError();
		CondorError(CondorError&);
		CondorError& operator==(CondorError&);
		void push( char* subsys, int code, char* message );
		void pushf( char* subsys, int code, char* format, ... ); 
		const char* getFullText( bool want_newline = false );
		char* subsys(int level = 0);
		int   code(int level = 0);
		char* message(int level = 0);
		CondorError* next(int level = 0);

		bool  pop();
		void  clear();

	private:
		void init();
		void deep_copy(CondorError&);

		char* _subsys;
		int   _code;
		char* _message;
		CondorError *_next;

};


#endif
