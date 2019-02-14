#ifndef   _PI_SLEEP_H
#define   _PI_SLEEP_H

#ifndef WIN32

#include <unistd.h>

#else

inline unsigned int
sleep( unsigned int seconds ) { Sleep( seconds * 1000 ); }
inline int
usleep( useconds_t usec ) { Sleep( usec / 1000 ); }

#endif /* WIN32 */

#endif /* _PI_SLEEP_H */
