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
#include "condor_syscall_mode.h"
#include "condor_sys.h"
#include "condor_debug.h"
#include "condor_error.h"
#include "image.h"

/*
Please read the documentation in condor_error.h
*/

#define BUFFER_SIZE 1024

/* here are the remote syscalls we use in this file */
extern "C" int REMOTE_CONDOR_report_error(char *text);

extern "C" {

static void _condor_message( const char *text )
{
	if( MyImage.GetMode()==STANDALONE ) {
		int scm = SetSyscalls(SYS_LOCAL|SYS_UNMAPPED);
		syscall( SYS_write, 2, "Condor: ", 8 );
		syscall( SYS_write, 2, text, strlen(text) );
		syscall( SYS_write, 2, "\n", 1 );
		SetSyscalls(scm);
	} else {
		char* _text = strdup( text );		//de-const
		REMOTE_CONDOR_report_error( _text );
		dprintf(D_ALWAYS, _text);
		free( _text );						//b/c of strdup
	}
}

void _condor_error_retry( const char *format, ... )
{
	char text[BUFFER_SIZE];
	va_list	args;
	va_start(args,format);
	strcpy(text,"Error: ");
	vsprintf( &text[strlen(text)], format, args );
	_condor_message(text);
	Suicide();
	va_end(args);
}

void _condor_error_fatal( const char *format, ... )
{
	char text[BUFFER_SIZE];
	va_list	args;
	va_start(args,format);
	strcpy(text,"Error: ");
	vsprintf( &text[strlen(text)], format, args );
	_condor_message(text);
	exit(-1);
	va_end(args);
}

struct kind_info {
	condor_warning_mode_t mode;
	int count;
	const char *name;
};

static struct kind_info table[] = {
	{CONDOR_WARNING_MODE_ON, 0, "ALL"    },
	{CONDOR_WARNING_MODE_ON, 0, "NOTICE"    },
	{CONDOR_WARNING_MODE_ON, 0, "READWRITE" },
	{CONDOR_WARNING_MODE_ON, 0, "UNSUP",    },
	{CONDOR_WARNING_MODE_ON, 0, "BADURL",   },
};

const char *mode_names[] = {
	"ON",
	"ONCE",
	"OFF",
};

void _condor_warning( condor_warning_kind_t kind, const char *format, ... )
{
	va_list	args;
	char text[BUFFER_SIZE];
	va_start(args,format);

	if( kind<0 || kind>=CONDOR_WARNING_KIND_MAX ) {
		kind = CONDOR_WARNING_KIND_NOTICE;
	}

	if(table[kind].mode==CONDOR_WARNING_MODE_OFF) {
		va_end(args);
		return;
	}

	table[kind].count++;

	if(table[kind].mode==CONDOR_WARNING_MODE_ONCE) {
		if(table[kind].count>1) {
			va_end(args);
			return;
		}
	}

	if(kind==CONDOR_WARNING_KIND_NOTICE) {
		sprintf(text,"Notice: ");
	} else {
		sprintf(text,"Warning: %s: ",table[kind].name);
	}

	vsprintf( &text[strlen(text)], format, args );
	_condor_message(text);

	if(table[kind].mode==CONDOR_WARNING_MODE_ONCE) {
		_condor_message("Warning: Further messages of this type supressed.");
	}

	va_end(args);
}

int _condor_warning_config( condor_warning_kind_t kind, condor_warning_mode_t mode )
{
	int i;

	if( kind>=CONDOR_WARNING_KIND_MAX || kind<0 ) {
		return 0;
	}

	if( kind==CONDOR_WARNING_KIND_ALL ) {
		for(i=0;i<CONDOR_WARNING_KIND_MAX;i++) {
			table[i].mode = mode;
		}
	} else {
		table[kind].mode = mode;
	}

	return 1;
}

static int _condor_warning_kind_parse( const char *str, condor_warning_kind_t *kind )
{
	int i;

	for( i=0; i<CONDOR_WARNING_KIND_MAX; i++ ) {
		if(!stricmp(table[i].name,str)) {
			*kind = (condor_warning_kind_t)i;
			return 1;
		}
	}

	return 0;
}

static int _condor_warning_mode_parse( const char *str, condor_warning_mode_t *mode )
{
	int i;

	for( i=0; i<CONDOR_WARNING_MODE_MAX; i++ ) {
		if(!stricmp(mode_names[i],str)) {
			*mode = (condor_warning_mode_t)i;
			return 1;
		}
	}

	return 0;
}

int _condor_warning_config_byname( const char *kind_str, const char *mode_str )
{
	condor_warning_kind_t kind;
	condor_warning_mode_t mode;

	if(!_condor_warning_kind_parse(kind_str,&kind)) return 0;
	if(!_condor_warning_mode_parse(mode_str,&mode)) return 0;

	return _condor_warning_config(kind,mode);
}

char * _condor_warning_kind_choices()
{
	static char buffer[1024];
	int i;

	buffer[0] = 0;
	for(i=0;i<CONDOR_WARNING_KIND_MAX;i++) {
		strcat(buffer,table[i].name);
		strcat(buffer," ");
	}

	return buffer;
}

char * _condor_warning_mode_choices()
{
	static char buffer[1024];
	int i;

	buffer[0] = 0;
	for(i=0;i<CONDOR_WARNING_MODE_MAX;i++) {
		strcat(buffer,mode_names[i]);
		strcat(buffer," ");
	}

	return buffer;
}

} /* end extern C */

