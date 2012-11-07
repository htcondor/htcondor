/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"
#include "CondorError.h"

#if !defined(SKIP_AUTHENTICATION) && defined(HAVE_EXT_OPENSSL)
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include "condor_auth.h"
#include "CryptKey.h"
#include "store_cred.h"
#include "my_username.h"
#include "MyString.h"

#include "condor_auth_passwd.h"

Condor_Auth_Passwd :: Condor_Auth_Passwd(ReliSock * sock)
    : Condor_Auth_Base(sock, CAUTH_PASSWORD)
{
	m_crypto = NULL;
}

Condor_Auth_Passwd :: ~Condor_Auth_Passwd()
{
    if(m_crypto) delete(m_crypto);
}

volatile char * 
Condor_Auth_Passwd::fetchPassword(const char* nameA,const char* nameB)
{
	char *name, *domain, *passwordA, *passwordB;

	if ( !nameA || !nameB ) {
		return NULL;
	}

		// Split nameA into name and domain, then get password
	name = strdup(nameA);	// copy string cuz input is const
	ASSERT(name);
	domain = strchr(name,'@');	// name and domain seperated by @
	if (domain) {
		*domain = '\0';
		domain++;
	}
	passwordA = getStoredCredential(name,domain);
	free(name);

		// Split nameB into name and domain, then get password
	name = strdup(nameB);	// copy string cuz input is const
	ASSERT(name);
	domain = strchr(name,'@');	// name and domain seperated by @
	if (domain) {
		*domain = '\0';
		domain++;
	}
	passwordB = getStoredCredential(name,domain);
	free(name);

		// If we failed to find either password, fail.
		// Someday, perhaps we should allow one them to be NULL, which
		// would enable assymetric authentication.  
	if ( !passwordA || !passwordB ) {
		if (passwordA) free(passwordA);
		if (passwordB) free(passwordB);
		return NULL;
	}

		// The result, the shared secret, is the password
		// for nameA concatenated with nameB.
	int len = strlen(passwordA) + strlen(passwordB) + 5;
	char *shared_secret = (char*) malloc(len);
	shared_secret[0] = '\0';
	strcpy(shared_secret,passwordA);
	strcat(shared_secret,passwordB);

	free(passwordA);
	free(passwordB);

	return (volatile char*)shared_secret;
}

char *
Condor_Auth_Passwd::fetchLogin()
{
	// return malloc-ed string "user@domain" that represents who we are.

	MyString login;
	
		// decide the login name we will try to authenticate with.  
	if ( is_root() ) {
		login.formatstr("%s@%s",POOL_PASSWORD_USERNAME,getLocalDomain());
	} else {
		// for now, always use the POOL_PASSWORD_USERNAME.  at some
		// point this code should call my_username() my_domainname().
		login.formatstr("%s@%s",POOL_PASSWORD_USERNAME,getLocalDomain());
	}

	return strdup( login.Value() );
}

bool
Condor_Auth_Passwd::setupCrypto(unsigned char* key, const int keylen)
{
		// get rid of any old crypto object
	if ( m_crypto ) delete m_crypto;
	m_crypto = NULL;

	if ( !key || !keylen ) {
		// cannot setup anything without a key
		return false;
	}

		// This could be 3des -- maybe we should use "best crypto" indirection.
	KeyInfo thekey(key,keylen,CONDOR_3DES);
	m_crypto = new Condor_Crypt_3des(thekey);
	return m_crypto ? true : false;
}

bool
Condor_Auth_Passwd::encrypt(unsigned char* input, 
					int input_len, unsigned char* & output, int& output_len)
{
	return encrypt_or_decrypt(true,input,input_len,output,output_len);
}

bool
Condor_Auth_Passwd::decrypt(unsigned char* input, int input_len, 
							unsigned char* & output, int& output_len)
{
	return encrypt_or_decrypt(false,input,input_len,output,output_len);
}

bool
Condor_Auth_Passwd::encrypt_or_decrypt(bool want_encrypt, 
									   unsigned char* input, 
									   int input_len, 
									   unsigned char* &output, 
									   int &output_len)
{
	bool result;
	
		// clean up any old buffers that perhaps were left over
	if ( output ) free(output);
	output = NULL;
	output_len = 0;
	
		// check some intput params
	if (!input || input_len < 1) {
		return false;
	}
	
		// make certain we got a crypto object
	if (!m_crypto) {
		return false;
	}

		// do the work
	m_crypto->resetState();
	if (want_encrypt) {
		result = m_crypto->encrypt(input,input_len,output,output_len);
	} else {
		result = m_crypto->decrypt(input,input_len,output,output_len);
	}
	
		// mark output_len as zero upon failure
	if (!result) {
		output_len = 0;
	}

		// an output_len of zero means failure; cleanup and return
	if ( output_len == 0 ) {
		if ( output ) free(output);
		output = NULL;
		return false;
	} 
	
		// if we made it here, we're golden!
	return true;
}

int 
Condor_Auth_Passwd::wrap(char *   input, 
						 int      input_len, 
						 char*&   output, 
						 int&     output_len)
{
	bool result;
	unsigned char* in = (unsigned char*)input;
	unsigned char* out = (unsigned char*)output;
	dprintf(D_SECURITY, "In Condor_Auth_Passwd::wrap.\n");
	result = encrypt(in,input_len,out,output_len);
	
	output = (char *)out;
	
	return result ? TRUE : FALSE;
}

int 
Condor_Auth_Passwd::unwrap(char *   input, 
						   int      input_len, 
						   char*&   output, 
						   int&     output_len)
{
	bool result;
	unsigned char* in = (unsigned char*)input;
	unsigned char* out = (unsigned char*)output;
	
	dprintf(D_SECURITY, "In Condor_Auth_Passwd::unwrap.\n");
	result = decrypt(in,input_len,out,output_len);
	
	output = (char *)out;
	
	return result ? TRUE : FALSE;
}

bool
Condor_Auth_Passwd::setup_shared_keys(struct sk_buf *sk) 
{
	if ( sk->shared_key == NULL ) {
		return false;
	}

		// These were generated randomly at coding time (see
		// setup_seed).  They are used as hash keys to create the two
		// keys K and K' (referred to here as ka and kb,
		// respectively).  We derive these ka and kb by hmacing the
		// shared key with these two seed keys.
    unsigned char *seed_ka = (unsigned char *)malloc(AUTH_PW_KEY_LEN);
    unsigned char *seed_kb = (unsigned char *)malloc(AUTH_PW_KEY_LEN);
    
		// These are the keys K and K' referred to in the AKEP2
		// description.
    volatile unsigned char *ka = (unsigned char *)malloc(EVP_MAX_MD_SIZE);
    volatile unsigned char *kb = (unsigned char *)malloc(EVP_MAX_MD_SIZE);

    unsigned int ka_len = 0;
    unsigned int kb_len = 0;

		// If any are NULL, free the others...
    if( !seed_ka || !seed_kb || !ka || !kb ) {
		if(seed_ka) free(seed_ka);
		if(seed_kb) free(seed_kb);
		if(ka) free((void *)const_cast<unsigned char*>(ka));
		if(kb) free((void *)const_cast<unsigned char*>(kb));
        dprintf(D_SECURITY, "Can't authenticate: malloc error.\n");
        return false;
    }
    
		// Fill in the data for the seed keys.
    setup_seed(seed_ka, seed_kb);

    sk->len = strlen(const_cast<char *>(sk->shared_key));

		// Generate the shared keys K and K'
    hmac((unsigned char *)const_cast<char*>(sk->shared_key), sk->len,
		 seed_ka, AUTH_PW_KEY_LEN, 
		 const_cast<unsigned char *>(ka), &ka_len );

    hmac((unsigned char *)const_cast<char*>(sk->shared_key), sk->len,
		 seed_kb, AUTH_PW_KEY_LEN, 
		 const_cast<unsigned char *>(kb), &kb_len );

	free(seed_ka);
	free(seed_kb);
	sk->ka = ka;
    sk->kb = kb;
    sk->ka_len = ka_len;
    sk->kb_len = kb_len;

    return true;
}

