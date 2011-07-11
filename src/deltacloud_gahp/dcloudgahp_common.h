#ifndef DCLOUDGAHP_COMMON_H
#define DCLOUDGAHP_COMMON_H

#include "string"

#define TRUE 1
#define FALSE 0

#define dcloudprintf(fmt, ...) dcloudprintf_internal(__FUNCTION__, fmt, ## __VA_ARGS__)
void dcloudprintf_internal(const char *function, const char *fmt, ...);

#define STRCASEEQ(a,b) (strcasecmp(a,b) == 0)
#define STRCASENEQ(a, b) (strcasecmp(a, b) != 0)

#define NULLSTRING "NULL"

extern FILE *logfp;

/* Users of GahpArgs should not manipulate the class data members directly.
 * Changing the object should only be done via the member functions.
 * If argc is 0, then the value of argv is undefined. If argc > 0, then
 * argv[0] through argv[argc-1] point to valid null-terminated strings. If
 * a NULL is passed to add_arg(), it will be ignored. argv may be resized
 * or freed by add_arg() and reset(), so users should not copy the pointer
 * and expect it to be valid after these calls.
 */
class Gahp_Args {
 public:
    Gahp_Args();
    ~Gahp_Args();
    void reset();
    void add_arg(char *arg);
    char **argv;
    int argc;
    int argv_size;
};

#endif
