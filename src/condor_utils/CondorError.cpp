/***************************************************************
 *
 * Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
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
#include "condor_debug.h"
#include "stl_string_utils.h"


CondorError::CondorError(const CondorError& copy) {
	init();
	deep_copy(copy);
}

CondorError& CondorError::operator=(const CondorError& copy) {
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

bool CondorError::empty() {
	return !_message && !_subsys && !_next;
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

void CondorError::deep_copy(const CondorError& copy) {
	_subsys = copy._subsys ? strdup(copy._subsys) : NULL;
	_code = copy._code;
	_message = copy._message ? strdup(copy._message) : NULL;
	if(copy._next) {
		_next = new CondorError();
		_next->deep_copy(*(copy._next));
	} else {
		_next = 0;
	}
}

void CondorError::push( const char* the_subsys, int the_code, const char* the_message ) {
	CondorError* tmp = new CondorError();
	if (the_subsys) tmp->_subsys = strdup(the_subsys);
	tmp->_code = the_code;
	if (the_message) tmp->_message = strdup(the_message);
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
	if (tmp->_message)
		vsnprintf ( tmp->_message, l+1, the_format, ap );
	tmp->_next = _next;
	_next = tmp;
	va_end(ap);
}

std::string
CondorError::getFullText( bool want_newline ) const
{
	std::string err_ss;
	bool printed_one = false;

	CondorError* walk = _next;
	while (walk) {
		if( printed_one ) {
			if( want_newline ) {
				err_ss += '\n';
			} else {
				err_ss += '|';
			}
		} else {
			printed_one = true;
		}
		if (walk->_subsys) err_ss += walk->_subsys;
		formatstr_cat(err_ss, ":%d:", walk->_code);
		if (walk->_message) err_ss += walk->_message;
		walk = walk->_next;
	}
	return err_ss;
}

const char*
CondorError::subsys(int level) const
{
	int n = 0;
	CondorError* walk = _next;
	while (walk && n < level) {
		walk = walk->_next;
		n++;
	}
	if (walk && walk->_subsys) {
		return walk->_subsys;
	} else {
		return NULL;
	}
}

int
CondorError::code(int level) const
{
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

const char*
CondorError::message(int level) const
{
	int n = 0;
	CondorError* walk = _next;
	while (walk && n < level) {
		walk = walk->_next;
		n++;
	}
	if (walk && walk->_message) {
		return walk->_message;
	} else {
		return "";
	}
}

void CondorError::walk(bool (*fn)(void*pv, int code, const char * subsys, const char * message), void*pv) const
{
	if (_code || _subsys || _message) {
		if ( ! fn(pv, _code, _subsys, _message))
			return;
	}
	CondorError* ce = _next;
	while (ce) {
		if ( ! fn(pv, ce->_code, ce->_subsys, ce->_message))
			break;
		ce = ce->_next;
	}
}