void
Condor_Auth_Passwd::setup_seed(unsigned char *ka, unsigned char *kb) 
{    
		// This is so ugly!
	ka[0] = 62;
    ka[1] = 74;
    ka[2] = 80;
    ka[3] = 32;
    ka[4] = 71;
    ka[5] = 213;
    ka[6] = 244;
    ka[7] = 229;
    ka[8] = 220;
    ka[9] = 124;
    ka[10] = 105;
    ka[11] = 187;
    ka[12] = 82;
    ka[13] = 16;
    ka[14] = 203;
    ka[15] = 182;
    ka[16] = 22;
    ka[17] = 122;
    ka[18] = 221;
    ka[19] = 128;
    ka[20] = 132;
    ka[21] = 247;
    ka[22] = 221;
    ka[23] = 158;
    ka[24] = 243;
    ka[25] = 173;
    ka[26] = 44;
    ka[27] = 202;
    ka[28] = 113;
    ka[29] = 210;
    ka[30] = 131;
    ka[31] = 221;
    ka[32] = 17;
    ka[33] = 74;
    ka[34] = 79;
    ka[35] = 187;
    ka[36] = 123;
    ka[37] = 30;
    ka[38] = 233;
    ka[39] = 10;
    ka[40] = 223;
    ka[41] = 168;
    ka[42] = 98;
    ka[43] = 196;
    ka[44] = 67;
    ka[45] = 4;
    ka[46] = 222;
    ka[47] = 84;
    ka[48] = 115;
    ka[49] = 163;
    ka[50] = 23;
    ka[51] = 47;
    ka[52] = 115;
    ka[53] = 92;
    ka[54] = 44;
    ka[55] = 187;
    ka[56] = 110;
    ka[57] = 119;
    ka[58] = 91;
    ka[59] = 93;
    ka[60] = 64;
    ka[61] = 211;
    ka[62] = 159;
    ka[63] = 172;
    ka[64] = 232;
    ka[65] = 115;
    ka[66] = 24;
    ka[67] = 37;
    ka[68] = 35;
    ka[69] = 249;
    ka[70] = 37;
    ka[71] = 43;
    ka[72] = 98;
    ka[73] = 59;
    ka[74] = 224;
    ka[75] = 212;
    ka[76] = 177;
    ka[77] = 103;
    ka[78] = 163;
    ka[79] = 168;
    ka[80] = 4;
    ka[81] = 12;
    ka[82] = 172;
    ka[83] = 254;
    ka[84] = 233;
    ka[85] = 238;
    ka[86] = 61;
    ka[87] = 160;
    ka[88] = 44;
    ka[89] = 10;
    ka[90] = 187;
    ka[91] = 244;
    ka[92] = 217;
    ka[93] = 216;
    ka[94] = 177;
    ka[95] = 31;
    ka[96] = 137;
    ka[97] = 0;
    ka[98] = 76;
    ka[99] = 148;
    ka[100] = 57;
    ka[101] = 35;
    ka[102] = 206;
    ka[103] = 93;
    ka[104] = 149;
    ka[105] = 8;
    ka[106] = 187;
    ka[107] = 63;
    ka[108] = 4;
    ka[109] = 188;
    ka[110] = 102;
    ka[111] = 163;
    ka[112] = 250;
    ka[113] = 32;
    ka[114] = 161;
    ka[115] = 58;
    ka[116] = 65;
    ka[117] = 108;
    ka[118] = 94;
    ka[119] = 111;
    ka[120] = 78;
    ka[121] = 13;
    ka[122] = 49;
    ka[123] = 135;
    ka[124] = 212;
    ka[125] = 95;
    ka[126] = 199;
    ka[127] = 131;
    ka[128] = 53;
    ka[129] = 197;
    ka[130] = 228;
    ka[131] = 133;
    ka[132] = 219;
    ka[133] = 44;
    ka[134] = 90;
    ka[135] = 55;
    ka[136] = 23;
    ka[137] = 151;
    ka[138] = 12;
    ka[139] = 194;
    ka[140] = 110;
    ka[141] = 123;
    ka[142] = 107;
    ka[143] = 157;
    ka[144] = 25;
    ka[145] = 101;
    ka[146] = 180;
    ka[147] = 122;
    ka[148] = 103;
    ka[149] = 223;
    ka[150] = 119;
    ka[151] = 163;
    ka[152] = 31;
    ka[153] = 34;
    ka[154] = 240;
    ka[155] = 138;
    ka[156] = 108;
    ka[157] = 11;
    ka[158] = 165;
    ka[159] = 112;
    ka[160] = 151;
    ka[161] = 162;
    ka[162] = 26;
    ka[163] = 156;
    ka[164] = 167;
    ka[165] = 198;
    ka[166] = 4;
    ka[167] = 36;
    ka[168] = 247;
    ka[169] = 39;
    ka[170] = 57;
    ka[171] = 171;
    ka[172] = 92;
    ka[173] = 185;
    ka[174] = 21;
    ka[175] = 164;
    ka[176] = 24;
    ka[177] = 91;
    ka[178] = 209;
    ka[179] = 9;
    ka[180] = 130;
    ka[181] = 142;
    ka[182] = 53;
    ka[183] = 228;
    ka[184] = 33;
    ka[185] = 8;
    ka[186] = 171;
    ka[187] = 133;
    ka[188] = 28;
    ka[189] = 8;
    ka[190] = 163;
    ka[191] = 223;
    ka[192] = 253;
    ka[193] = 224;
    ka[194] = 227;
    ka[195] = 176;
    ka[196] = 111;
    ka[197] = 61;
    ka[198] = 57;
    ka[199] = 56;
    ka[200] = 205;
    ka[201] = 173;
    ka[202] = 109;
    ka[203] = 246;
    ka[204] = 239;
    ka[205] = 154;
    ka[206] = 111;
    ka[207] = 109;
    ka[208] = 194;
    ka[209] = 203;
    ka[210] = 116;
    ka[211] = 240;
    ka[212] = 34;
    ka[213] = 133;
    ka[214] = 18;
    ka[215] = 235;
    ka[216] = 122;
    ka[217] = 61;
    ka[218] = 104;
    ka[219] = 35;
    ka[220] = 1;
    ka[221] = 6;
    ka[222] = 132;
    ka[223] = 176;
    ka[224] = 21;
    ka[225] = 193;
    ka[226] = 42;
    ka[227] = 195;
    ka[228] = 1;
    ka[229] = 76;
    ka[230] = 79;
    ka[231] = 159;
    ka[232] = 147;
    ka[233] = 142;
    ka[234] = 56;
    ka[235] = 77;
    ka[236] = 173;
    ka[237] = 30;
    ka[238] = 59;
    ka[239] = 215;
    ka[240] = 69;
    ka[241] = 255;
    ka[242] = 140;
    ka[243] = 20;
    ka[244] = 31;
    ka[245] = 215;
    ka[246] = 11;
    ka[247] = 70;
    ka[248] = 91;
    ka[249] = 168;
    ka[250] = 175;
    ka[251] = 93;
    ka[252] = 27;
    ka[253] = 152;
    ka[254] = 180;
    ka[255] = 177;
    kb[0] = 1;
    kb[1] = 0;
    kb[2] = 38;
    kb[3] = 173;
    kb[4] = 117;
    kb[5] = 223;
    kb[6] = 198;
    kb[7] = 193;
    kb[8] = 144;
    kb[9] = 165;
    kb[10] = 162;
    kb[11] = 102;
    kb[12] = 176;
    kb[13] = 209;
    kb[14] = 181;
    kb[15] = 216;
    kb[16] = 96;
    kb[17] = 247;
    kb[18] = 207;
    kb[19] = 163;
    kb[20] = 132;
    kb[21] = 103;
    kb[22] = 32;
    kb[23] = 85;
    kb[24] = 1;
    kb[25] = 205;
    kb[26] = 70;
    kb[27] = 13;
    kb[28] = 74;
    kb[29] = 136;
    kb[30] = 212;
    kb[31] = 115;
    kb[32] = 250;
    kb[33] = 82;
    kb[34] = 224;
    kb[35] = 179;
    kb[36] = 233;
    kb[37] = 20;
    kb[38] = 30;
    kb[39] = 51;
    kb[40] = 201;
    kb[41] = 125;
    kb[42] = 133;
    kb[43] = 30;
    kb[44] = 238;
    kb[45] = 45;
    kb[46] = 211;
    kb[47] = 54;
    kb[48] = 50;
    kb[49] = 243;
    kb[50] = 136;
    kb[51] = 103;
    kb[52] = 104;
    kb[53] = 239;
    kb[54] = 1;
    kb[55] = 14;
    kb[56] = 200;
    kb[57] = 223;
    kb[58] = 221;
    kb[59] = 102;
    kb[60] = 138;
    kb[61] = 222;
    kb[62] = 146;
    kb[63] = 213;
    kb[64] = 195;
    kb[65] = 67;
    kb[66] = 8;
    kb[67] = 187;
    kb[68] = 36;
    kb[69] = 56;
    kb[70] = 149;
    kb[71] = 216;
    kb[72] = 78;
    kb[73] = 215;
    kb[74] = 133;
    kb[75] = 226;
    kb[76] = 114;
    kb[77] = 104;
    kb[78] = 204;
    kb[79] = 94;
    kb[80] = 231;
    kb[81] = 86;
    kb[82] = 13;
    kb[83] = 228;
    kb[84] = 152;
    kb[85] = 40;
    kb[86] = 250;
    kb[87] = 183;
    kb[88] = 102;
    kb[89] = 194;
    kb[90] = 173;
    kb[91] = 140;
    kb[92] = 11;
    kb[93] = 44;
    kb[94] = 10;
    kb[95] = 251;
    kb[96] = 67;
    kb[97] = 92;
    kb[98] = 56;
    kb[99] = 45;
    kb[100] = 181;
    kb[101] = 210;
    kb[102] = 255;
    kb[103] = 54;
    kb[104] = 168;
    kb[105] = 174;
    kb[106] = 173;
    kb[107] = 88;
    kb[108] = 32;
    kb[109] = 71;
    kb[110] = 10;
    kb[111] = 154;
    kb[112] = 212;
    kb[113] = 93;
    kb[114] = 121;
    kb[115] = 133;
    kb[116] = 111;
    kb[117] = 94;
    kb[118] = 46;
    kb[119] = 206;
    kb[120] = 137;
    kb[121] = 75;
    kb[122] = 210;
    kb[123] = 80;
    kb[124] = 121;
    kb[125] = 41;
    kb[126] = 220;
    kb[127] = 242;
    kb[128] = 111;
    kb[129] = 125;
    kb[130] = 9;
    kb[131] = 240;
    kb[132] = 2;
    kb[133] = 143;
    kb[134] = 26;
    kb[135] = 196;
    kb[136] = 217;
    kb[137] = 113;
    kb[138] = 244;
    kb[139] = 130;
    kb[140] = 12;
    kb[141] = 95;
    kb[142] = 84;
    kb[143] = 113;
    kb[144] = 126;
    kb[145] = 157;
    kb[146] = 205;
    kb[147] = 171;
    kb[148] = 235;
    kb[149] = 33;
    kb[150] = 95;
    kb[151] = 97;
    kb[152] = 101;
    kb[153] = 93;
    kb[154] = 234;
    kb[155] = 212;
    kb[156] = 183;
    kb[157] = 44;
    kb[158] = 61;
    kb[159] = 59;
    kb[160] = 95;
    kb[161] = 102;
    kb[162] = 250;
    kb[163] = 75;
    kb[164] = 48;
    kb[165] = 184;
    kb[166] = 88;
    kb[167] = 136;
    kb[168] = 214;
    kb[169] = 47;
    kb[170] = 172;
    kb[171] = 212;
    kb[172] = 18;
    kb[173] = 156;
    kb[174] = 19;
    kb[175] = 4;
    kb[176] = 145;
    kb[177] = 159;
    kb[178] = 105;
    kb[179] = 173;
    kb[180] = 109;
    kb[181] = 140;
    kb[182] = 44;
    kb[183] = 67;
    kb[184] = 217;
    kb[185] = 206;
    kb[186] = 92;
    kb[187] = 219;
    kb[188] = 49;
    kb[189] = 212;
    kb[190] = 88;
    kb[191] = 3;
    kb[192] = 82;
    kb[193] = 199;
    kb[194] = 54;
    kb[195] = 43;
    kb[196] = 141;
    kb[197] = 128;
    kb[198] = 183;
    kb[199] = 239;
    kb[200] = 27;
    kb[201] = 186;
    kb[202] = 93;
    kb[203] = 103;
    kb[204] = 102;
    kb[205] = 96;
    kb[206] = 169;
    kb[207] = 68;
    kb[208] = 118;
    kb[209] = 69;
    kb[210] = 2;
    kb[211] = 249;
    kb[212] = 29;
    kb[213] = 29;
    kb[214] = 60;
    kb[215] = 84;
    kb[216] = 145;
    kb[217] = 12;
    kb[218] = 8;
    kb[219] = 139;
    kb[220] = 204;
    kb[221] = 183;
    kb[222] = 43;
    kb[223] = 17;
    kb[224] = 148;
    kb[225] = 138;
    kb[226] = 94;
    kb[227] = 26;
    kb[228] = 29;
    kb[229] = 205;
    kb[230] = 4;
    kb[231] = 54;
    kb[232] = 156;
    kb[233] = 23;
    kb[234] = 210;
    kb[235] = 152;
    kb[236] = 128;
    kb[237] = 76;
    kb[238] = 33;
    kb[239] = 110;
    kb[240] = 122;
    kb[241] = 38;
    kb[242] = 144;
    kb[243] = 184;
    kb[244] = 192;
    kb[245] = 233;
    kb[246] = 112;
    kb[247] = 54;
    kb[248] = 51;
    kb[249] = 0;
    kb[250] = 208;
    kb[251] = 146;
    kb[252] = 223;
    kb[253] = 36;
    kb[254] = 251;
    kb[255] = 140;
}

