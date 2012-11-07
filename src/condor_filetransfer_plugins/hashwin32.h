/***************************************************************
*
* Copyright (C) 1990-2009, Condor Team, Computer Sciences Department,
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

#ifndef _HASHWIN32_
#define _HASHWIN32_
typedef struct _ENTRY
{
	char *key;
	void *data;
} ENTRY;

typedef struct _BUCK
{
	struct _BUCK *next;
	ENTRY entry;
} BUCK;

static BUCK **tree;
static size_t HASHSIZE;
static size_t count;

enum ACTION
{
	FIND,
	ENTER,
};

enum VISIT
{
	preorder,
	postorder,
	endorder,
	leaf
};

unsigned int hash(const char *s);

int hcreate(size_t);
void hdestroy();
ENTRY *hsearch(ENTRY, enum ACTION);
#endif
