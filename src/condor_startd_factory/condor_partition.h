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

#ifndef CONDOR_PARTITION_H
#define CONDOR_PARTITION_H

class MyString;
class ClassAd;

enum PState
{
	PSTATE_ERROR = 0,

	NOT_GENERATED,	// partition has yet to be made

	GENERATED,		// partition is already generated 

	BOOTED,			// partition is generated and booted

	ASSIGNED,		// partition is generated, booted, and in the process of
					// having a startd back it.

	BACKED,			// part is generated, booted, and a known startd presents it

	PSTATE_END
};

enum PKind
{
	PKIND_ERROR = 0,
	SMP,				// 1 process per node
	DUAL,				// 2 processes per node
	VN,					// 4 procosses per node
	PKIND_END
};

/* This object represents an already generted partition and whether or
	not it is activated. */
class Partition
{
	public:
		Partition();
		~Partition();

		// The class owns the memory of the classad and destructs it when
		// this class gets deleted
		void attach(ClassAd *ad);

		// transfer ownership of the internal classad memory back to the caller.
		ClassAd* detach(void);

		// what is the canonical name the BG thinks this partition is called?
		void set_name(MyString &name);
		MyString get_name(void);

		// Which, if any, startd backs this partition?
		void set_backer(MyString &name);
		MyString get_backer(void);

		void set_size(size_t size);
		size_t get_size(void);

		void set_pstate(PState pstate);
		void set_pstate(MyString pstate);
		PState get_pstate(void);

		void set_pkind(MyString pkind);
		void set_pkind(PKind pkind);
		PKind get_pkind(void);

		// dprintf out the structure.
		void dump(int flags);

		bool generate(char *script, int size); 
		void boot(char *script, PKind pkind);
		void shutdown(char *script);

		void back(char *script);

	private:
		bool	m_initialized;
		ClassAd	*m_data;
};

MyString pkind_xlate(PKind pkind);
PKind pkind_xlate(MyString pkind);

MyString pstate_xlate(PState pstate);
PState pstate_xlate(MyString pstate);


#endif


