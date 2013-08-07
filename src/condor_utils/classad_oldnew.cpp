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

#define _CONDOR_ALLOW_OPEN
#include "condor_common.h"
#include "stream.h"
#include "condor_classad.h"
#include "condor_attributes.h"
#include "my_hostname.h"
#include "string_list.h"

using namespace std;

#include "classad/classad_distribution.h"
#include "classad_oldnew.h"
#include "compat_classad.h"


static bool publish_server_timeMangled = false;
void AttrList_setPublishServerTimeMangled( bool publish)
{
    publish_server_timeMangled = publish;
}

static const char *SECRET_MARKER = "ZKM"; // "it's a Zecret Klassad, Mon!"

classad::ClassAd *
getClassAd( Stream *sock )
{
	classad::ClassAd *ad = new classad::ClassAd( );
	if( !ad ) { 
		return NULL;
	}
	if( !getClassAd( sock, *ad ) ) {
		delete ad;
		return NULL;
	}
	return ad;	
}

bool
getClassAd( Stream *sock, classad::ClassAd& ad )
{
	int 					numExprs;
	MyString				inputLine;

	ad.Clear( );

		// Reinsert CurrentTime, emulating the special version in old
		// ClassAds
	if ( !compat_classad::ClassAd::m_strictEvaluation ) {
		ad.Insert( ATTR_CURRENT_TIME " = time()" );
	}

	sock->decode( );
	if( !sock->code( numExprs ) ) {
 		return false;
	}

		// pack exprs into classad
	for( int i = 0 ; i < numExprs ; i++ ) {
		char const *strptr = NULL;
		string buffer;
		if( !sock->get_string_ptr( strptr ) || !strptr ) {
			return( false );	 
		}		

		if(strcmp(strptr,SECRET_MARKER) ==0 ){
			char *secret_line = NULL;
			if( !sock->get_secret(secret_line) ) {
				dprintf(D_FULLDEBUG, "Failed to read encrypted ClassAd expression.\n");
				break;
			}
			compat_classad::ConvertEscapingOldToNew(secret_line,buffer);
			free( secret_line );
		}
		else {
			compat_classad::ConvertEscapingOldToNew(strptr,buffer);
		}

		// inserts expression at a time
		if ( !ad.Insert(buffer) )
		{
		  dprintf(D_FULLDEBUG, "FAILED to insert %s\n", buffer.c_str() );
		  return false;
		}
		
	}

		// get type info
	if (!sock->get(inputLine)) {
		 dprintf(D_FULLDEBUG, "FAILED to get(inputLine)\n" );
		return false;
	}
	if (inputLine != "" && inputLine != "(unknown type)") {
		if (!ad.InsertAttr("MyType",(string)inputLine.Value())) {
			dprintf(D_FULLDEBUG, "FAILED to insert MyType\n" );
			return false;
		}
	}

	if (!sock->get(inputLine)) {
		 dprintf(D_FULLDEBUG, "FAILED to get(inputLine) 2\n" );
		return false;
	}
	if (inputLine != "" && inputLine != "(unknown type)") {
		if (!ad.InsertAttr("TargetType",(string)inputLine.Value())) {
		dprintf(D_FULLDEBUG, "FAILED to insert TargetType\n" );
			return false;
		}
	}

	return true;
}

