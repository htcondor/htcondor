#include <stdio.h>
#include <stdarg.h>

#include "print_error.h"

int g_verbose = 0;
int g_diagnostic = 0;
int g_config_syntax = 0;
int g_config_fail_on_error = 0;

int print_error(int mode, const char * fmt, ...) {
	char temp[4096];
	char * ptmp = temp;
	int max_temp = (int)(sizeof(temp)/sizeof(temp[0]));

	FILE * out = g_config_syntax ? stdout : stderr;
	bool is_error = true;
	switch (mode) {
	case MODE_VERBOSE:
		if (! g_verbose) return 0;
		is_error = false;
		out = stdout;
		break;
	case MODE_DIAGNOSTIC_MSG:
		out = stdout;
		is_error = false;
		// fall through
	case MODE_DIAGNOSTIC_ERR:
		if (! g_diagnostic) return 0;
		break;
	}

	// if config syntax is desired, turn error messages into comments
	// unless g_config_fail_on_error is set.
	if (g_config_syntax && ( !is_error || ! g_config_fail_on_error)) {
		*ptmp++ = '#';
		--max_temp;
	}

	va_list args;
	va_start(args, fmt);
	int cch = vsnprintf(ptmp, max_temp, fmt, args);
	va_end (args);

	if (cch < 0 || cch >= max_temp) {
		// TJ: I don't think its possible to get where when g_config_syntax is on
		// because all known inputs will be < 4k in size.
		// but if we do, just suppress this error message
		if (! g_config_syntax) {
			fprintf(stderr, "internal error, %d is not enough space to format, value will be truncated.\n", max_temp);
		}
		temp[max_temp-1] = 0;
	}

	return fputs(temp, out);
}

