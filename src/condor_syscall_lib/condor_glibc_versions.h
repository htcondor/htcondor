#ifndef CONDOR_GLIBC_VERSION_DEFINES
#define CONDOR_GLIBC_VERSION_DEFINES

/* We only define CGV_MINOR for glibc 2.x where we know we have ported
   standard universe to that version. */

#define CGV_MAJOR 2

#if defined(GLIBC20)
#define CGV_MINOR 0
#endif

#if defined(GLIBC21)
#define CGV_MINOR 1
#endif

#if defined(GLIBC22)
#define CGV_MINOR 2
#endif

#if defined(GLIBC23)
#define CGV_MINOR 3
#endif

#if defined(GLIBC24)
#define CGV_MINOR 4
#endif

#if defined(GLIBC25)
#define CGV_MINOR 5
#endif

/* Do NOT include 6. */

#if defined(GLIBC27)
#define CGV_MINOR 7
#endif

/* Do NOT include 8, 9, or 10. */

#if defined(GLIBC211)
#define CGV_MINOR 11
#endif

#if defined(GLIBC212)
#define CGV_MINOR 12
#endif

#if defined(GLIBC213)
#define CGV_MINOR 13
#endif

/* Do NOT include 14, 15, or 16. */

#if defined(GLIBC217)
#define CGV_MINOR 17
#endif

/* Do NOT include 18. */

#if defined(GLIBC219)
#define CGV_MINOR 19
#endif

#endif
