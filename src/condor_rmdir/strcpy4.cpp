//==========================================
// LIBCTINY - TJ's version
// March 2011.
//==========================================
#define UNICODE
#include <windows.h>

template <class T>
int _strcopy4(
    T*   pszDest,
    int  cchDest,
    const T* pszSrc,
    int  cchToCopy)
{
    HRESULT hr = S_OK;
    int cchNewDestLength = 0;

    while (cchDest && cchToCopy && *pszSrc) {
        *pszDest++ = *pszSrc++;
        cchDest--;
        cchToCopy--;
        cchNewDestLength++;
    }

    // we havn't copied the terminating null yet, so if
    // cchDest is 0, we are going to truncate
    if (0 == cchDest) {
        pszDest--;
        cchNewDestLength--;
        return -1;
    }

    *pszDest = 0;
    return cchNewDestLength;
}

int _strcopy4A(char * pszDest, int cchDest, const char * pszSrc, int cchToCopy)
{
  return _strcopy4(pszDest, cchDest, pszSrc, cchToCopy);
}

#ifdef UNICODE
int _strcopy4W(WCHAR * pszDest, int cchDest, const WCHAR * pszSrc, int cchToCopy)
{
  return _strcopy4(pszDest, cchDest, pszSrc, cchToCopy);
}
#endif
