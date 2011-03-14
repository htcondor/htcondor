//==========================================
// LIBCTINY - Matt Pietrek 2001
// MSDN Magazine, January 2001
//==========================================
#include <windows.h>
#include <malloc.h>

extern "C" void * __cdecl _nh_malloc(size_t size, int nhFlag)
{
    return HeapAlloc( GetProcessHeap(), 0, size );
}

extern "C" size_t __cdecl _msize(void * p)
{
    return HeapSize( GetProcessHeap(), 0, p );
}

