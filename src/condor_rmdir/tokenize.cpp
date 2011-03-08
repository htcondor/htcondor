//==========================================
// LIBCTINY - TJ's version
// March 2011.
//==========================================
#define UNICODE
#include <windows.h>
#include "tokenize.h"

#undef MoveMemory
#undef RtlMoveMemory
extern "C" __declspec(dllimport) void* __stdcall RtlMoveMemory(void *, const void *, int);

#ifdef UNICODE
#define _strcopy4 _strcopy4W
#else
#define _strcopy4 _strcopy4A
#endif
extern int _strcopy4A(char *, int, const char*, int);
extern int _strcopy4W(wchar_t *, int, const wchar_t*, int);

template <class T> __inline bool CharIsSpace(T ch) {
   return (' ' == ch) || (ch >= 0x09 && ch <= 0x0D);
}

template <class T> T _SkipWhiteSpace(T psz) {
   while(CharIsSpace(psz[0])) {
      psz = CharNext(psz);
   }
   return psz;
}

#if 0
LPCTSTR * _storeArgvT(LPCTSTR * ppszArgs) {
   LPCTSTR * ppszOld = _ppszArgvT;
   _ppszArgvT = ppszArgs;
   return ppszOld;
}
#else
#endif

// returns pointer to array of pointers to null terminated strings
// (like argc, argv from main), quoted arguments have quotes stripped.
// this function is destructive in that it will write null's and remove
// quotes from the string.
//
//
LPCTSTR * _TokenizeString (
   LPCTSTR pszInput,
   LPUINT  pcArgs)
{
   *pcArgs = 0;
   if ( ! pszInput)
      return NULL;

   UINT cch = lstrlen (pszInput);
   if ( ! cch)
      return NULL;

   UINT cbAlloc = ((cch + 1) * sizeof(char*) +  cch * sizeof(TCHAR) * 2);
   LPCTSTR * pArgv = (LPCTSTR *)HeapAlloc(GetProcessHeap(), 0, cbAlloc);
   if ( ! pArgv)
      return NULL;


   LPTSTR pszToken = (LPTSTR)(&pArgv[cch + 1]);
   UINT   cArgs    = 0;

   //lstrcpy(pszToken, pszInput);
   _strcopy4 (pszToken, cch+1, pszInput, cch + 1);

   pszToken = _SkipWhiteSpace (pszToken);
   while (0 != pszToken[0])
      {
      LPTSTR psz = _ParseToken(pszToken);
      pArgv[cArgs++] = pszToken;
      pszToken = _SkipWhiteSpace (psz);
      }

   *pcArgs = cArgs;
   pArgv[cArgs] = NULL;
   if ( ! cArgs)
      {
      HeapFree(GetProcessHeap(), 0, pArgv);
      pArgv = NULL;
      }

   return pArgv;
}
