/*
Libraries for fields, doubly-linked lists and red-black trees.
Copyright (C) 2001 James S. Plank

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

---------------------------------------------------------------------------
Please see http://www.cs.utk.edu/~plank/plank/classes/cs360/360/notes/Libfdr/
for instruction on how to use this library.

Jim Plank
plank@cs.utk.edu
http://www.cs.utk.edu/~plank

Associate Professor
Department of Computer Science
University of Tennessee
203 Claxton Complex
1122 Volunteer Blvd.
Knoxville, TN 37996-3450

     865-974-4397
Fax: 865-974-4404
*/
#ifndef _DLLIST_H_
#define _DLLIST_H_

#include "jval.h"

typedef struct dllist
{
    struct dllist *flink;
    struct dllist *blink;
    Jval val;
} *Dllist;

extern Dllist new_dllist ();
extern void free_dllist (Dllist);

extern void dll_append (Dllist, Jval);
extern void dll_prepend (Dllist, Jval);
extern void dll_insert_b (Dllist, Jval);
extern void dll_insert_a (Dllist, Jval);

extern void dll_delete_node (Dllist);
extern int dll_empty (Dllist);

extern Jval dll_val (Dllist);

#define dll_first(d) ((d)->flink)
#define dll_next(d) ((d)->flink)
#define dll_last(d) ((d)->blink)
#define dll_prev(d) ((d)->blink)
#define dll_nil(d) (d)

#define dll_traverse(ptr, list) \
  for (ptr = list->flink; ptr != list; ptr = ptr->flink)
#define dll_rtraverse(ptr, list) \
  for (ptr = list->blink; ptr != list; ptr = ptr->blink)

#endif
