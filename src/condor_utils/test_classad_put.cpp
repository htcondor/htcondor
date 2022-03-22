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

/* This is a horribly done "unit test" for the putOldClassAd() found in
 *   classad_oldnew.(cpp/h). 
 * It uses a copied over version of putOldClassAd() because 
 *  trying to implement Stream gave me too many problems. So the code 
 *  might be old. In the future I might come back and do this properly.
 */

#include "condor_common.h"
#include "condor_attributes.h"

#include "classad/classad_distribution.h"
#include "classad_oldnew.h"
#include "compat_classad.h"

#include "classad/sink.h"

#include "my_hostname.h"
#include "stream.h"
#include <stdio.h>
#include <stdlib.h>
using namespace std;

//{{{ DummyStream
class DummyStream 
{

    public:
        DummyStream();

        std::vector<char*>::iterator getItr();
        void emptyVec();
        void printContents();

        ~DummyStream(){emptyVec();};

        void encode();

        int code( int blah );

        int put(char *str);
        int put(const char*);
        int put(int i);

    private:
        std::vector<char*> testVec;
};

DummyStream::DummyStream() 
{
}

void DummyStream::encode()
{
    //noop!
    return;
}

int DummyStream::code( int blah)
{
    put(blah); 
    return 1;
}

int DummyStream::put(char *str)
{
    char* foo = new char[strlen(str) + 1];
    strncpy(foo, str, strlen(str) + 1);
    testVec.push_back(foo);
    return 1;
}

int DummyStream::put(const char *str)
{
    char* foo = new char[strlen(str) + 1];
    strncpy(foo, str, strlen(str) + 1);
    testVec.push_back(foo);
    return 1;
}


int DummyStream::put(int i)
{
    char *tmp = new char[16];
    sprintf(tmp, "%d", i);
    put(tmp);
    //testVec.push_back(tmp);
    delete[] tmp;
    return 1;
}

std::vector<char*>::iterator DummyStream::getItr()
{
    return testVec.begin();
}

void DummyStream::emptyVec()
{
    std::vector<char*>::iterator itr;
    for(itr = testVec.begin(); itr < testVec.end(); itr++)
    {
        delete[] *itr;
    }
    testVec.clear();
}

void DummyStream::printContents()
{
    std::vector<char*>::iterator itr;

    printf("Size of vec: %d. Printing contents.\n", testVec.size() );

    for(itr = testVec.begin(); itr < testVec.end(); itr++)
    {
        printf("%s\n", *itr);
    }

    printf("Done printing contents.\n");
}
//}}}

static bool publish_server_timeMangled = false;
void AttrList_setPublishServerTimeMangled2( bool publish)
{
    publish_server_timeMangled = publish;
}

//{{{ putOldClassAd
/* need to free exprString every time. Whatever put() does can deal with it
 *
 */
