#ifndef _UID_H
#define _UID_H

#if defined(__cplusplus)
extern "C" {
#endif

int set_condor_euid();
int set_root_euid();

#if defined(__cplusplus)
}
#endif

#endif /* _UID_H */
