#ifndef _ZKM_BASE64_H_
#define _ZKM_BASE64_H_

#include <vector>
#include <string>
typedef unsigned char BYTE;

class Base64
{
public:
    static std::string zkm_base64_encode(const BYTE* buf, unsigned int bufLen);
    static std::vector<BYTE> zkm_base64_decode(std::string encoded_string);
};

// Caller needs to free the returned pointer
char* zkm_base64_encode(const unsigned char *input, int length);

// Caller needs to free *output if non-NULL
void zkm_base64_decode(const char *input,unsigned char **output, int *output_length);


#endif

