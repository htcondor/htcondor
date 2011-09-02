#ifndef _CFC_TOOL_KEYS_H_
#define _CFC_TOOL_KEYS_H_

//Using the key structure created for chimera's operation
#include <key.h>

#include <vector>

namespace cfc
{
    char* sha1_file_keygen( FILE* fp);
    void file_make_hash( Key* key, FILE* fp);

    typedef std::vector<Key> KeySet;
    typedef std::vector<Key>::iterator KeyIter;


    void keysetDiff( KeySet ksa, KeySet ksb, KeySet & ksu, KeySet & ksda, KeySet & ksdb );

};

#endif
