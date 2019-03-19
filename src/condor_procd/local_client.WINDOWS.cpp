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
#include "condor_debug.h"
#include "local_client.h"

double pfc_lc_rt_start_connection = 0;
double pfc_lc_rt_open_pipe = 0;
double pfc_lc_rt_wait_pipe = 0;
double pfc_lc_rt_write_pipe = 0;
double pfc_lc_rt_end_connection = 0;
double pfc_lc_rt_read_data = 0;

// can't use _condor_auto_accum_runtime in this file because it pulls in
// other things as well. 
class _lc_auto_save_runtime
{
public:
#ifdef PROFILE_PROCAPI
   double & runtime;
   double   begin;
   double   begin_step;
   _lc_auto_save_runtime(double & store) : runtime(store) { this->begin_step = this->begin = current_time(); }
   ~_lc_auto_save_runtime() { runtime = elapsed(); }
   double   elapsed() { return current_time() - begin; }
   double   step() { 
      double now = current_time(); 
      double ret = begin_step - now; 
      begin_step = now; 
      return ret; 
   }
#else
   _lc_auto_save_runtime(double & store) {}
   double step() { return 0; }
#endif
   double current_time() const { return _condor_debug_get_time_double(); }
};


LocalClient::LocalClient() :
	m_initialized(false),
	m_pipe_addr(NULL),
	m_pipe(INVALID_HANDLE_VALUE)
{
}

bool
LocalClient::initialize(const char* pipe_addr)
{
	assert(!m_initialized);

	m_pipe_addr = strdup(pipe_addr);
	assert(m_pipe_addr != NULL);

	m_initialized = true;
	return true;
}

LocalClient::~LocalClient()
{
	if (!m_initialized) {
		return;
	}

	if (m_pipe != INVALID_HANDLE_VALUE) {
		CloseHandle(m_pipe);
	}

	free(m_pipe_addr);
}

bool
LocalClient::start_connection(void* buffer, int len)
{
	assert(m_initialized);
    _lc_auto_save_runtime rt(pfc_lc_rt_start_connection);

	assert(m_pipe == INVALID_HANDLE_VALUE);
	while (true) {
		m_pipe = CreateFile(m_pipe_addr,                   // path to pipe
		                    GENERIC_READ | GENERIC_WRITE,  // read-write access
		                    0,                             // no sharing
	                        NULL,                          // default security
	                        OPEN_EXISTING,                 // fail if not there
	                        0,                             // default attributes
		                    NULL);                         // no template file
        pfc_lc_rt_open_pipe = rt.step();
		if (m_pipe != INVALID_HANDLE_VALUE) {
			break;
		}
		DWORD error = GetLastError();
		if (error != ERROR_PIPE_BUSY) {
			if (error == ERROR_FILE_NOT_FOUND) {
				dprintf(D_ALWAYS | D_BACKTRACE,
					"Could not open pipe handle '%s'. error %u : The system cannot find the file specified.\n",
					m_pipe_addr, error);
			} else {
				dprintf(D_ALWAYS, "CreateFile error: %u\n", error);
			}
			return false;
		}
		if (WaitNamedPipe(m_pipe_addr, NMPWAIT_WAIT_FOREVER) == FALSE) {
			dprintf(D_ALWAYS, "WaitNamedPipe error: %u\n", GetLastError());
            pfc_lc_rt_wait_pipe = rt.step();
			return false;
		}
        pfc_lc_rt_wait_pipe = rt.step();
	}

	DWORD bytes;
	if (WriteFile(m_pipe, buffer, len, &bytes, NULL) == FALSE) {
		dprintf(D_ALWAYS, "WriteFile error: %u\n", GetLastError());
		CloseHandle(m_pipe);
		m_pipe = INVALID_HANDLE_VALUE;
        pfc_lc_rt_write_pipe = rt.step();
		return false;
	}
	assert(bytes == len);
    pfc_lc_rt_write_pipe = rt.step();

	return true;
}

void
LocalClient::end_connection()
{
	assert(m_initialized);
    _lc_auto_save_runtime rt(pfc_lc_rt_end_connection);

	assert(m_pipe != INVALID_HANDLE_VALUE);
	CloseHandle(m_pipe);
	m_pipe = INVALID_HANDLE_VALUE;
}

bool
LocalClient::read_data(void* buffer, int len)
{
	assert(m_initialized);
    _lc_auto_save_runtime rt(pfc_lc_rt_read_data);

	DWORD bytes;
	if (ReadFile(m_pipe, buffer, len, &bytes, NULL) == FALSE) {
		dprintf(D_ALWAYS, "ReadFile error: %u\n", GetLastError());
		return false;
	}
	assert(bytes == len);

	return true;
}
