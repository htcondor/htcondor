#include "keys.h"
#include "base.h"
#include <cmath>
#include <algorithm>
#include <iostream>

char* cfc::sha1_file_keygen( FILE* fp)
{
    EVP_MD_CTX mdctx;
    const EVP_MD *md;
    unsigned char *md_value;
    unsigned int md_len;
    int i;
    char digit[10];
    char* tmp;
    char* digest;
    char chunk;

    md_value = (unsigned char *) malloc (EVP_MAX_MD_SIZE);

    OpenSSL_add_all_digests ();

    md = EVP_get_digestbyname ("sha1");

    EVP_MD_CTX_init (&mdctx);
    EVP_DigestInit_ex (&mdctx, md, NULL);

    rewind( fp);
    chunk = fgetc(fp);
	while( !feof(fp) )
		{
			EVP_DigestUpdate (&mdctx, &chunk, 1);			
			chunk = fgetc(fp);
		}
	rewind( fp);

    EVP_DigestFinal_ex (&mdctx, md_value, &md_len);
    EVP_MD_CTX_cleanup (&mdctx);

    digest = (char *) malloc (KEY_SIZE / BASE_B + 1);

    tmp = digest;
    *tmp = '\0';
    for (i = 0; i < md_len; i++)
	{
	    if (int(pow(2, BASE_B)) == BASE_16)
		{
		    convert_base16 (md_value[i], digit);
		}
	    else if (int(pow(2, BASE_B)) == BASE_4)
		{
		    convert_base4 (md_value[i], digit);
		}
	    else if (int(pow(2, BASE_B)) == BASE_2)
		{
		    convert_base2 (md_value[i], digit);
		}

	    strcat (tmp, digit);
	    tmp = tmp + strlen (digit);
	}

    free (md_value);

    tmp = '\0';
    return (digest);
}


void cfc::file_make_hash( Key* key, FILE* fp)
{
    char *digest;

    digest = sha1_file_keygen (fp);
    str_to_key (digest, key);

    free(digest);
}

bool key_sort( Key k1, Key k2)
{

    int i;

    unsigned long ka, kb;

    //std::cerr<< get_key_string(&k1) << " < " << get_key_string(&k2) << std::endl;

    for (i = 4; i >= 0; i--)
	{
        ka = k1.t[i] & 0xffffffff;
        kb = k1.t[i] & 0xffffffff;

        //std::cerr << std::hex << ka << " < " << kb << " = ";

	    if (ka > kb)
            {
                //std::cerr << 0 << std::endl;
                return (false);
            }
	    else if (ka < kb)
            {
                //std::cerr << 1 << std::endl;
                return (true);
            }
        //std::cerr << 0 << std::endl;
	}
    return (false);

}


void cfc::keysetDiff( KeySet ksa, KeySet ksb, KeySet & ksu, KeySet & ksda, KeySet & ksdb )
{
    KeyIter end_range;

    std::sort( ksa.begin(), ksa.end(), key_sort );
    std::sort( ksb.begin(), ksb.end(), key_sort );

    ksu.clear();
    ksu.resize( ksa.size() + ksb.size() );

    ksda.clear();
    ksda.resize( ksa.size() );

    ksdb.clear();
    ksdb.resize( ksb.size() );

    std::cerr << "KSA SIZE: " << ksa.size() << std::endl;
    std::cerr << "KSB SIZE: " << ksb.size() << std::endl;
    std::cerr << "KSU SIZE: " << ksu.size() << std::endl;
    std::cerr << "KSDA SIZE: " << ksda.size() << std::endl;
    std::cerr << "KSDB SIZE: " << ksdb.size() << std::endl;

    end_range = std::set_union( ksa.begin(), ksa.end(), ksb.begin(), ksb.end(),
                    ksu.begin(), key_sort );

    ksu.resize( end_range - ksu.begin());



   
    end_range = std::set_difference( ksa.begin(), ksa.end(), ksb.begin(), ksb.end(),
                         ksda.begin(), key_sort );

    ksda.resize( end_range - ksda.begin());




    end_range = std::set_difference( ksb.begin(), ksb.end(), ksa.begin(), ksa.end(),
                         ksdb.begin(), key_sort );    

    ksdb.resize( end_range - ksdb.begin());


    std::cerr << "KSA SIZE: " << ksa.size() << std::endl;
    std::cerr << "KSB SIZE: " << ksb.size() << std::endl;
    std::cerr << "KSU SIZE: " << ksu.size() << std::endl;
    std::cerr << "KSDA SIZE: " << ksda.size() << std::endl;
    std::cerr << "KSDB SIZE: " << ksdb.size() << std::endl;

}