void
Condor_Auth_Passwd::hmac(unsigned char *sk, int sk_len,
          unsigned char *key, int key_len,
          unsigned char *result, unsigned int *result_len)
{
		// TODO: when stronger hashing functions are available, they
		// should be substituted.
    HMAC(EVP_sha1(), key, key_len, sk, sk_len, result, result_len);
}

void
Condor_Auth_Passwd::init_sk(struct sk_buf *sk) 
{       
    sk->shared_key = NULL;
	sk->len        = 0;
    sk->ka         = NULL;
	sk->ka_len     = 0;
    sk->kb         = NULL;
	sk->kb_len     = 0;
}

volatile void *
Condor_Auth_Passwd::spc_memset(volatile void *dst, int c, size_t len)
{
		// This should get changed to the lib func when it's there.
	volatile char *buf;
   
	for (buf = (volatile char *)dst;  len;  buf[--len] = c) { }
	return dst;
}
void
Condor_Auth_Passwd::destroy_sk(struct sk_buf *sk) 
{
    if(sk->shared_key) {
		spc_memset(sk->shared_key, 0, sk->len);
        free((void *)const_cast<char*>(sk->shared_key));
    }
	if(sk->ka) {
		spc_memset(sk->ka, 0, sk->ka_len);
		free((void *)const_cast<unsigned char*>(sk->ka));
		sk->ka_len = 0;
	}
	if(sk->kb) {
		spc_memset(sk->kb, 0, sk->kb_len);
		free((void *)const_cast<unsigned char*>(sk->kb));
		sk->kb_len = 0;
	}
	init_sk(sk);
}

