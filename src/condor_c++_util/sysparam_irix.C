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
