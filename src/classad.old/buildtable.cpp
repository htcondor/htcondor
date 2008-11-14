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
