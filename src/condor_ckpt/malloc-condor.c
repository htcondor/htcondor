
/*
This file sets up some definitions in order
to create a private malloc implementation suitable
for use by the checkpointing library to make 
a private heap.  This is *not* the malloc called
by ordinary user code.
*/

#define MALLOC_SYMBOL(x) condor_##x

#include "malloc.c"
