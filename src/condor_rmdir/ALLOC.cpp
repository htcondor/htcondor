//==========================================
// LIBCTINY - Matt Pietrek 2001
// MSDN Magazine, January 2001
//==========================================
#include <windows.h>
#include <malloc.h>

extern "C" void * __cdecl malloc(size_t size)
{
    return HeapAlloc( GetProcessHeap(), 0, size );
}

extern "C" void __cdecl free(void * p)
{
    HeapFree( GetProcessHeap(), 0, p );
}
