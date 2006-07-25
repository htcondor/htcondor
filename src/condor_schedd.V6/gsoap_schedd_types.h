/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2006, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#import "gsoap_daemon_core_types.h"

typedef xsd__string condor__Requirement;

enum condor__HashType {
  NOHASH,
  MD5HASH
};

enum condor__UniverseType {
  STANDARD = 1,
  PVMUNIVERSE = 4,
  VANILLA = 5,
  SCHEDULER = 7,
  MPI = 8,
  GLOBUS = 9,
  JAVA = 10
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
