/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
