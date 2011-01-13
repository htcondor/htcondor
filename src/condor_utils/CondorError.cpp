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
#include "CondorError.h"
#include "condor_snutils.h"


CondorError::CondorError() {
	init();
}
		
CondorError::~CondorError() {
	clear();
}

CondorError::CondorError(CondorError& copy) {
	init();
	deep_copy(copy);
}

CondorError& CondorError::operator=(CondorError& copy) {
	if (&copy != this) {
		clear();
		deep_copy(copy);
	}
	return *this;
}

void CondorError::init() {
	_subsys = 0;
	_code = 0;
	_message = 0;
	_next = 0;
}

void CondorError::clear() {
	if (_subsys) {
		free( _subsys );
		_subsys = 0;
	}
	if (_message) {
		free( _message );
		_message = 0;
	}
	if (_next) {
		delete _next;
		_next = 0;
	}
}

bool CondorError::pop() {
	if (_next) {
		CondorError* tmp = _next->_next;
		_next->_next = 0;
		delete _next;
		_next = tmp;
		return true;
	} else {
		return false;
	}

}

void CondorError::deep_copy(CondorError& copy) {
	_subsys = strdup(copy._subsys);
	_code = copy._code;
	_message = strdup(copy._message);
	if(copy._next) {
		_next = new CondorError();
		_next->deep_copy(*(copy._next));
	} else {
		_next = 0;
	}
}

void CondorError::push( const char* the_subsys, int the_code, const char* the_message ) {
	CondorError* tmp = new CondorError();
	tmp->_subsys = strdup(the_subsys);
	tmp->_code = the_code;
	tmp->_message = strdup(the_message);
	tmp->_next = _next;
	_next = tmp;
}

void CondorError::pushf( const char* the_subsys, int the_code, const char* the_format, ... ) {
	CondorError* tmp = new CondorError();
	tmp->_subsys = strdup(the_subsys);
	tmp->_code = the_code;
	va_list ap;
	va_start(ap, the_format);
	int l = vprintf_length( the_format, ap );
	tmp->_message = (char*)malloc( l+1 );
	vsprintf ( tmp->_message, the_format, ap );
	tmp->_next = _next;
	_next = tmp;
	va_end(ap);
}

const char*
CondorError::getFullText( bool want_newline )
{
	static MyString errbuf;
	bool printed_one = false;

	errbuf = "";
	CondorError* walk = _next;
	while (walk) {
		if( printed_one ) {
			if( want_newline ) {
				errbuf += '\n';
			} else {
				errbuf += '|';
			}
		} else {
			printed_one = true;
		}
		errbuf += walk->_subsys;
		errbuf += ':';
		errbuf += walk->_code;
		errbuf += ':';
		errbuf += walk->_message;
		walk = walk->_next;
	}
	return errbuf.Value();
}

char* CondorError::subsys(int level) {
	int n = 0;
	CondorError* walk = _next;
	while (walk && n < level) {
		walk = walk->_next;
		n++;
	}
	if (walk && walk->_subsys) {
		return walk->_subsys;
	} else {
		return "SUBSYS-NULL";
	}
}

int   CondorError::code(int level) {
	int n = 0;
	CondorError* walk = _next;
	while (walk && n < level) {
		walk = walk->_next;
		n++;
	}
	if (walk) {
		return walk->_code;
	} else {
		return 0;
	}
}

char* CondorError::message(int level) {
	int n = 0;
	CondorError* walk = _next;
	while (walk && n < level) {
		walk = walk->_next;
		n++;
	}
	if (walk && walk->_subsys) {
		return walk->_message;
	} else {
		return "MESSAGE-NULL";
	}
}

