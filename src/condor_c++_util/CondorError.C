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

CondorError& CondorError::operator==(CondorError& copy) {
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
		delete _subsys;
		_subsys = 0;
	}
	if (_message) {
		delete _message;
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
	}
}

void CondorError::push( char* the_subsys, int the_code, char* the_message ) {
	CondorError* tmp = new CondorError();
	tmp->_subsys = strdup(the_subsys);
	tmp->_code = the_code;
	tmp->_message = strdup(the_message);
	tmp->_next = _next;
	_next = tmp;
}

void CondorError::pushf( char* the_subsys, int the_code, char* the_format, ... ) {
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
}

const char* CondorError::get_full_text() {
	static MyString errbuf;

	errbuf = "";
	CondorError* walk = _next;
	while (walk) {
		errbuf += walk->_subsys;
		errbuf += ':';
		errbuf += walk->_code;
		errbuf += ':';
		errbuf += walk->_message;
		errbuf += '\n';
		walk = walk->_next;
	}
	return errbuf.GetCStr();
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