bool
getClassAdNoTypes( Stream *sock, classad::ClassAd& ad )
{
	classad::ClassAdParser	parser;
	int 					numExprs = 0; // Initialization clears Coverity warning
	string					buffer;
	classad::ClassAd		*upd=NULL;
	MyString				inputLine;


	ad.Clear( );

		// Reinsert CurrentTime, emulating the special version in old
		// ClassAds
	if ( !compat_classad::ClassAd::m_strictEvaluation ) {
		ad.Insert( ATTR_CURRENT_TIME " = time()" );
	}

	sock->decode( );
	if( !sock->code( numExprs ) ) {
 		return false;
	}

		// pack exprs into classad
	buffer = "[";
	for( int i = 0 ; i < numExprs ; i++ ) {
		if( !sock->get( inputLine ) ) { 
			return( false );	 
		}		

        if(strcmp(inputLine.Value(),SECRET_MARKER) ==0 ){
            char *secret_line = NULL;
            if( !sock->get_secret(secret_line) ) {
                dprintf(D_FULLDEBUG, "Failed to read encrypted ClassAd expression.\n");
                break;
            }
	    inputLine = secret_line;
	    free( secret_line );
        }

		if ( strncmp( inputLine.Value(), "ConcurrencyLimit.", 17 ) == 0 ) {
			inputLine.setChar( 16, '_' );
		}
		buffer += string(inputLine.Value()) + ";";
	}
	buffer += "]";

		// parse ad
	if( !( upd = parser.ParseClassAd( buffer ) ) ) {
		return( false );
	}

		// put exprs into ad
	ad.Update( *upd );
	delete upd;

	return true;
}

/* 
 * It now prints chained attributes. Or it should.
 * 
 * It should add the server_time attribute if it's
 * defined.
 *
 * It should convert default IPs to SocketIPs.
 *
 * It should also do encryption now.
 */

bool putClassAd ( Stream *sock, classad::ClassAd& ad, bool exclude_private, StringList *attr_whitelist )
{
    bool completion;
    completion = _putClassAd(sock, ad, false, exclude_private, attr_whitelist);


    //should be true by this point
	return completion;
}

bool
putClassAdNoTypes ( Stream *sock, classad::ClassAd& ad, bool exclude_private )
{
    return _putClassAd(sock, ad, true, exclude_private, NULL);
}