void
Condor_Auth_Passwd::init_t_buf(struct msg_t_buf *t) 
{
	t->a           = NULL;
	t->b           = NULL;
	t->ra          = NULL;
	t->rb          = NULL;
	t->hkt         = NULL;
	t->hkt_len     = 0;
	t->hk          = NULL;
	t->hk_len      = 0;
}
void
Condor_Auth_Passwd::destroy_t_buf(struct msg_t_buf *t) 
{
	if(t->a) {
		free(t->a);
		t->a = NULL;
	}
	if(t->b) {
		free(t->b);
		t->b = NULL;
	}
	if(t->ra) {
		free(t->ra);
		t->ra = NULL;
	}
	if(t->rb) {
		free(t->rb);
		t->rb = NULL;
	}
	if(t->hkt) {
		free(t->hkt);
		t->hkt = NULL;
	}
	if(t->hk) {
		free(t->hk);
		t->hk = NULL;
	}
	init_t_buf(t);
}

int
Condor_Auth_Passwd::authenticate(const char * /* remoteHost */, 
								 CondorError* /* errstack */ )
{
    int client_status = AUTH_PW_A_OK;
    int server_status = AUTH_PW_A_OK;
	int ret_value = -1;
    
    struct msg_t_buf t_client;
    struct msg_t_buf t_server;

		// In order to create the shared keys used by the client and
		// the server in this protocol, we take a single shared secret
		// and hmac it twice with two different keys.  The original
		// password buffer and the two generated keys are stored in
		// the sk_buf structure.
    struct sk_buf sk;

	int tmp_rv;

		// Initialize these structures (with NULLs)
    init_t_buf(&t_client);
    init_t_buf(&t_server);
    init_sk(&sk);
	dprintf(D_SECURITY, "PW.\n");

	if ( mySock_->isClient() ) {
			// ** client side authentication **
        
			// Get my name, password and setup the shared keys based
			// on this data.  The server will do the same when it
			// learns my name.
		dprintf(D_SECURITY, "PW: getting name.\n");
        t_client.a = fetchLogin();

			// We complete the entire protocol even if there's an
			// error, but there's no point trying to actually do any
			// work.  This is protocol step (a).
		dprintf(D_SECURITY, "PW: Generating ra.\n");
	        
        if(client_status == AUTH_PW_A_OK) {
			t_client.ra = Condor_Crypt_Base::randomKey(AUTH_PW_KEY_LEN);
			if(!t_client.ra) {
				dprintf(D_SECURITY, "Malloc error in random key?\n");
				client_status = AUTH_PW_ERROR;
			}
        }
        
			// This differs from the protocol description in the book
			// only that the client also sends its name "A".  The
			// protocol doesn't mention how the peers know who they're
			// talking to.  This is also protocol step (a).
		dprintf(D_SECURITY, "PW: Client sending.\n");
        client_status = client_send_one(client_status, &t_client);

		if(client_status == AUTH_PW_ABORT) {
			goto client_abort;
		}
			// This is protocol step (b).
		dprintf(D_SECURITY, "PW: Client receiving.\n");
        server_status = client_receive(&client_status, &t_server);
		if(client_status == AUTH_PW_ABORT) {
			goto client_abort;
		}

			// Now that we've received the server's name, we can go
			// ahead and setup the keys.
		if(client_status == AUTH_PW_A_OK && server_status == AUTH_PW_A_OK) {
			sk.shared_key = fetchPassword(t_client.a, t_server.b);
			dprintf(D_SECURITY, "PW: Client setting keys.\n");
			if(!setup_shared_keys(&sk)) {
				client_status = AUTH_PW_ERROR;
			}
		}

			// This is protocol step (c).
		if(client_status == AUTH_PW_A_OK
		   && server_status == AUTH_PW_A_OK) {
			dprintf(D_SECURITY, "PW: Client checking T.\n");
			client_status = client_check_t_validity(&t_client, &t_server, &sk);
		}

			// Are we copying the data into the t_client struct?
			// This is protocol step (d).  Server does (e).
		dprintf(D_SECURITY, "PW: CLient sending two.\n");
        client_status = client_send_two(client_status, &t_client, &sk);
		if(client_status == AUTH_PW_ABORT) {
			goto client_abort;
		}

	client_abort:
			// This is protocol step (f).
		if(client_status == AUTH_PW_A_OK
		   && server_status == AUTH_PW_A_OK
		   && set_session_key(&t_client, &sk)) {
			dprintf(D_SECURITY, "PW: CLient set session key.\n");
			ret_value = 1;
		} else {
			ret_value = 0;
		}
	}
	else {
			// ** server side authentication **
		
			// First we get the client's name and ra, protocol step
			// (a).
		dprintf(D_SECURITY, "PW: Server receiving 1.\n");
		client_status = server_receive_one(&server_status, &t_client);
		if(client_status == AUTH_PW_ABORT || server_status == AUTH_PW_ABORT) {
			goto server_abort;
		}

			// Then we do the key setup, and generate the random string.
		if(client_status == AUTH_PW_A_OK && server_status == AUTH_PW_A_OK) {
			t_server.b = fetchLogin();
			dprintf(D_SECURITY, "PW: Server fetching password.\n");
			sk.shared_key = fetchPassword(t_client.a, t_server.b);
			if(!setup_shared_keys(&sk)) {
				server_status = AUTH_PW_ERROR;
			} else {
				dprintf(D_SECURITY, "PW: Server generating rb.\n");
		//server_status = server_gen_rand_rb(&t_server);
	            t_server.rb = Condor_Crypt_Base::randomKey(AUTH_PW_KEY_LEN);
				if(t_client.a) {
					t_server.a = strdup(t_client.a);
				} else {
					t_server.a = NULL;
				}
				t_server.ra = (unsigned char *)malloc(AUTH_PW_KEY_LEN);
				if(!t_server.ra || !t_server.rb) {
					dprintf(D_SECURITY, "Malloc error 1.\n"); 
					server_status = AUTH_PW_ERROR;
				} else {
					memcpy(t_server.ra, t_client.ra, AUTH_PW_KEY_LEN);
				}
			}
		}

			// Protocol message (2), step (b).
		dprintf(D_SECURITY, "PW: Server sending.\n");
		tmp_rv = server_send(server_status, &t_server, &sk);
		if(server_status == AUTH_PW_A_OK) {
			server_status = tmp_rv;
		}
		if(server_status == AUTH_PW_ABORT) {
			goto server_abort;
		}

			// Protocol step (d)
		dprintf(D_SECURITY, "PW: Server receiving 2.\n");
		if(t_server.a) {
			t_client.a = strdup(t_server.a);
		} else { 
			t_client.a = NULL;
		}
        if(server_status == AUTH_PW_A_OK) {
			t_client.rb = (unsigned char *)malloc(AUTH_PW_KEY_LEN);
			if(!t_client.rb) {
				dprintf(D_SECURITY, "Malloc_error.\n");
				server_status = AUTH_PW_ERROR;
			} else {
				memcpy(t_client.rb, t_server.rb, AUTH_PW_KEY_LEN);
			}
		} else {
			t_client.rb = NULL;
		}
		client_status = server_receive_two(&server_status, &t_client);

		if(server_status == AUTH_PW_A_OK
		   && client_status == AUTH_PW_A_OK) {
				// Protocol step (e)
			dprintf(D_SECURITY, "PW: Server checking hk.\n");
			server_status = server_check_hk_validity(&t_client, 
													 &t_server, &sk);
		}
		
	server_abort:
				// protocol step (f)
		if(client_status == AUTH_PW_A_OK
		   && server_status == AUTH_PW_A_OK
		   && set_session_key(&t_server, &sk)) {
			dprintf(D_SECURITY, "PW: Server set session key.\n");
			ret_value = 1;
		} else {
			ret_value = 0;
		}
		
	}

		//ret_value is 1 for success, 0 for failure.
	if ( ret_value == 1 ) {
			// if all is good, set the remote user and domain names
		char *login, *domain;
		if ( mySock_->isClient() ) {
			login = t_server.b;	// server is remote to client
		} else {
			login = t_client.a; // client is remote to server
		}
		ASSERT(login);
		domain = strchr(login,'@');
		if (domain) {
			*domain='\0';
			domain++;
		}

		setRemoteUser(login);
		setRemoteDomain(domain);
	}

	destroy_t_buf(&t_client);
	destroy_t_buf(&t_server);
	destroy_sk(&sk);

		//return 1 for success, 0 for failure. Server should send
		//sucess/failure back to client so client can know what to
		//return.
	return ret_value;
}

