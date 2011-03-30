/***************************************************************
 *
 * Copyright (C) 2010, John M. Knoeller
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/
#undef UNICODE
#include <windows.h>
#include "tokenize.h"

template <class T> __inline bool CharIsSpace(T ch) {
   return (' ' == ch) || (ch >= 0x09 && ch <= 0x0D);
}

template <class T> T _SkipWhiteSpace(T psz) {
   while(CharIsSpace(psz[0])) {
      psz = CharNext(psz);
   }
   return psz;
}

template <class T> __inline UINT _strallocbytes(T psz) {
   return (UINT)(psz ? lstrlen(psz)+1 : 0) * sizeof(psz[0]);
}

template <class T> T _NextWhiteSpaceOrQuote(T psz) {
   while ((0 != psz[0]) && ! ('"' == psz[0]) && ! CharIsSpace(psz[0])) {
      psz = CharNext(psz);
   }
   return psz;
}

// destructively parse a string into tokens, inserting a \0 at the token
// end and removing quotes. 
//
// This function will scan ahead to the next token, replaceing the token
// separator with \0 and returning a pointer to the next character after it.
// if a \0 is encountered while scanning, then this function returns a
// pointer to the terminating \0. 
// if an open quote (") is encountered, then it will ignore whitespace while
// scanning until it encounters a close quote (") or terminating null \0. 
// both open an close quotes are removed from the string. 
// 
//
LPTSTR ParseToken (
   LPTSTR pszToken)
{
   LPTSTR psz = pszToken;
   BOOL   fQuote = FALSE;

   // scan for next token
   //
   for (;;)
      {
      psz = _NextWhiteSpaceOrQuote (psz);
      if ( ! psz[0])
         break;

      // if we stopped on a quote
      //
      if ('"' == psz[0])
         {
         // remove the quote
         //
         LPTSTR pszT = CharNext (psz);
         MoveMemory(psz, pszT, _strallocbytes(pszT));

         if (fQuote) // we are looking for a close quote
            {
            // if this is two quotes in a row, it is not a close quote, but
            // an escape, we already threw out one quote, now just skip over
            // the other and keep looking for the close.
            //
            if ('"' == psz[0])
               psz = CharNext (psz);
            else
               fQuote = FALSE;
            }
         else
            fQuote = TRUE;
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

   // if the end of the token is not a \0, then set it to
   // \0 and advance psz to point past it.
   //
   if ( ! psz[0])
      {
      LPTSTR pszT = CharNext (psz);
      psz[0] = 0;
      psz = pszT;
      }

   return psz; // return pointer to next token or terminating null
}

// returns pointer to array of pointers to null terminated strings
// (like argc, argv from main), quoted arguments have quotes stripped.
// this function is destructive in that it will write null's and remove
// quotes from the string.
//
// the caller must use HeapFree to free the returned pointer.
//
LPCTSTR * TokenizeString (
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

   CopyMemory(pszToken, pszInput, (cch+1) * sizeof(TCHAR));

   pszToken = _SkipWhiteSpace (pszToken);
   while (0 != pszToken[0])
      {
      LPTSTR psz = ParseToken(pszToken);
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
