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

#import "gsoap_daemon_core_types.h"

typedef xsd__string condor__Requirement;

enum condor__HashType {
  NOHASH,
  MD5HASH
};

enum condor__UniverseType {
  STANDARD = 1,
  VANILLA = 5,
  SCHEDULER = 7,
  MPI = 8,
  GRID = 9,
  JAVA = 10,
  PARALLEL = 11,
  LOCALUNIVESE = 12,
  VM = 13
};

struct condor__Transaction
{
  xsd__int id 0:1;
  xsd__int duration 0:1; // change to xsd:duration ?
};

struct condor__Requirements
{
  condor__Requirement *__ptr;
  int __size;
};

struct condor__RequirementsAndStatus
{
  struct condor__Status status 1:1;
  struct condor__Requirements requirements 0:1;
};


struct condor__TransactionAndStatus
{
  struct condor__Status status 1:1;
  struct condor__Transaction transaction 0:1;
};

struct condor__IntAndStatus
{
  struct condor__Status status 1:1;
  xsd__int integer 0:1;
};

struct condor__Base64DataAndStatus
{
  struct condor__Status status 1:1;
  struct xsd__base64Binary data 0:1;
};

struct condor__FileInfo
{
  xsd__string name 1:1;
  xsd__long size 1:1;
};

struct condor__FileInfoArray
{
  struct condor__FileInfo *__ptr;
  int __size;
};

struct condor__FileInfoArrayAndStatus
{
  struct condor__Status status 1:1;
  struct condor__FileInfoArray info 0:1;
};
