#include "condor_common.h"
#include "cgahp_common.h"
#include "condor_debug.h"

int
parse_gahp_command (const char* raw, char *** _argv, int * _argc) {

	*_argv = NULL;
	*_argc = 0;

	if (!raw) {
		dprintf(D_ALWAYS,"ERROR parse_gahp_command: empty command\n");
		return FALSE;
	}

	char ** argv = (char**)calloc (10,sizeof(char*)); 	// Max possible number of arguments
	int argc = 0;

	int beginning = 0;

	int len=strlen(raw);

	char * buff = (char*)malloc(len+1);
	int buff_len = 0;

	for (int i = 0; i<len; i++) {

		if ( raw[i] == '\\' ) {
			i++; 			//skip this char
			if (i<(len-1))
				buff[buff_len++] = raw[i];
			continue;
		}

		/* Check if charcater read was whitespace */
		if ( raw[i]==' ' || raw[i]=='\t' || raw[i]=='\r' || raw[i] == '\n') {

			/* Handle Transparency: we would only see these chars
			if they WEREN'T escaped, so treat them as arg separators
			*/
			buff[buff_len++] = '\0';
			argv[argc++] = strdup(buff);
			buff_len = 0;	// re-set temporary buffer

			beginning = i+1; // next char will be one after whitespace
		}
		else {
			// It's just a regular character, save it
			buff[buff_len++] = raw[i];
		}
	}

	/* Copy the last portion */
	buff[buff_len++] = '\0';
	argv[argc++] = strdup (buff);

	*_argv = argv;
	*_argc = argc;

	free (buff);
	return TRUE;


}
