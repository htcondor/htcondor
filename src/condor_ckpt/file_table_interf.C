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
#include "syscall_numbers.h"
#include "condor_debug.h"

#include "file_table_interf.h"
#include "file_state.h"
#include "signals_control.h"

extern "C" {

int _condor_get_unmapped_fd( int user_fd )
{
	_condor_file_table_init();
	if( MappingFileDescriptors() ) {
		return FileTab->get_unmapped_fd(user_fd);
	} else {
		return user_fd;
	}
}

int _condor_is_fd_local( int user_fd )
{
	_condor_file_table_init();
	if( MappingFileDescriptors() ) {
		return FileTab->is_fd_local(user_fd);
	} else {
		return LocalSysCalls();
	}
}

int _condor_is_file_name_local( const char *name, char **local_name )
{
	assert( local_name != NULL && *local_name == NULL );

	_condor_file_table_init();
	if( MappingFileDescriptors() ) {
		return FileTab->is_file_name_local(name,local_name);
	} else {
		*local_name = strdup(name);
		return LocalSysCalls();
	}
}

void _condor_file_table_dump()
{
	_condor_file_table_init();
	FileTab->dump();
}

void _condor_file_table_aggravate( int on_off )
{
	_condor_file_table_init();
	FileTab->set_aggravate_mode( on_off );
}

void _condor_file_table_checkpoint()
{
	_condor_file_table_init();
	FileTab->checkpoint();
}

void _condor_file_table_resume()
{
	_condor_file_table_init();
	FileTab->resume();
}

void _condor_file_table_init()
{
	if(!FileTab) {
		FileTab = new CondorFileTable();
		FileTab->init();
	}
}

} // extern "C"
