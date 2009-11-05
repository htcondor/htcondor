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

using namespace std;

#define WANT_CLASSAD_NAMESPACE
#include "classad/classad_distribution.h"
#include "classad_oldnew.h"
#include "conversion.h"
#include "compat_classad.h"


static bool publish_server_timeMangled = false;
void AttrList_setPublishServerTimeMangled( bool publish)
{
    publish_server_timeMangled = publish;
}

static const char *SECRET_MARKER = "ZKM"; // "it's a Zecret Klassad, Mon!"

classad::ClassAd *
getOldClassAd( Stream *sock )
{
	classad::ClassAd *ad = new classad::ClassAd( );
	if( !ad ) { 
		return NULL;
	}
	if( !getOldClassAd( sock, *ad ) ) {
		delete ad;
		return NULL;
	}
	return ad;	
}

bool
getOldClassAd( Stream *sock, classad::ClassAd& ad )
{
	classad::ClassAdParser	parser;
	int 					numExprs;
	string					buffer;
	char					*tmp;
	classad::ClassAd		*upd=NULL;
	static char				*inputLine = new char[ 10240 ];


	ad.Clear( );
	sock->decode( );
	if( !sock->code( numExprs ) ) {
 		return false;
	}

    char *secret_line = NULL;

		// pack exprs into classad
	buffer = "[";
	for( int i = 0 ; i < numExprs ; i++ ) {
		tmp = NULL;
		if( !sock->get( inputLine ) ) { 
			return( false );	 
		}		

        if(strcmp(tmp,SECRET_MARKER) ==0 ){
            free(secret_line);
            secret_line = NULL;
            if( !sock->get_secret(secret_line) ) {
                dprintf(D_FULLDEBUG, "Failed to read encrypted ClassAd expression.\n");
                break;
            }
        }

		buffer += string(inputLine) + ";";
		free( tmp );
	}
	buffer += "]";
    free(secret_line);

		// get type info
	if (!sock->get(inputLine)||!ad.InsertAttr("MyType",(string)inputLine)) {
		return false;
	}
	if (!sock->get(inputLine)|| !ad.InsertAttr("TargetType",
											   (string)inputLine)) {
		return false;
	}

		// parse ad
	if( !( upd = parser.ParseClassAd( buffer ) ) ) {
		return( false );
	}

		// put exprs into ad
	classad::ClassAd *tmpAd = new classad::ClassAd( );
	tmpAd->Update( *upd );
	classad::ClassAd *tmpAd2 = AddExplicitTargets( tmpAd );
	ad.Update( *tmpAd );
	delete tmpAd;
	delete tmpAd2;
	delete upd;

	return true;
}