bool putOldClassAd ( DummyStream *sock, classad::ClassAd& ad, bool excludeTypes )
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


            if(!ClassAdAttributeIsPrivate(buf))
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

            if(ClassAdAttributeIsPrivate(buf)){
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
            /*
            if( ! sock->prepare_crypto_for_secret_is_noop() &&
                    ClassAdAttributeIsPrivate(tmpAttrName)) {
                sock->put(SECRET_MARKER);

                sock->put_secret(exprString);
            }
            else */ if (!sock->put(exprString) ){
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

    //ok, so we're not really excluding it here, but...
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
//}}}

char *classad_strings[] = 
{
    "A = 1\n B = 2",
    "A = 1\n B = 3",
    "B = 1241\n C = 3\n D = 4"
};

//{{{setUpClassAds()
void setUpClassAds(ClassAd* c1, ClassAd* c2, ClassAd* c3, FILE* c1FP,
                    FILE* c2FP, FILE* c3FP, bool verbose)
{
    if(verbose)
        printf("Creating ClassAds\n");

	c1 = new ClassAd;
	c2 = new ClassAd;
	c3 =  new ClassAd;
    initAdFromString(classad_strings[0], *c1);
    initAdFromString(classad_strings[1], *c2);
    initAdFromString(classad_strings[2], *c3);
    SetMyTypeName(*c1, "c1");
    SetMyTypeName(*c2, "c2");
    SetMyTypeName(*c3, "c3");

    SetTargetTypeName(*c1, "not c1!");
    SetTargetTypeName(*c2, "not c2!");
    SetTargetTypeName(*c3, "not c3!");

    if(verbose)
    {
        printf("C1:\n"); fPrintAd(stdout, *c1); printf("\n");
        printf("C2:\n"); fPrintAd(stdout, *c2); printf("\n");
        printf("C3:\n"); fPrintAd(stdout, *c3); printf("\n");
    }

    //ugh, converting old classads into ClassAds, in probably the 
    //  worst possible way. Ever. But it should work.
    c1FP = fopen("c1FP.txt", "w+");
    c2FP = fopen("c2FP.txt", "w+");
    c3FP = fopen("c3FP.txt", "w+");

    fPrintAd(c1FP, *c1);
    fPrintAd(c2FP, *c2);
    fPrintAd(c3FP, *c3);

    fclose(c1FP); fclose(c2FP); fclose(c3FP);
}
//}}}

//{{{setUpCompatClassAd()
void setUpCompatClassAds(ClassAd** compC1, ClassAd** compC2,
						 ClassAd** compC3, FILE* c1FP, FILE* c2FP, FILE* c3FP,
        bool verbose)
{
    c1FP = fopen("c1FP.txt", "r+");
    c2FP = fopen("c2FP.txt", "r+");
    c3FP = fopen("c3FP.txt", "r+");

    if(verbose)
        printf("creating compatclassads\n");

    int eofCheck, errorCheck, emptyCheck; 
    (*compC1) = new ClassAd(c1FP, ",", eofCheck, errorCheck, emptyCheck); 
    (*compC2)= new ClassAd(c2FP, ",", eofCheck, errorCheck, emptyCheck); 
    (*compC3) = new ClassAd(c3FP, ",", eofCheck, errorCheck, emptyCheck); 
    fclose(c1FP); fclose(c2FP); fclose(c3FP);

    SetMyTypeName(*(*compC1), "compC1");
    SetMyTypeName(*(*compC2), "compC2");
    SetMyTypeName(*(*compC3), "compC3");

    SetTargetTypeName(*(*compC1), "not compC1!");
    SetTargetTypeName(*(*compC2), "not compC2!");
    SetTargetTypeName(*(*compC3), "not compC3!");


}
//}}}

//{{{ test_put_server_time
bool test_put_server_time(bool verbose)
{
    DummyStream ds;

    bool failed = false; 
    ClassAd *c1, *c2, *c3;
    FILE* c1FP, *c2FP, *c3FP;

    setUpClassAds(c1, c2, c3, c1FP, c2FP, c3FP,verbose);

	ClassAd *compC1, *compC2, *compC3;

    setUpCompatClassAds(&compC1, &compC2, &compC3, c1FP, c2FP, c3FP,verbose);

    if(verbose)
    {
        printf("Comp1:\n"); fPrintAd(stdout, *compC1); printf("\n");
        printf("Comp2:\n"); fPrintAd(stdout, *compC2); printf("\n");
        printf("Comp3:\n"); fPrintAd(stdout, *compC3); printf("\n");
    
        printf("----------------\n\n");
    }
    /*

    printf("Putting CompClassAd1\n");
    ds.emptyVec();

    AttrList_setPublishServerTimeMangled2(true);
    putOldClassAd(&ds, *compC1, false);
    ds.printContents();
    ds.emptyVec();

    printf("\nPutting CompClassAd2\n");
    AttrList_setPublishServerTimeMangled2(false);
    putOldClassAd(&ds, *compC2, false);
    ds.printContents();
    ds.emptyVec();

    printf("\nTurning server_time on\n");
    AttrList_setPublishServerTimeMangled2(true);

    printf("Putting CompClassAd3\n");
    putOldClassAd(&ds, *compC3, false);
    ds.printContents();
    ds.emptyVec();
    */

    printf("Trying out excludeTypes = false\n");
    putOldClassAd(&ds, *compC3, false);
    ds.printContents();
    ds.emptyVec();

    printf("Trying out excludeTypes = true\n");
    putOldClassAd(&ds, *compC3, true);
    ds.printContents();
    ds.emptyVec();

    return failed;

}
//}}}
//{{{ test_put_chained_ads
bool test_put_chained_ads(bool verbose)
{

    DummyStream ds;

    bool failed = false;

    ClassAd *c1, *c2, *c3;
    FILE* c1FP, *c2FP, *c3FP;

    setUpClassAds(c1, c2, c3, c1FP, c2FP, c3FP, verbose);

	ClassAd *compC1, *compC2, *compC3;

    setUpCompatClassAds(&compC1, &compC2, &compC3, c1FP, c2FP, c3FP,verbose);

    compC1->ChainToAd(compC3);
    compC3->ChainToAd(compC2);

    if(verbose)
    {
        printf("Comp1:\n"); fPrintAd(stdout, *compC1); printf("\n");
        printf("Comp2:\n"); fPrintAd(stdout, *compC2); printf("\n");
        printf("Comp3:\n"); fPrintAd(stdout, *compC3); printf("\n");

        printf("Putting CompClassAd1\n");
    }
    ds.emptyVec();

    AttrList_setPublishServerTimeMangled2(true);
    putOldClassAd(&ds, *compC1, false);
    ds.printContents();
    ds.emptyVec();

    printf("\nPutting CompClassAd2\n");
    AttrList_setPublishServerTimeMangled2(false);
    putOldClassAd(&ds, *compC2, false);
    ds.printContents();
    ds.emptyVec();

    printf("\nTurning server_time on\n");
    AttrList_setPublishServerTimeMangled2(true);

    printf("Putting CompClassAd3\n");
    putOldClassAd(&ds, *compC3, false);
    ds.printContents();
    ds.emptyVec();

    
    return failed;
}
//}}}

int main(int argc, char **argv)
{
    bool verbose;

    if(argc > 1 && !strcmp(argv[1], "-v"))
    {
        verbose = true;
    }
    else
    {
        verbose = false;
    }

    printf("testing server time\n");
    test_put_server_time(verbose);
    printf("Server time complete.\n-----------------\nTesting chained ads.\n");
    
    //test_put_chained_ads(verbose);

    printf("chained ads complete.\n-----------------\n");
}