bool _putClassAd( Stream *sock, classad::ClassAd& ad, bool excludeTypes,
					 bool exclude_private, StringList *attr_whitelist )
{
	classad::ClassAdUnParser	unp;
	std::string					buf;
    bool send_server_time = false;

	unp.SetOldClassAd( true, true );

	int numExprs=0;

    classad::AttrList::const_iterator itor;
    classad::AttrList::const_iterator itor_end;

    bool haveChainedAd = false;
    
    classad::ClassAd *chainedAd = ad.GetChainedParentAd();
    
    if(chainedAd){
        haveChainedAd = true;
    }

	if( attr_whitelist ) {
		numExprs += attr_whitelist->number();
	}
	else for(int pass = 0; pass < 2; pass++){

        /* 
        * Count the number of chained attributes on the first
        *   pass (if any!), then the number of attrs in this classad on
        *   pass number 2.
        */
        if(pass == 0){
            if(!haveChainedAd){
                continue;
            }
            itor = chainedAd->begin();
			itor_end = chainedAd->end();
        }
        else {
            itor = ad.begin();
			itor_end = ad.end();
        }

		for(;itor != itor_end; itor++) {
			std::string const &attr = itor->first;

            if(!exclude_private ||
			   !compat_classad::ClassAdAttributeIsPrivate(attr.c_str()))
            {
                if(excludeTypes)
                {
                    if(strcasecmp( ATTR_MY_TYPE, attr.c_str() ) != 0 &&
                        strcasecmp( ATTR_TARGET_TYPE, attr.c_str() ) != 0)
                    {
                        numExprs++;
                    }
                }
                else { numExprs++; }
            }
			if ( strcasecmp( ATTR_CURRENT_TIME, attr.c_str() ) == 0 ) {
				numExprs--;
			}
        }
    }

    if( publish_server_timeMangled ){
        //add one for the ATTR_SERVER_TIME expr
        numExprs++;
        send_server_time = true;
    }

	sock->encode( );
	if( !sock->code( numExprs ) ) {
		return false;
	}
    
	if( attr_whitelist ) {
		attr_whitelist->rewind();
		char const *attr;
		while( (attr=attr_whitelist->next()) ) {
			classad::ExprTree const *expr = ad.Lookup(attr);
			buf = attr;
			buf += " = ";
            if( !expr || (exclude_private && compat_classad::ClassAdAttributeIsPrivate(attr)) )
			{
				buf += "undefined";
            }
			else {
				unp.Unparse( buf, expr );
			}
            ConvertDefaultIPToSocketIP(attr,buf,*sock);

            if( ! sock->prepare_crypto_for_secret_is_noop() &&
				compat_classad::ClassAdAttributeIsPrivate(attr) )
			{
                sock->put(SECRET_MARKER);

                sock->put_secret(buf.c_str());
            }
            else if (!sock->put(buf.c_str()) ){
                return false;
            }
		}
	}
    else for(int pass = 0; pass < 2; pass++){
        if(pass == 0) {
            /* need to copy the chained attrs first, so if
             *  there are duplicates, the non-chained attrs
             *  will override them
             */
            if(!haveChainedAd){
                continue;
            }
            itor = chainedAd->begin();
			itor_end = chainedAd->end();
        } 
        else {
            itor = ad.begin();
			itor_end = ad.end();
        }

		for(;itor != itor_end; itor++) {
			std::string const &attr = itor->first;
			classad::ExprTree const *expr = itor->second;

			if(strcasecmp(ATTR_CURRENT_TIME,attr.c_str())==0) {
				continue;
			}
            if(exclude_private && compat_classad::ClassAdAttributeIsPrivate(attr.c_str())){
                continue;
            }

            if(excludeTypes){
                if(strcasecmp( ATTR_MY_TYPE, attr.c_str( ) ) == 0 || 
				   strcasecmp( ATTR_TARGET_TYPE, attr.c_str( ) ) == 0 )
				{
                    continue;
                }
            }

			buf = attr;
            buf += " = ";
            unp.Unparse( buf, expr );

            ConvertDefaultIPToSocketIP(attr.c_str(),buf,*sock);

            if( ! sock->prepare_crypto_for_secret_is_noop() &&
				compat_classad::ClassAdAttributeIsPrivate(attr.c_str()))
			{
                sock->put(SECRET_MARKER);

                sock->put_secret(buf.c_str());
            }
            else if (!sock->put(buf.c_str()) ){
                return false;
            }
        }
    }

    if(send_server_time) {
        //insert in the current time from the server's (Schedd) point of
        //view. this is used so condor_q can compute some time values 
        //based upon other attribute values without worrying about 
        //the clocks being different on the condor_schedd machine
        // -vs- the condor_q machine

        char* serverTimeStr;
        serverTimeStr = (char *) malloc(strlen(ATTR_SERVER_TIME)
                                        + 3     //for " = "
                                        + 12    // for integer
                                        +1);    //for null termination
        ASSERT( serverTimeStr );
        sprintf(serverTimeStr, "%s = %ld", ATTR_SERVER_TIME, (long)time(NULL) );
        if(!sock->put(serverTimeStr)){
            free(serverTimeStr);
            return 0;
        }
        free(serverTimeStr);
    }

    //ok, so the name of the bool doesn't really work here. It works
    //  in the other places though.
    if(!excludeTypes)
    {
        // Send the type
        if (!ad.EvaluateAttrString(ATTR_MY_TYPE,buf)) {
            buf="";
        }
        if (!sock->put(buf.c_str())) {
            return false;
        }

        if (!ad.EvaluateAttrString(ATTR_TARGET_TYPE,buf)) {
            buf="";
        }
        if (!sock->put(buf.c_str())) {
            return false;
        }
    }

	return true;
}


bool EvalTree(classad::ExprTree* eTree, classad::ClassAd* mine, classad::Value* v)
{
    return EvalTree(eTree, mine, NULL, v);
}


bool EvalTree(classad::ExprTree* eTree, classad::ClassAd* mine, classad::ClassAd* target, classad::Value* v)
{
    if(!mine)
    {
        return false;
    }
    const classad::ClassAd* tmp = eTree->GetParentScope(); 
    eTree->SetParentScope(mine);

    if(target)
    {
        classad::MatchClassAd mad(mine,target);

        bool rval = eTree->Evaluate(*v);

        mad.RemoveLeftAd( );
        mad.RemoveRightAd( );
        
        //restore the old scope
        eTree->SetParentScope(tmp);

        return rval;
    }


    //restore the old scope
    eTree->SetParentScope(tmp);

    return eTree->Evaluate(*v);
}
