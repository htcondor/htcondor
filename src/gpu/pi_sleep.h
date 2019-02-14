#ifndef   _PI_SLEEP_H
#define   _PI_SLEEP_H

#ifndef WIN32

#include <unistd.h>

#else

inline unsigned int
sleep( unsigned int seconds ) { Sleep( seconds * 1000 ); return 0; }
inline int
usleep( unsigned int usec ) { Sleep( usec / 1000 ); return 0; }

#endif /* WIN32 */

#endif /* _PI_SLEEP_H */
