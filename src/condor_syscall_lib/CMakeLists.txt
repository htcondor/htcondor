 ###############################################################
 # 
 # Copyright 2011 Red Hat, Inc. 
 # 
 # Licensed under the Apache License, Version 2.0 (the "License"); you 
 # may not use this file except in compliance with the License.  You may 
 # obtain a copy of the License at 
 # 
 #    http://www.apache.org/licenses/LICENSE-2.0 
 # 
 # Unless required by applicable law or agreed to in writing, software 
 # distributed under the License is distributed on an "AS IS" BASIS, 
 # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 # See the License for the specific language governing permissions and 
 # limitations under the License. 
 # 
 ############################################################### 

##############################################################
## Abandon all hope ye who enter here:
## this be the mystical workings of the infamous
## standard universe.  No moral platitudes apply, so
## batton your hatches, find yourself a cup of ale,
## because you are about to loose your mind.
## YARRR!
##############################################################
if( STD_UNIVERSE )

	###########################################################################
	# set the local build to debug only *no optimizations*
	set( CMAKE_BUILD_TYPE Debug )

	##########################################################################
	# localized std:u compilation flags and settings
	include (StdUnivExtractNToUpper)
	add_definitions(${STD_U_C_FLAGS} -DREMOTE_SYSCALLS -DSKIP_AUTHENTICATION -DHEADER_DES_LOCL_H -DFILE_TABLE )
	include_directories(${CONDOR_SOURCE_DIR}/src/condor_shadow.std ${CONDOR_SOURCE_DIR}/src/condor_ckpt ${CMAKE_CURRENT_SOURCE_DIR})
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")

	###########################################################################
	# Begin symbol transformation.
	#
	# Summary - standard universe is extracting specific standard .o files which
	# it then renames the symbols and inserts it's own proxy/symbols in their place.
	# which can call the originals.
	###########################################################################
	exec_program ( gcc ARGS -print-file-name=crt1.o OUTPUT_VARIABLE CRT1 )
	exec_program ( gcc ARGS -print-file-name=libgcc.a OUTPUT_VARIABLE LIBGCC )
	exec_program ( gcc ARGS -print-file-name=libgcc_eh.a OUTPUT_VARIABLE LIBGCC_EH )
	exec_program ( gcc ARGS -print-file-name=libstdc++.a OUTPUT_VARIABLE STDCPLUS )
	exec_program ( objcopy ARGS --redefine-sym main=MAIN ${CRT1} ${CMAKE_CURRENT_BINARY_DIR}/condor_rt0.o )

	##################################################
	# rip from custom built glibc and uppercase the defined symbols, yeah you read that correctly :-/
	stdu_extract_and_toupper(${EXT_GLIBC_FOUND} getcwd.o "getcwd;__getcwd")
	stdu_extract_and_toupper(${EXT_GLIBC_FOUND} mmap.o "mmap;__mmap")
	stdu_extract_and_toupper(${EXT_GLIBC_FOUND} syscall.o "syscall")
	stdu_extract_and_toupper(${EXT_GLIBC_FOUND} geteuid.o "geteuid;__geteuid")
	stdu_extract_and_toupper(${EXT_GLIBC_FOUND} getegid.o "getegid;__getegid")
	stdu_extract_and_toupper(${EXT_GLIBC_FOUND} getppid.o "getppid;__getppid")
	stdu_extract_and_toupper(${EXT_GLIBC_FOUND} fork.o "fork;__fork;__libc_fork")
	stdu_extract_and_toupper(${EXT_GLIBC_FOUND} pipe.o "pipe;__pipe")
	stdu_extract_and_toupper(${EXT_GLIBC_FOUND} getlogin.o "getlogin")
	stdu_extract_and_toupper(${EXT_GLIBC_FOUND} sysdep.o "__syscall_error;__syscall_error_1")
	stdu_extract_and_toupper(${EXT_GLIBC_FOUND} sigsuspend.o "sigsuspend;__sigsuspend;__libc_sigsuspend")
	stdu_extract_and_toupper(${EXT_GLIBC_FOUND} truncate.o "truncate;__truncate")
	stdu_extract_and_toupper(${EXT_GLIBC_FOUND} sigaction.o "sigaction;__sigaction;__libc_sigaction")
	stdu_extract_and_toupper(${EXT_GLIBC_FOUND} getdirentries.o "getdirentries")

	##################################################
	# part 1:
	# Below is some clever magic to
	# elim the need of shuffling objs, instead find
	# matching refs in other libs, build locally with
	# the previously declared compilation flags/definitions and
	# slam into archive.  Uber pandantic b/c std:u
	# is concentrated evil.
	##################################################
	src_target_ref( ckpt "^.*$" CKPT_REFS)
	src_target_ref( condor_utils
			"flock;dprintf_common;except;update_rusage;internet;mkargv;filename_tools;strupr;mark_thread;condor_version;condor_ver_info;stat_wrapper;stat_wrapper_internal;condor_open;selector;MyString;subsystem_info;HashTable;get_random_num;condor_sockaddr;condor_netaddr;condor_sockfunc_ex;stream;sig_num;open_flags;fcntl_cmd;errno_num;condor_rw" UTIL_REFS)
	src_target_ref( stdunivio "std_univ_sock;std_univ_buffers" STDIO_REF )

	# local references used by condor_shadow.std & condor_starter.std
	src_target_ref(ckpt "syscall_mode;signals_control" CKPT_LOCAL_REFS)
	condor_selective_glob("remote_syscall*;syscall_connect*;syscall_numbers*;xfer_file*;file_stream*;extern_path*" SYSCALL_LOCAL_REFS)

	##################################################
	# begin all the stub-gen targets fyi - cmake's ";" == " "
	get_target_property(STUB_GEN stub_gen LOCATION)
	dump_local_flags(COMP_DEFS)
	command_target (switches ${CMAKE_C_COMPILER} "-E;-x;c-header;-DSWITCH;${COMP_DEFS};${CMAKE_CURRENT_SOURCE_DIR}/syscall.tmpl;|;${STUB_GEN};-mode;switches;-p;${CMAKE_CURRENT_SOURCE_DIR}/switches.prologue;-e;${CMAKE_CURRENT_SOURCE_DIR}/switches.epilogue;>;switches.cpp" switches.cpp)
	command_target (senders ${CMAKE_C_COMPILER} "-E;-x;c-header;-DSENDER;${COMP_DEFS};${CMAKE_CURRENT_SOURCE_DIR}/syscall.tmpl;|;${STUB_GEN};-mode;senders;-p;${CMAKE_CURRENT_SOURCE_DIR}/senders.prologue;-e;${CMAKE_CURRENT_SOURCE_DIR}/senders.epilogue;>;senders.cpp" senders.cpp)
	command_target (receivers ${CMAKE_C_COMPILER} "-E;-x;c-header;-DRECEIVER;${COMP_DEFS};${CMAKE_CURRENT_SOURCE_DIR}/syscall.tmpl;|;${STUB_GEN};-mode;receivers;-p;${CMAKE_CURRENT_SOURCE_DIR}/receivers.prologue;-e;${CMAKE_CURRENT_SOURCE_DIR}/receivers.epilogue;-n;syscheck;>;receivers.cpp" receivers.cpp)
	command_target (sys-list ${CMAKE_C_COMPILER} "-E;-x;c-header;-DSWITCH;${COMP_DEFS};${CMAKE_CURRENT_SOURCE_DIR}/syscall.tmpl;|;${STUB_GEN};-mode;listcalls;>;syscall-list" syscall-list)

	##################################################
	# finally add in the local sources, remove stale stubs if they exist
	# so we don't have duplicates in the glob from previous cmake runs.
	file( GLOB StubRmvSrcs switches.cpp senders.cpp receivers.cpp )
	condor_glob( SyscallHdrs SyscallSrcs "${StubRmvSrcs}" )

	##################################################
	# now create the glorious syscall lib from various refs.
	# behold the birth of frankenstein.
	condor_static_lib( condorsyscall "senders.cpp;switches.cpp;${SyscallHdrs};${SyscallSrcs};${STDU_OBJS};${CKPT_REFS};${UTIL_REFS};${STDIO_REF};${SAFE_OPEN_SRC}")
	if (DOES_COMPRESS_CKPT)
		# For condorzsyscall, pull in all the same objects as
		# condorsyscall except use ZCKPT_REFS in place of
		# CKPT_REFS
		src_target_ref( zckpt "^.*$" ZCKPT_REFS)

		# We need to compile condor-malloc.c with special preprocessor
		# definitions.
		src_target_ref( zckpt "malloc-condor.c" MALLOC_CONDOR_REF)
		set_property(SOURCE ${MALLOC_CONDOR_REF} APPEND PROPERTY COMPILE_DEFINITIONS "MORECORE=condor_morecore;HAVE_MMAP=0;malloc_getpagesize=8192")

		condor_static_lib( condorzsyscall "senders.cpp;switches.cpp;${SyscallHdrs};${SyscallSrcs};${STDU_OBJS};${ZCKPT_REFS};${UTIL_REFS};${IO_REFS};${STDIO_REF};${SAFE_OPEN_SRC}")
	endif()

	# local library which the daemons can link without glibc tainting.
	condor_static_lib( std_local_ref "senders.cpp;receivers.cpp;${CKPT_LOCAL_REFS};${SYSCALL_LOCAL_REFS}" )

	##################################################
	# part 2:
	# set dependencies to ensure build dep order for parallel builds
	##################################################
	add_dependencies( condorsyscall senders switches )
	if (DOES_COMPRESS_CKPT)
		add_dependencies( condorzsyscall senders switches )
	endif()
	add_dependencies( switches syscall_numbers stub_gen )
	add_dependencies( senders syscall_numbers stub_gen )
	add_dependencies( receivers syscall_numbers stub_gen )
	add_dependencies( std_local_ref senders receivers )
	add_dependencies( sys-list syscall_numbers stub_gen )

	#################################################
	# here we begin the install target section
	install ( TARGETS condorsyscall DESTINATION ${C_LIB} )
	if (DOES_COMPRESS_CKPT)
		install ( TARGETS condorzsyscall DESTINATION ${C_LIB} )
	endif()
	install ( FILES ${CMAKE_CURRENT_BINARY_DIR}/condor_rt0.o DESTINATION ${C_LIB} )
	install ( FILES ${LIBGCC} DESTINATION ${C_LIB} RENAME libcomp_libgcc.a )
	install ( FILES ${LIBGCC_EH} DESTINATION ${C_LIB} RENAME libcomp_libgcc_eh.a )
	install ( FILES ${STDCPLUS} DESTINATION ${C_LIB} RENAME libcomp_libstdc++.a )

	if (UNIX)
		set_source_files_properties(receivers.cpp senders.cpp PROPERTIES COMPILE_FLAGS "-Wno-uninitialized -Wno-unused-variable -Wno-unused-parameter")
		set_source_files_properties(getcwd.c remote_startup.c PROPERTIES COMPILE_FLAGS "-Wno-nonnull -Wno-deprecated-declarations")
	endif(UNIX)
endif( STD_UNIVERSE )
