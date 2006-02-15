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
/*
 * Abstract data type for StatTable, which is a data structure which holds 
 * statistical information about a pool of machines.
 */
#include "condor_common.h"
#include "condor_exprtype.h"
#include "condor_ast.h"
#include "condor_buildtable.h"

#define NextSpace(str) while(*str != ' ') str++;

int VarTypeTable::FindType(char *var)
{
  VTableEntry *tmpEntry;

  for(tmpEntry = table; tmpEntry; tmpEntry = tmpEntry->next)
    if(!strcmp(var, tmpEntry->MyName()))
      return tmpEntry->MyType();
  
  return VTAB_FIXED; 
}

void VarTypeTable::AddEntry(char *var, int type)
{
  VTableEntry *newEntry = new VTableEntry(var, type);

  newEntry->next = table;
  table = newEntry;
}

void VarTypeTable::PrintTable(FILE *fd)
{
  VTableEntry *tmpEntry = table;
  char *tmp;

  for( ; tmpEntry; tmpEntry = tmpEntry->next) {
    tmp = tmpEntry->MyName();
    if(tmpEntry->MyType() == RANGE)
      fprintf(fd, "%s RANGE\n", tmpEntry->MyName());
    else
      fprintf(fd, "%s FIXED\n", tmpEntry->MyName());
  }
}

void BuildVarTypeTable(FILE *f, VarTypeTable *table)
{
  char name[10000];
  char type[10000];

  while(fscanf(f, "%s%s", name, type) != EOF)
    if(!strcmp(type, "RANGE") || !strcmp(type, "range"))
      table->AddEntry(name, RANGE);
    else
      table->AddEntry(name, VTAB_FIXED);
}
