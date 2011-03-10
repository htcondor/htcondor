//==========================================
// LIBCTINY - Matt Pietrek 2001
// MSDN Magazine, January 2001
//==========================================
#include <windows.h>
#include <malloc.h>
#include "initterm.h"

#pragma data_seg(".CRT$XCA")
_PVFV __xc_a[] = { NULL };


#pragma data_seg(".CRT$XCZ")
_PVFV __xc_z[] = { NULL };

#pragma data_seg()  /* reset */

#pragma comment(linker, "/merge:.CRT=.rdata")

typedef void (__cdecl *_PVFV)(void);

void __cdecl _initterm (
        _PVFV * pfbegin,
        _PVFV * pfend
        )
{
    /*
     * walk the table of function pointers from the bottom up, until
     * the end is encountered.  Do not skip the first entry.  The initial
     * value of pfbegin points to the first valid entry.  Do not try to
     * execute what pfend points to.  Only entries before pfend are valid.
     */
    while ( pfbegin < pfend )
    {
        // if current table entry is non-NULL, call thru it.
        if ( *pfbegin != NULL )
            (**pfbegin)();
        ++pfbegin;
    }
}

static _PVFV * pf_atexitlist = 0;
static unsigned max_atexitlist_entries = 0;
static unsigned cur_atexitlist_entries = 0;

void __cdecl _atexit_init(void)
{
    max_atexitlist_entries = 32;
    pf_atexitlist = (_PVFV *)calloc( max_atexitlist_entries,
                                     sizeof(_PVFV*) );
}

int __cdecl atexit (_PVFV func )
{
    if ( cur_atexitlist_entries < max_atexitlist_entries )
    {
        pf_atexitlist[cur_atexitlist_entries++] = func; 
        return 0;
    }

    return -1;
}

void __cdecl _DoExit( void )
{
    if ( cur_atexitlist_entries )
    {
        _initterm(  pf_atexitlist,
                    // Use ptr math to find the end of the array
                    pf_atexitlist + cur_atexitlist_entries );
    }
}