bool 
Condor_Auth_Passwd::calculate_hk(struct msg_t_buf *t_buf, struct sk_buf *sk)
{
	unsigned char *buffer;
	int prefix_len, buffer_len;

	dprintf(D_SECURITY, "In calculate_hk.\n");
	if(!t_buf->a || !t_buf->rb) {
		dprintf(D_SECURITY, "Can't hk hmac NULL.\n");
		return false;
	}

		// Create a buffer that contains the values to be hmaced.  The
		// buffer needs to be long enough to contain the client name,
		// it's trailing null, and the random string rb.
	prefix_len = strlen(t_buf->a);
	buffer_len = prefix_len+1+AUTH_PW_KEY_LEN;
	buffer = (unsigned char *)malloc(buffer_len);
	t_buf->hk = (unsigned char *)malloc(EVP_MAX_MD_SIZE);
	if(!buffer || !t_buf->hk) {
		dprintf(D_SECURITY,"Malloc error 2.\n");
		goto hk_error;
	}

	memset(buffer, 0, buffer_len);
		// Copy the data into the buffer.
	memcpy(buffer, t_buf->a, strlen(t_buf->a));
	memcpy(buffer+prefix_len+1, t_buf->rb, AUTH_PW_KEY_LEN);
	
		// Calculate the hmac using K as the key.
	hmac( buffer, buffer_len,
		  const_cast<unsigned char *>(sk->ka), sk->ka_len,
		  t_buf->hk, &t_buf->hk_len);
	if(t_buf->hk_len < 1) {
		dprintf(D_SECURITY, "Error: hk hmac too short.\n");
		goto hk_error;
	}

	free(buffer);
	return true;
 hk_error:
	if(buffer) 
		free(buffer);
	if(t_buf->hk) {
		free(t_buf->hk);
		t_buf->hk = NULL;
	}
	return false;
}

int 
Condor_Auth_Passwd::client_send_two(int client_status, 
									struct msg_t_buf *t_client, 
									struct sk_buf *sk)
{
	char *send_a   = t_client->a;
	unsigned char *send_b   = t_client->rb;
	unsigned char *send_c   = NULL;
	int send_a_len = 0;
	int send_b_len = AUTH_PW_KEY_LEN;
	int send_c_len = 0;
	char nullstr[2];

	dprintf(D_SECURITY, "In client_send_two.\n");

	nullstr[0] = 0;
	nullstr[1] = 0;

		// First we check for sanity in what we're sending.
	if(send_a == NULL) {
		client_status = AUTH_PW_ERROR;
		dprintf(D_SECURITY, "Client error: don't know my own name?\n");
	} else {
		send_a_len = strlen(send_a);
	} 
	if(send_b == NULL) {
		client_status = AUTH_PW_ERROR;
		dprintf(D_SECURITY, "Can't send null for random string.\n");
	}
	if(send_a_len == 0) {
		client_status = AUTH_PW_ERROR;
		dprintf(D_SECURITY, "Client error: I have no name?\n");
	}

		// If everything's OK so far, we calculate the hash.
	if( client_status == AUTH_PW_A_OK ) {
		if(!calculate_hk(t_client, sk)) {
			client_status = AUTH_PW_ERROR;
			dprintf(D_SECURITY, "Client can't calculate hk.\n");
		} else {
			dprintf(D_SECURITY, "Client calculated hk.\n");
		}
	}

