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

int _condor_is_file_name_local( const char *name, char *local_name )
{
	_condor_file_table_init();
	if( MappingFileDescriptors() ) {
		return FileTab->is_file_name_local(name,local_name);
	} else {
		strcpy(local_name,name);
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
