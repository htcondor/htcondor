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

template <class T> __inline UINT _strallocbytes(T psz) {
   return (UINT)(psz ? lstrlen(psz)+1 : 0) * sizeof(psz[0]);
}

template <class T> __inline bool CharIsSpace(T ch) {
   return (' ' == ch) || (ch >= 0x09 && ch <= 0x0D);
}

template <class T> T _SkipWhiteSpace(T psz) {
   while(CharIsSpace(psz[0])) {
      psz = CharNext(psz);
   }
   return psz;
}

template <class T> T _NextWhiteSpaceOrQuote(T psz) {
   while ((0 != psz[0]) && ! ('"' == psz[0]) && ! CharIsSpace(psz[0])) {
      psz = CharNext(psz);
   }
   return psz;
}


// destructively parse a string into tokens, inserting a NULL at the token
// end and removing quotes
//
LPTSTR _ParseToken (
   LPTSTR pszToken)
{
   LPTSTR psz = pszToken;
   BOOL   fQuote = FALSE;

   // scan for next token
   //
   for (;;)
      {
      psz = _NextWhiteSpaceOrQuote (psz);
      if (0 == psz[0])
         break;

      // if we stopped on a quote
      //
      if ('"' == psz[0])
         {
         // remove the quote
         //
         LPTSTR pszT = CharNext (psz);
         RtlMoveMemory(psz, pszT, _strallocbytes(pszT));

         if (fQuote) // we are looking for a close quote
            {
            // if this is two quotes in a row, it is not a close quote, but
            // an escape, we already threw out one quote, now just skip over
            // the other and keep looking for the close.
            //
            if ('"' == psz[0])
               {
               psz = CharNext (psz);
               }
            else
               {
               fQuote = FALSE;
               }
            }
         else
            {
            fQuote = TRUE;
            }
         }
      else if (fQuote)
         {
         psz = _SkipWhiteSpace(psz);
         }
      else
         {
         // found whitespace, and we are not inside a quote, I guess we found
         // the token end...
         //
         break;
         }
      }

   // if the end of the token is not a NULL, then set it to
   // null and advance psz to point past the NULL we just wrote
   //
   if (0 != psz[0])
      {
      LPTSTR pszT = CharNext (psz);
      psz[0] = 0;
      psz = pszT;
      }

   return psz; // return pointer to next token or terminating null
}
