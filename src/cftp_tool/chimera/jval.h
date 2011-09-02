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
#ifndef	_JVAL_H_
#define	_JVAL_H_

/* The Jval -- a type that can hold any 8-byte type */

typedef union
{
    int i;
    long l;
    float f;
    double d;
    void *v;
    char *s;
    char c;
    unsigned char uc;
    short sh;
    unsigned short ush;
    unsigned int ui;
    int iarray[2];
    float farray[2];
    char carray[8];
    unsigned char ucarray[8];
} Jval;

extern Jval new_jval_i (int);
extern Jval new_jval_l (long);
extern Jval new_jval_f (float);
extern Jval new_jval_d (double);
extern Jval new_jval_v (void* );
extern Jval new_jval_s (char *);
extern Jval new_jval_c (char);
extern Jval new_jval_uc (unsigned char);
extern Jval new_jval_sh (short);
extern Jval new_jval_ush (unsigned short);
extern Jval new_jval_ui (unsigned int);
extern Jval new_jval_iarray (int, int);
extern Jval new_jval_farray (float, float);
extern Jval new_jval_carray_nt (char *);	/* Carray is null terminated */
extern Jval new_jval_carray_nnt (char *);	/* Carray is not null terminated */
       /* For ucarray -- use carray, because it uses memcpy */

extern Jval JNULL;

extern int jval_i (Jval);
extern long jval_l (Jval);
extern float jval_f (Jval);
extern double jval_d (Jval);
extern void *jval_v (Jval);
extern char *jval_s (Jval);
extern char jval_c (Jval);
extern unsigned char jval_uc (Jval);
extern short jval_sh (Jval);
extern unsigned short jval_ush (Jval);
extern unsigned int jval_ui (Jval);
extern int *jval_iarray (Jval);
extern float *jval_farray (Jval);
extern char *jval_carray (Jval);

#endif
