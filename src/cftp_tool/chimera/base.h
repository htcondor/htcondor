#ifndef _CHIMERA_BASE_H_
#define _CHIMERA_BASE_H_


void convert_base2 (unsigned char num, char *out);


void convert_base4 (unsigned char num, char *out);


void convert_base16 (unsigned char num, char *out);


void hex_to_base2 (char *hexstr, char *binstr);


void hex_to_base4 (char *hexstr, char *base4str);


// assuming that the input string is only 4 bytes long, convert this number to a hex digit
char *get_hex_digit_from_bin (char *binstr);


// assuming that the input string is only 2 bytes long, convert this number to a hex digit
char *get_hex_digit_from_base4 (char *base4str);


void base2_to_hex (char *binstr, char *hexstr);


void base4_to_hex (char *base4str, char *hexstr);

#endif /* _CHIMERA_BASE_H_ */
