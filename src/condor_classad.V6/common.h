#ifndef __COMMON_H__
#define __COMMON_H__

enum MmMode
{
	MEM_DUP,
	MEM_ADOPT_PRESERVE,
	MEM_ADOPT_DISPOSE,
	MEM_DISPOSE,
};

enum CopyMode 
{ 
	EXPR_DEEP_COPY, 
	EXPR_REF_COUNT 
};

enum NumberFactor 
{ 
    NO_FACTOR   = 1,
    K_FACTOR    = 1024,             // kilo
    M_FACTOR    = 1024*1024,        // mega
    G_FACTOR    = 1024*1024*1024    // giga
};

#endif//__COMMON_H__
