#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DATA_DEBUG 1

/*
 * in: base64 encoded string with
 * out: 3 uncoded bytes
 * returns number of chars passed over (4 or more)
 */
int decode_block(unsigned char *in, unsigned char *out) {
	unsigned char *c = in;
	int i = 0, j = 0;
	char temp[4];
	
		// convert char to corresponding base64 value(0-63)
	while(i < 4 && c && *c) {
		if(*c != ' ') {	// skip over whitespace
			if(*c == '+') {
				temp[i] = 62;
			} else if(*c == '/') {
				temp[i] = 63;
			} else if(*c == '=') {
				temp[i] = 0;	// padding
			} else if(*c >= '0' && *c <= '9') {
				temp[i] = *c + 4;
			} else if(*c >= 'A' && *c <= 'Z') {
				temp[i] = *c - 65;
			} else if(*c >= 'a' && *c <= 'z') {
				temp[i] = *c - 71;
			} else if(*c == ' ') {
				j++;
			} else {
				break;	//invalid char
			}
			i++;
		}
		j++;
		c++;
	}

		// Convert base64 values into bytes
	out[0] = (temp[0] << 2) + (temp[1] >> 4);
	out[1] = (temp[1] << 4) + (temp[2] >> 2);
	out[2] = (temp[2] << 6) + (temp[3] >> 0);
	
#if DATA_DEBUG
	i  = 0;
	printf("decoded '");
	while(i < j) {
		printf("%c", *(in+i));
		i++;
	}
	printf("' to '%s'\n", out);
#endif

	return j;	// Number of chars passed over
}

int main(int argc, char **argv) {
    if(argc != 2) {
		printf("Usage: data_plugin data:[<MIME-type>][;charset=<encoding>]"
			"[;base64],<data>/file\n");
        return -1;
    }
	
	char *c = NULL, *d = NULL;
	FILE *file = NULL;
	int base64 = 0, rval = -1;

	char *data = strchr(argv[1], ',');
	char *dest = strrchr(argv[1], '/');

	if(data && dest && (file = fopen(dest+1, "w"))) {
		rval = 4;

		// Check for base64 encoding
		c = argv[1];
		d = NULL;
		do {
			d = strchr(c, ';');
			if(d && d < data) {
				base64 = !strncmp(d, ";base64,", 7);
				c = d+1;
			} else {
				break;
			}
		} while(!base64);

		c = data+1;

		if(base64) {
			// base64 encoding
			unsigned char out[4];
			out[3] = '\0';
			while(c && *c && c+3 < dest && rval >= 4) {
				rval = decode_block((unsigned char*)c, out);
				fputs((char*)out, file);
				c+=rval;
			}

			// invalid base64 data (not 4 char aligned)
			if(c < dest) {
				rval = -1;
			}
		} else {
			// ASCII encoding
			char hex[2];

			while(c && *c && *c != '/') {
				if(*c == '%') {
					c++;
					strncpy(hex, c, 2);
					fputc(strtol(hex, NULL, 16), file);
					c++;
				} else {
					fputc(*c, file);
				}
				c++;
			}
		}
		fclose(file);
		if(rval < 4) {
			remove(dest+1);	// Remove file if we failed
		} else {
			rval = 0;
		}
	}
	return rval;	// >= 0 on success, -1 on failure
}