		// If there's an error, we don't send anything.
	if(client_status != AUTH_PW_A_OK) {
		send_a = nullstr;
		send_b = (unsigned char *)nullstr;
		send_c = (unsigned char *)nullstr;
		send_a_len = 0;
		send_b_len = 0;
		send_c_len = 0;
	} else {
		send_c = t_client->hk;
		send_c_len = t_client->hk_len;
	}

	dprintf(D_SECURITY, "Client sending: %d(%s) %d %d\n",
			send_a_len, send_a, send_b_len, send_c_len);

	mySock_->encode();
	if( !mySock_->code(client_status)
		|| !mySock_->code(send_a_len)
		|| !mySock_->code(send_a) 
		|| !mySock_->code(send_b_len)
		|| !(send_b_len == mySock_->put_bytes(send_b, send_b_len))
		|| !mySock_->code(send_c_len)
		|| !(send_c_len == mySock_->put_bytes(send_c, send_c_len))
		|| !mySock_->end_of_message()) {
		dprintf(D_SECURITY, "Error sending to server (second message).  "
				"Aborting...\n");
		client_status = AUTH_PW_ABORT;
	}
	dprintf(D_SECURITY, "Sent ok.\n");
	return client_status;
}

int Condor_Auth_Passwd::client_check_t_validity(msg_t_buf *t_client, 
												msg_t_buf *t_server, 
												sk_buf *sk) 
{

		// We received t_server from the server, want to compare it
		// against t_client.

		// First see if everything's there.
	if(!t_client->a || !t_client->ra
	   || (strlen(t_client->a) == 0) 
	   || !t_server->a || !t_server->b 
	   || (strlen(t_server->a) == 0) || (strlen(t_server->b) == 0)
	   || !t_server->ra || !t_server->rb 
	   || !t_server->hkt || (t_server->hkt_len == 0)) {
		dprintf(D_SECURITY, "Error: unexpected null.\n");
		return AUTH_PW_ERROR;
	}
		
		// Fill in the information supplied by the server.
	if(t_server->b) {
		t_client->b = strdup(t_server->b);
	} else {
		t_client->b = NULL;
	}
	if((t_client->rb = (unsigned char *)malloc(AUTH_PW_KEY_LEN))) {
		memcpy(t_client->rb, t_server->rb, AUTH_PW_KEY_LEN);
	} else {
		dprintf(D_SECURITY, "Malloc error 3.\n");
		return AUTH_PW_ABORT;
	}

		// Check that its data sent was the same as ours.
	if(strcmp(t_client->a, t_server->a)) {
		dprintf(D_SECURITY, "Error: server message T contains "
				"wrong client name.\n");
		return AUTH_PW_ERROR;
	}
	if(memcmp(t_client->ra, t_server->ra, AUTH_PW_KEY_LEN)) {
		dprintf(D_SECURITY, "Error: server message T contains "
				"different random string than what I sent.\n");
		return AUTH_PW_ERROR;
	}

		// Calculate the hash on our data.
	if(!calculate_hkt(t_client, sk)) {
		dprintf(D_SECURITY, "Error calculating hmac.\n");
		return AUTH_PW_ERROR;
	}

		// Compare the client's hash with that supplied by the server.
	if(memcmp(t_client->hkt, t_server->hkt, t_client->hkt_len)) {
		dprintf(D_SECURITY, "Hash supplied by server doesn't match "
				"that calculated by the client.\n");
		return AUTH_PW_ERROR;
	}

	return AUTH_PW_A_OK;
}

int Condor_Auth_Passwd::server_check_hk_validity(struct msg_t_buf *t_client, 
												 struct msg_t_buf *t_server, 
												 struct sk_buf *sk) 
{
		// Check for empty input.  This shouldn't happen, but...
	if(!t_client->a || !t_client->rb || !t_client->hk || !t_client->hk_len) {
		dprintf(D_SECURITY, "Error: unexpected NULL.\n");
		return AUTH_PW_ERROR;
	}

		// Check that everything's the same.
	if(strcmp(t_client->a, t_server->a)) {
		dprintf(D_SECURITY, 
				"Error: client message contains wrong server name.\n");
		return AUTH_PW_ERROR;
	}
	if(memcmp(t_client->rb, t_server->rb, AUTH_PW_KEY_LEN)) {
		dprintf(D_SECURITY, 
				"Error: client message contains wrong random rb.\n");
		return AUTH_PW_ERROR;
	}

		// Calculate the hash.
	if(!calculate_hk(t_server, sk)) {
		dprintf(D_SECURITY, "Error calculating hmac.\n");
		return AUTH_PW_ERROR;
	}
		// See that the hash is the same.
	if(t_server->hk_len != t_client->hk_len 
	   || memcmp(t_client->hk, t_server->hk, t_server->hk_len)) {
		dprintf(D_SECURITY, "Hash supplied by client doesn't match "
				"that calculated by the server.\n");
		return AUTH_PW_ERROR;
	}
	return AUTH_PW_A_OK;
}

int Condor_Auth_Passwd::server_receive_two(int *server_status, 
										   struct msg_t_buf *t_client) 
{
	int client_status = AUTH_PW_ERROR;
	char *a = NULL;
	int a_len = 0;
	unsigned char *rb = (unsigned char *)malloc(AUTH_PW_KEY_LEN);
	int rb_len = 0;
	unsigned char *hk = (unsigned char *)malloc(EVP_MAX_MD_SIZE);
	int hk_len = 0;
	
	if(!rb || !hk) {
		dprintf(D_SECURITY, "Malloc error 4.\n");
		*server_status = AUTH_PW_ABORT;
		client_status = AUTH_PW_ABORT;
		goto server_receive_two_abort;
	}
	memset(rb, 0, AUTH_PW_KEY_LEN);
	memset(hk, 0, EVP_MAX_MD_SIZE);

	if(!t_client->a || !t_client->rb) {
		dprintf(D_SECURITY, "Can't compare to null.\n");
		*server_status = client_status = AUTH_PW_ABORT;
		goto server_receive_two_abort;
	}
		// Get the data.
	mySock_->decode();
	if( !mySock_->code(client_status)
		|| !mySock_->code(a_len)
		|| !mySock_->code(a)
		|| !mySock_->code(rb_len)
		|| !(rb_len == mySock_->get_bytes(rb, rb_len))
		|| !mySock_->code(hk_len)
		|| !(hk_len == mySock_->get_bytes(hk, hk_len))
		|| !mySock_->end_of_message()) {
		dprintf(D_SECURITY, "Error communicating with client.  Aborting...\n");
		*server_status = AUTH_PW_ABORT;	
		client_status = AUTH_PW_ABORT;
		goto server_receive_two_abort;
	}