bool
getOldClassAdNoTypes( Stream *sock, classad::ClassAd& ad )
{
	classad::ClassAdParser	parser;
	int 					numExprs = 0; // Initialization clears Coverity warning
	string					buffer;
	char					*tmp;
	classad::ClassAd		*upd=NULL;
	static char				*inputLine = new char[ 10240 ];


	ad.Clear( );
	sock->decode( );
	if( !sock->code( numExprs ) ) {
 		return false;
	}

    char *secret_line = NULL;
		// pack exprs into classad
	buffer = "[";
	for( int i = 0 ; i < numExprs ; i++ ) {
		tmp = NULL;
		if( !sock->get( inputLine ) ) { 
			return( false );	 
		}		

        if(strcmp(tmp,SECRET_MARKER) ==0 ){
            free(secret_line);
            secret_line = NULL;
            if( !sock->get_secret(secret_line) ) {
                dprintf(D_FULLDEBUG, "Failed to read encrypted ClassAd expression.\n");
                break;
            }
        }

		buffer += string(inputLine) + ";";
		free( tmp );
	}
	buffer += "]";
    free(secret_line);

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

bool putOldClassAd ( Stream *sock, classad::ClassAd& ad )
{
    bool completion;
    completion = _putOldClassAd(sock, ad, false);


    //should be true by this point
	return completion;
}

bool
putOldClassAdNoTypes ( Stream *sock, classad::ClassAd& ad )
{
    return _putOldClassAd(sock, ad, true);
}

bool _putOldClassAd( Stream *sock, classad::ClassAd& ad, bool excludeTypes )
{
	classad::ClassAdUnParser	unp;
	string						buf;
	const classad::ExprTree		*expr;
    bool send_server_time = false;

	int numExprs=0;

	classad::ClassAdIterator itor(ad);

    bool haveChainedAd = false;
    
    classad::ClassAd *chainedAd = ad.GetChainedParentAd();
    
    if(chainedAd){
        haveChainedAd = true;
    }

    for(int pass = 0; pass < 2; pass++){

        /* 
        * Count the number of chained attributes on the first
        *   pass (if any!), then the number of attrs in this classad on
        *   pass number 2.
        */
        if(pass == 0){
            if(!haveChainedAd){
                continue;
            }
            itor.Initialize(*chainedAd);
        }
        else {
            itor.Initialize(ad);
        }

        while( !itor.IsAfterLast( ) ) {
            itor.CurrentAttribute( buf, expr );


            if(!compat_classad::ClassAd::ClassAdAttributeIsPrivate(buf.c_str()))
            {
                if(excludeTypes)
                {
                    if(strcasecmp( "MyType", buf.c_str() ) != 0 &&
                        strcasecmp( "TargetType", buf.c_str() ) != 0)
                    {
                        numExprs++;
                    }
                }
                else { numExprs++; }
            }
            itor.NextAttribute( buf, expr );
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
    
    classad::ClassAdIterator attrItor; 
    for(int pass = 0; pass < 2; pass++){
        if(pass == 0) {
            /* need to copy the chained attrs first, so if
             *  there are duplicates, the non-chained attrs
             *  will override them
             */
            if(!haveChainedAd){
                continue;
            }
            attrItor.Initialize(*chainedAd);
        } 
        else {
            attrItor.Initialize(ad);
        }

        char *exprString;
        for( attrItor.ToFirst();
            !attrItor.IsAfterLast();
            attrItor.NextAttribute(buf, expr) ) {

            attrItor.CurrentAttribute( buf, expr );

            //yea, these two if-statements could be combined into one, 
            //but this is more readable.
            if(compat_classad::ClassAd::ClassAdAttributeIsPrivate(buf.c_str())){
                continue;
            }

            if(excludeTypes){
                if(strcasecmp( "MyType", buf.c_str( ) ) == 0 || 
                        strcasecmp( "TargetType", buf.c_str( ) ) == 0 ){
                    continue;
                }
            }
            //store the name for later
            string tmpAttrName(buf);
            buf += " = ";
            unp.Unparse( buf, expr );
            
            //get buf's c_str in an editable format
            exprString = (char*)malloc(buf.size() + 1);
            strncpy(exprString, buf.c_str(),buf.size() + 1 ); 
            ConvertDefaultIPToSocketIP(tmpAttrName.c_str(),&exprString,*sock);
            if( ! sock->prepare_crypto_for_secret_is_noop() &&
				compat_classad::ClassAd::ClassAdAttributeIsPrivate(tmpAttrName.c_str())) {
                sock->put(SECRET_MARKER);

                sock->put_secret(exprString);
            }
            else if (!sock->put(exprString) ){
                free(exprString);
                return false;
            }
            free(exprString);
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
        sprintf(serverTimeStr, "%s = %ld", ATTR_SERVER_TIME, (long)time(NULL) );
        if(!sock->put(serverTimeStr)){
            free(serverTimeStr);
            return 0;
        }
        free(serverTimeStr);
    }

    //ok, so the name of the bool doesn't really work here. It works
    //  in the other places though.
    if(excludeTypes)
    {
        // Send the type
        if (!ad.EvaluateAttrString("MyType",buf)) {
            buf="(unknown type)";
        }
        if (!sock->put(buf.c_str())) {
            return false;
        }

        if (!ad.EvaluateAttrString("TargetType",buf)) {
            buf="(unknown type)";
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
