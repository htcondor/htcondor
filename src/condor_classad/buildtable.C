/*
 * Abstract data type for StatTable, which is a data structure which holds 
 * statistical information about a pool of machines.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "exprtype.h"
#include "ast.h"
#include "classad.h"
#include "parser.h"
#include "buildtable.h"

#define NextSpace(str) while(*str != ' ') str++;

int VarTypeTable::FindType(char *var)
{
  VTableEntry *tmpEntry;

  for(tmpEntry = table; tmpEntry; tmpEntry = tmpEntry->next)
    if(!strcmp(var, tmpEntry->MyName()))
      return tmpEntry->MyType();
  
  return FIXED; 
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
      table->AddEntry(name, FIXED);
}