		// See if it's sane.
	if(client_status == AUTH_PW_A_OK && *server_status == AUTH_PW_A_OK) {
		if(rb_len == AUTH_PW_KEY_LEN 
		   && a && strlen(a) == strlen(t_client->a)
		   && a_len == (int)strlen(a)
		   && !strcmp(a, t_client->a) 
		   && !memcmp(rb, t_client->rb, AUTH_PW_KEY_LEN)) {
			
			t_client->hk = hk;
			t_client->hk_len = hk_len;
			free(a);
			if(rb) free(rb);
			return client_status;
		} else {
			dprintf(D_SECURITY, "Received inconsistent data.\n");
			*server_status = AUTH_PW_ERROR;
		}
	} else {
		dprintf(D_SECURITY, "Error from client.\n");
	}

	server_receive_two_abort:
		// Since we didn't move the data to the struct
	if(a) free(a);
	if(rb) free(rb);
	if(hk) free(hk);
	
	return client_status;
}

bool Condor_Auth_Passwd::calculate_hkt(msg_t_buf *t_buf, sk_buf *sk) 
{
	unsigned char *buffer;
	int prefix_len, buffer_len;

	if(t_buf->a && t_buf->b)
		dprintf(D_SECURITY, "Calculating hkt '%s' (%lu), '%s' (%lu).\n",
			t_buf->a, (unsigned long)strlen(t_buf->a),
			t_buf->b, (unsigned long)strlen(t_buf->b));
		// Assemble the buffer to be hmac'd by concatentating T in
		// buffer.  Then call hmac with ka.
	if(!t_buf->a || !t_buf->b || !t_buf->ra || !t_buf->rb) {
		dprintf(D_SECURITY, "Can't hmac NULL.\n");
		return false;
	}

		// The length of this buffer is the length of a, space, b, one
		// null, and two times the key len (ra+rb).
	prefix_len = strlen(t_buf->a) + strlen(t_buf->b) + 1;
	buffer_len = prefix_len + 1 + 2 * AUTH_PW_KEY_LEN;
	buffer = (unsigned char *)malloc(buffer_len);
	t_buf->hkt = (unsigned char *)malloc(EVP_MAX_MD_SIZE);
	if(!buffer || !t_buf->hkt) {
		dprintf(D_SECURITY, "Malloc error 5.\n");
		goto hkt_error;
	}
	if(prefix_len != sprintf((char *)buffer, "%s %s", t_buf->a, t_buf->b)) {
		dprintf(D_SECURITY, "Error copying memory.\n");
		goto hkt_error;
	}

	memcpy(buffer+prefix_len+1, t_buf->ra, AUTH_PW_KEY_LEN);
	memcpy(buffer+prefix_len+1+AUTH_PW_KEY_LEN, t_buf->rb, AUTH_PW_KEY_LEN);

		// Calculate the hmac.
	hmac( buffer, buffer_len, 
		  const_cast<unsigned char *>(sk->ka), sk->ka_len,
		  t_buf->hkt, &t_buf->hkt_len);
	if(t_buf->hkt_len < 1) {  // Maybe should be larger!
		dprintf(D_SECURITY, "Error: hmac returned zero length.\n");
		goto hkt_error;
	}
	free(buffer);
	// t_buf->hkt, allocated above, must be freed by caller if this function
	// returns true.
	return true;
 hkt_error:
	if(buffer)
		free(buffer);
	if(t_buf->hkt) {
		// make it very clear that t_buf->hkt is not valid.
		free(t_buf->hkt);
		t_buf->hkt = NULL;
		t_buf->hkt_len = 0;
	}
	return false;
}

int Condor_Auth_Passwd::server_send(int server_status, 
									struct msg_t_buf *t_server, 
									struct sk_buf *sk) 
{
	char *send_a = t_server->a;
	char *send_b = t_server->b;
	unsigned char *send_ra = t_server->ra;
	unsigned char *send_rb = t_server->rb;
	int send_a_len = 0;
	int send_b_len = 0;
	int send_ra_len = AUTH_PW_KEY_LEN;
	int send_rb_len = AUTH_PW_KEY_LEN;
	unsigned char *send_hkt = NULL;
	int send_hkt_len = 0;
	char nullstr[2];

	dprintf(D_SECURITY, "In server_send: %d.\n", server_status);

	nullstr[0] = 0;
	nullstr[1] = 0;

		// Make sure everything's ok.
	if(server_status == AUTH_PW_A_OK) {
		if(!send_a || !send_b 
		   || !send_ra || !send_rb) {
			dprintf(D_SECURITY, "Error: NULL or zero length string in T!\n");
			server_status = AUTH_PW_ERROR;
		} else {
			send_a_len = strlen(send_a);
			send_b_len = strlen(send_b);
			if(!calculate_hkt(t_server, sk)) {
				server_status = AUTH_PW_ERROR;
			}
		}
	}

		// Set what will get sent.
	if(server_status !=AUTH_PW_A_OK) {
		send_a = nullstr;
		send_b = nullstr;
		send_ra = (unsigned char *)nullstr;
		send_rb = (unsigned char *)nullstr;
		send_a_len = 0;
		send_b_len = 0;
		send_ra_len = 0;
		send_rb_len = 0;
		send_hkt = (unsigned char *)nullstr;
		send_hkt_len = 0;
	} else {
		send_hkt = t_server->hkt;
		send_hkt_len = t_server->hkt_len;
	}
	dprintf(D_SECURITY, "Server send '%s', '%s', %d %d %d\n", 
			send_a, send_b, send_ra_len, send_rb_len, send_hkt_len);

	mySock_->encode();
	if( !mySock_->code(server_status)
		|| !mySock_->code(send_a_len)
		|| !mySock_->code(send_a)
		|| !mySock_->code(send_b_len)
		|| !mySock_->code(send_b)
		|| !mySock_->code(send_ra_len)
		|| !(mySock_->put_bytes(send_ra, send_ra_len))
		|| !mySock_->code(send_rb_len)
		|| !(mySock_->put_bytes(send_rb, send_rb_len))
		|| !mySock_->code(send_hkt_len)
		|| !(mySock_->put_bytes(send_hkt, send_hkt_len))
		|| !mySock_->end_of_message()) {
		dprintf(D_SECURITY, "Error sending to client.  Aborting...\n");
		server_status = AUTH_PW_ABORT;
	}
	return server_status;
}

