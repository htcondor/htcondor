/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-1998 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
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
