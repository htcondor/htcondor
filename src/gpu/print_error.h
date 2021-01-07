#ifndef   _PRINT_ERROR_H
#define   _PRINT_ERROR_H

extern int g_verbose;
extern int g_diagnostic;
extern int g_config_syntax;
extern int g_config_fail_on_error;

#define MODE_ERROR          1
#define MODE_VERBOSE        2
#define MODE_DIAGNOSTIC_MSG 4 // diagnostic to stdout
#define MODE_DIAGNOSTIC_ERR 5 // diagnostic to stderr

int print_error(int mode, const char * fmt, ...);

#endif /* _PRINT_ERROR_H */
