#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * in: base64 encoded string with
 * out: 3 uncoded bytes
 * returns number of chars passed over (4 or more)
 */
int decode_block(unsigned char *in, unsigned char *out) {
	unsigned char *c = in;
	int i = 0, j = 0;
	char temp[4] = { 0, 0, 0, 0};
	
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
	
	return j;	// Number of chars passed over
}

// decode ASCII hex characters
int decode_hex(const char * p, int len, int * pvalid)
{
	int valid = 1;
	int  value = 0;
	int ii;
	for (ii = 0; ii < len; ++ii) {
		int ch = p[ii];
		if (ch >= '0' && ch <= '9') {
			ch -= '0'; // ch is now a hex digit between 0x00 and 0x09
		} else {
			ch &= ~20; // force uppercase if ascii
			if (ch < 'A' || ch > 'F') {
				// digit was not a valid hex digit...
				valid = 0;
				break;
			}
			ch = ch - 'A' + 0x0A; // ch is now a hex digit between 0x0A and 0x0F
		}
		value = (value << 4) + ch;
	}
	if (pvalid) *pvalid = valid;
	return value;
}

int main(int argc, char **argv) {
	char *c = NULL, *d = NULL, *data = NULL, *dest = NULL, *lastslash = NULL;
	unsigned char out[4];
	FILE *file = NULL;
	int base64 = 0, rval = -1;

    if(argc == 2 && strcmp(argv[1], "-classad") == 0) {
		printf("%s",
			"PluginVersion = \"0.1\"\n"
			"PluginType = \"FileTransfer\"\n"
			"SupportedMethods = \"data\"\n"
			);

        return 0;
    }

    if(argc != 3) {
		return -1;
	}

	data = strchr(argv[1], ',');
	lastslash = strrchr(argv[1], '/');
	dest = argv[2];

	if(data && lastslash && (file = fopen(dest, "w"))) {
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
			out[3] = '\0';
			while(c && *c && c+3 < lastslash && rval >= 4) {
				rval = decode_block((unsigned char*)c, out);
				fputs((char*)out, file);
				c+=rval;
			}

			// invalid base64 data (not 4 char aligned)
			if(c < lastslash) {
				rval = -1;
			}
		} else {
			// ASCII encoding

			while(c && *c && *c != '/') {
				if(*c == '%') { // %XX two hex digits must follow a %
					int valid = 0, ch;
					c++;
					ch = decode_hex(c, 2, &valid);
					if ( ! valid) { rval = -1; break; }
					fputc(ch, file);
					c++;
				} else {
					fputc(*c, file);
				}
				c++;
			}
		}
		fclose(file);
		if(rval < 4) {
			remove(dest);	// Remove file if we failed
		} else {
			rval = 0;
		}
	}
	return rval;	// >= 0 on success, -1 on failure
}
