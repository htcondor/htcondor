#include "condor_common.h"
#include "cgahp_common.h"
#include "condor_debug.h"

int
parse_gahp_command (const char* raw, Gahp_Args* args) {

	if (!raw) {
		dprintf(D_ALWAYS,"ERROR parse_gahp_command: empty command\n");
		return FALSE;
	}

	args->reset();

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
			args->add_arg( strdup(buff) );
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
	args->add_arg( strdup(buff) );

	free (buff);
	return TRUE;


}
