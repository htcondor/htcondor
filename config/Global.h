/*************
**
** Global macros
**
*************/
#define YES	1
#define NO	0

#define __CONCAT(a,b) a ## b
#define concat(a,b) __CONCAT(a,b)

#define __CONCAT3(a,b,c) a ## b ## c
#define concat3(a,b,c) __CONCAT3(a,b,c)

#define __CONCAT4(a,b,c,d) a ## b ## c ## d
#define concat4(a,b,c,d) __CONCAT4(a,b,c,d)

#define __CONCAT5(a,b,c,d,e) a ## b ## c ## d ## e
#define concat5(a,b,c,d,e) __CONCAT5(a,b,c,d,e)

#define __CONCAT6(a,b,c,d,e,f) a ## b ## c ## d ## e ## f
#define concat6(a,b,c,d,e,f) __CONCAT6(a,b,c,d,e,f)
