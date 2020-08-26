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

#ifndef _VM_MACHINE_H
#define _VM_MACHINE_H

#include "condor_common.h"

class VMManager;
class VMMachine {
public:
	VMMachine(VMManager *, const char *);
	virtual ~VMMachine();

	void sendEventToVM(int, void*);
	bool match(const char *);
	void print(void);
	void updateTimeStamp(void);
	int getTimeStamp(void) const;
	char *getVMSinful(void);

private:
	VMManager* m_vmmanager;
	char *m_addr;  // address of this virtual machine
	int m_last_time; // last timestamp when a register command is received

};

#endif /* _VM_MACHINE_H */
