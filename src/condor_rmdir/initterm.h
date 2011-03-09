typedef void (__cdecl *_PVFV)(void);
extern _PVFV __xc_a[], __xc_z[];    /* C++ initializers */

void __cdecl _initterm ( _PVFV * pfbegin, _PVFV * pfend );
void __cdecl _atexit_init(void);
void __cdecl _DoExit( void );
