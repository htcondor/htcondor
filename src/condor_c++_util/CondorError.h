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
		const char* get_full_text();
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
