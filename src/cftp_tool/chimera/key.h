/*
** $Id: key.h,v 1.16 2006/06/07 09:21:28 krishnap Exp $
**
** Matthew Allen
** description: 
*/

#ifndef _CHIMERA_KEY_H_
#define _CHIMERA_KEY_H_

#include <climits>
#include <cstdio>
#include <openssl/evp.h>
#include <cstring>

#define KEY_SIZE 160

// Changed this to 2 for base4 and 4 to have keys in base 16; Only these two are supported right now
#define BASE_B 4		/* Base representation of key digits */

#define BASE_16_KEYLENGTH 40

#define BASE_2 2
#define BASE_4 4
#define BASE_16 16

#define IS_BASE_2 (BASE_B == 0)
#define IS_BASE_4 (BASE_B == 2)
#define IS_BASE_16 (BASE_B == 4)

typedef struct
{
    unsigned long t[5];
    char keystr[KEY_SIZE / BASE_B + 1];	/* string representation of key in hex */
    short int valid;		// indicates if the keystr is most up to date with value in key
} Key;


/* global variables!! that are set in key_init function */
extern Key Key_Max;
extern Key Key_Half;


/* key_makehash: hashed, s
** assign sha1 hash of the string #s# to #hashed# */

void key_makehash (void *logs, Key * hashed, char *s);


/* key_make_hash */
void key_make_hash (Key * hashed, char *s, size_t size);

/* key_init: 
** initializes Key_Max and Key_Half */

void key_init ();

/* key_distance:k1,k2
** calculate the distance between k1 and k2 in the keyspace and assign that to #diff# */

void key_distance (void *logs, Key * diff, const Key * const k1,
		   const Key * const k2);


/* key_between: test, left, right
** check to see if the value in #test# falls in the range from #left# clockwise
** around the ring to #right#. */

int key_between (void *logs, const Key * const test, const Key * const left,
		 const Key * const right);


/* key_midpoint: mid, key
** calculates the midpoint of the namespace from the #key#  */

void key_midpoint (void *logs, Key * mid, Key key);


/* key_index: mykey, key
** returns the lenght of the longest prefix match between #mykey# and #k# */

int key_index (void *logs, Key mykey, Key k);

void key_print (Key k);

void key_to_str (Key * k);
void str_to_key (const char *str, Key * k);
char *get_key_string (Key * k);	// always use this function to get the string representation of a key

/* key_assign: k1, k2
** copies value of #k2# to #k1# */

void key_assign (Key * k1, Key k2);

/* key_assign_ui: k1, ul
** copies #ul# to the least significant 32 bits of #k# */

void key_assign_ui (Key * k, unsigned long ul);

/* key_equal:k1, k2 
** return 1 if #k1#==#k2# 0 otherwise*/

int key_equal (Key k1, Key k2);

/* key_equal_ui:k1, ul
** return 1 if the least significat 32 bits of #k1#==#ul# 0 otherwise */

int key_equal_ui (Key k, unsigned long ul);

/*key_comp: k1, k2
** returns >0 if k1>k2, <0 if k1<k2, and 0 if k1==k2 */

int key_comp (const Key * const k1, const Key * const k2);

#endif /* _CHIMERA_KEY_H_ */