int Condor_Auth_Passwd :: client_receive(int *client_status, 
										 msg_t_buf *t_server) 
{
	int server_status  = AUTH_PW_ERROR;
	char *a            = (char *)malloc(AUTH_PW_MAX_NAME_LEN);
	int a_len          = 0;
	char *b            = (char *)malloc(AUTH_PW_MAX_NAME_LEN);
	int b_len          = 0;
	unsigned char *ra  = (unsigned char *)malloc(AUTH_PW_KEY_LEN);
	int ra_len         = 0;
	unsigned char *rb  = (unsigned char *)malloc(AUTH_PW_KEY_LEN);
	int rb_len         = 0;
	unsigned char *hkt = (unsigned char *)malloc(EVP_MAX_MD_SIZE);
	int hkt_len        = 0;

	if(!a || !b || !ra || !rb || !hkt) {
		dprintf(D_SECURITY, "Malloc error.  Aborting...\n");
		*client_status = AUTH_PW_ABORT;
		server_status = AUTH_PW_ABORT;
		goto client_receive_abort;
	}
	memset(ra, 0, AUTH_PW_KEY_LEN);
	memset(rb, 0, AUTH_PW_KEY_LEN);
	memset(hkt, 0, EVP_MAX_MD_SIZE);

		// Get the data
	mySock_->decode();
	if( !mySock_->code(server_status)
		|| !mySock_->code(a_len)
		|| !mySock_->get(a,AUTH_PW_MAX_NAME_LEN)
		|| !mySock_->code(b_len)
		|| !mySock_->get(b,AUTH_PW_MAX_NAME_LEN)
		|| !mySock_->code(ra_len)
		|| !(ra_len  == mySock_->get_bytes(ra, ra_len))
		|| !mySock_->code(rb_len)
		|| !(rb_len  == mySock_->get_bytes(rb, rb_len))
		|| !mySock_->code(hkt_len)
		|| !(hkt_len == mySock_->get_bytes(hkt, hkt_len))
		|| !mySock_->end_of_message()) {
		dprintf(D_SECURITY, "Error communicating with server.  Aborting...\n");
		*client_status = AUTH_PW_ABORT;
		server_status = AUTH_PW_ABORT;
		goto client_receive_abort;
	}

		// Make sure the random strings are the right size.
	if(ra_len != AUTH_PW_KEY_LEN || rb_len != AUTH_PW_KEY_LEN) {
		dprintf(D_SECURITY, "Incorrect protocol.\n");
		server_status = AUTH_PW_ERROR;
	}
		
		// Fill in the struct.
	if(server_status == AUTH_PW_A_OK) {
		t_server->a = a;
		t_server->b = b;
		t_server->ra = ra;
		dprintf(D_SECURITY, "Wrote server ra.\n");
		t_server->rb =rb;
		t_server->hkt = hkt;
		t_server->hkt_len = hkt_len;
		return server_status;
	} else {
		dprintf(D_SECURITY, "Server sent status indicating not OK.\n");
	}

 client_receive_abort:
		// If we get here, there's been an error.  Avoid leaks...
	if(a) free(a);
	if(b) free(b);
	if(ra) free(ra);
	if(rb) free(rb);
	if(hkt) free(hkt);
	return server_status;
}

int Condor_Auth_Passwd::client_send_one(int client_status, msg_t_buf *t_client)
{
	char *send_a = 0;
	unsigned char *send_b = 0;
	
	if(t_client && t_client->a) send_a = t_client->a;
	if(t_client && t_client->ra) send_b = t_client->ra;
	int send_a_len=0;
	if(send_a) send_a_len = strlen(send_a);
	int send_b_len = AUTH_PW_KEY_LEN;
	char nullstr[2];

	nullstr[0] = 0;
	nullstr[1] = 0;
	if(client_status == AUTH_PW_A_OK 
	   && (!send_a || !send_b || !send_a_len)) {
		client_status = AUTH_PW_ERROR;
		dprintf(D_SECURITY,  "Client error: NULL in send?\n");
	}

		// If there's a problem, we don't send anything.
	if(client_status != AUTH_PW_A_OK) {
		send_a = nullstr;
		send_b = (unsigned char *)nullstr;
		send_a_len = 0;
		send_b_len = 0;
	}
	dprintf(D_SECURITY, "Client sending: %d, %d(%s), %d\n", 
			client_status, send_a_len, send_a, send_b_len);

	mySock_->encode();
	if( !mySock_->code(client_status)
		|| !mySock_->code(send_a_len)
		|| !mySock_->code(send_a)
		|| !mySock_->code(send_b_len)
		|| !(send_b_len == mySock_->put_bytes(send_b, send_b_len))
		|| !(mySock_->end_of_message())) {
		dprintf(D_SECURITY, "Error sending to server (first message).  "
				"Aborting...\n");
		client_status = AUTH_PW_ABORT;
	}
	return client_status;
}

int Condor_Auth_Passwd::server_receive_one(int *server_status, 
										   struct msg_t_buf *t_client)
{
	int client_status = AUTH_PW_ERROR;
	char *a           = NULL;
	int a_len         = 0;
	unsigned char *ra = (unsigned char *)malloc(AUTH_PW_KEY_LEN);
	int ra_len        = 0;

	if(!ra) {
		dprintf(D_SECURITY, "Malloc error 6.\n");
		*server_status = AUTH_PW_ABORT;
		client_status = AUTH_PW_ABORT;
		goto server_receive_one_abort;
	}

	mySock_->decode();
	if( !mySock_->code(client_status)
		|| !mySock_->code(a_len)
		|| !mySock_->code(a) 
		|| !mySock_->code(ra_len)
		|| !(ra_len == mySock_->get_bytes(ra, ra_len))
		|| !mySock_->end_of_message()) {

		dprintf(D_SECURITY, "Error communicating with client.  Aborting...\n");
		*server_status = AUTH_PW_ABORT;
		client_status = AUTH_PW_ABORT;
		goto server_receive_one_abort;
	}
	dprintf(D_SECURITY, "Received: %d, %d(%s), %d\n", client_status, a_len, 
			a, ra_len);
		// If everything's ok, incorporate into data structure.
	if(client_status == AUTH_PW_A_OK 
	   && *server_status == AUTH_PW_A_OK) { 
		if(ra_len != AUTH_PW_KEY_LEN) {
			dprintf(D_SECURITY, "Bad length on received data: %d.\n", ra_len);
			*server_status = AUTH_PW_ERROR;
		}
	}
	if(client_status == AUTH_PW_A_OK && *server_status == AUTH_PW_A_OK) {
		t_client->a = a;
		t_client->ra = ra;
		return client_status;
	}
 server_receive_one_abort:
	if(a) free(a);
	if(ra) free(ra);
	return client_status;
}

int Condor_Auth_Passwd :: isValid() const
{
	if ( m_crypto ) {
		return TRUE;
	} else {
		return FALSE;
	}
}

bool
Condor_Auth_Passwd::set_session_key(struct msg_t_buf *t_buf, struct sk_buf *sk)
{
	unsigned char *key = (unsigned char *)malloc(EVP_MAX_MD_SIZE);
	unsigned int key_len = 0;
	
	dprintf(D_SECURITY, "Setting session key.\n");

	if(!t_buf->rb || !sk->kb || !sk->kb_len || !key) {
			// shouldn't happen
		dprintf(D_SECURITY, "Unexpected NULL.\n");
		if (key) free(key);
		return false;
	}
	
	memset(key, 0, EVP_MAX_MD_SIZE);

		// get rid of any old crypto object
	if ( m_crypto ) delete m_crypto;
	m_crypto = NULL;

		// Calculate W based on K'
	hmac( t_buf->rb, AUTH_PW_KEY_LEN,
		  const_cast<unsigned char *>(sk->kb), sk->kb_len,
		  (unsigned char *)key, &key_len );

	dprintf(D_SECURITY, "Key length: %d\n", key_len);
		// Fill the key structure.
	KeyInfo thekey(key,(int)key_len,CONDOR_3DES);
	m_crypto = new Condor_Crypt_3des(thekey);

	if ( key ) free(key);	// KeyInfo makes a copy of the key

	return m_crypto ? true : false;
}



#endif	// of if !defined(SKIP_AUTHENTICATION) && defined(HAVE_EXT_OPENSSL)
