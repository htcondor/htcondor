
#ifdef UNICODE
#define _TokenizeString _TokenizeStringW
#define _ParseToken _ParseTokenW
#else
#define _TokenizeString _TokenizeStringA
#define _ParseToken _ParseTokenA
#endif

// tokenize.cpp
LPCWSTR * _TokenizeStringW (LPCWSTR pszInput, LPUINT pcArgs);
LPCSTR  * _TokenizeStringA (LPCSTR  pszInput, LPUINT pcArgs);

// from parsetok.cpp
LPWSTR _ParseTokenW (LPWSTR pszToken);
LPSTR _ParseTokenA (LPSTR pszToken);
