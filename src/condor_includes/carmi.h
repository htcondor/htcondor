#if !defined(_CARMI_H)
#define _CARMI_H

#if defined(__cplusplus)
extern "C" {
#endif

int carmi_respawn(int, int);
/* Response: {int old_tid, int new_tid} */

#if defined(__cplusplus)
}
#endif
#endif
