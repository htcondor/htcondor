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
/*
#include <stdio.h>
#include <nlist.h>
#include <sys/types.h>
#include <sys/file.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <string.h>
#include <malloc.h>
*/

#include "condor_debug.h"
#include "condor_sysparam.h"

Irix:: Irix() {}
int  Irix::readSysParam(const SysParamName, char*& buffer, int& size, SysType& type){}
void Irix::disSysParam(const SysParamName sys, const char* buffer, const int size, const SysType type){}

// dummy stubs
Osf1:: Osf1() {}
int  Osf1::readSysParam(const SysParamName, char*& buffer, int& size, SysType& type){}
void Osf1::disSysParam(const SysParamName sys, const char* buffer, const int size, const SysType type){}

Ultrix:: Ultrix() {}
int  Ultrix::readSysParam(const SysParamName, char*& buffer, int& size, SysType& type){}
void Ultrix::disSysParam(const SysParamName sys, const char* buffer, const int size, const SysType type){}

Hpux:: Hpux() {}
int  Hpux::readSysParam(const SysParamName, char*& buffer, int& size, SysType& type){}
void Hpux::disSysParam(const SysParamName sys, const char* buffer, const int size, const SysType type){}

Linux:: Linux() {}
int  Linux::readSysParam(const SysParamName, char*& buffer, int& size, SysType& type){}
void Linux::disSysParam(const SysParamName sys, const char* buffer, const int size, const SysType type){}

Aix:: Aix() {}
int  Aix::readSysParam(const SysParamName, char*& buffer, int& size, SysType& type){}
void Aix::disSysParam(const SysParamName sys, const char* buffer, const int size, const SysType type){}
