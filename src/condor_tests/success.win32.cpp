/***************************************************************
*
* Copyright (C) 2026, John M. Knoeller 
* Condor Team, Computer Sciences Department
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

// disable code checks that emit external refs to c-runtime functions
#pragma strict_gs_check(off)
#pragma runtime_checks("scu", off)

// Force the linker to include KERNEL32.LIB and to NOT include the c-runtime
#pragma comment(linker, "/entry:begin")
#pragma comment(linker, "/nodefaultlib:msvcrt")
#pragma comment(linker, "/defaultlib:kernel32.lib")
#pragma comment(linker, "/subsystem:console")

extern "C" void __stdcall ExitProcess(unsigned int uExitCode);

extern "C" void __cdecl begin( void )
{
    ExitProcess(0);
}
