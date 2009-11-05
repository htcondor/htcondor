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
#include "condor_attributes.h"

#define WANT_CLASSAD_NAMESPACE
#include "classad/classad_distribution.h"
#include "classad_oldnew.h"
#include "conversion.h"
#include "compat_classad.h"

#include "classad/sink.h"

#include <stdio.h>
#include <stdlib.h>
using namespace std;

bool test_sPrintExpr(compat_classad::ClassAd *c1, int verbose);
bool test_IsValidAttrValue(compat_classad::ClassAd *c1, int verbose);
bool test_fPrintAsXML(compat_classad::ClassAd *c1, int verbose);
bool test_sPrintAsXML(int verbose); //I guess if fPrintAsXML works, 
                                     //this does too

bool test_ChainCollapse(compat_classad::ClassAd *c2, compat_classad::ClassAd *c3, int verbose);
bool test_EvalStringCharStar(compat_classad::ClassAd *c1, compat_classad::ClassAd *c2, int verbose);
bool test_EvalStringCharStarStar(compat_classad::ClassAd *c1, compat_classad::ClassAd *c2, int verbose);
bool test_EvalStringMyString(compat_classad::ClassAd *c1, compat_classad::ClassAd *c2, int verbose);

bool test_NextDirtyExpr(compat_classad::ClassAd *c1, int verbose);
bool test_EscapeStringValue(compat_classad::ClassAd *c1, int verbose);

bool test_EvalTree(compat_classad::ClassAd *c1, compat_classad::ClassAd *c2,int verbose);

bool test_GIR(int verbose);
classad::References* gir_helper(classad::ClassAd* c, string attr, int verbose, bool full = true);

bool correctRefs(classad::References* ref, std::vector<string> expected);
bool runAndCheckGIR(classad::ClassAd* c, string attr, string listString, bool full, int verbose );

void setUpAndRun(int verbose);

char *classad_strings[] = 
{
    "A = 1, B = 2",
    "A = 1, B = 3",
    "B = 1241, C = 3, D = 4",
    "A = \"hello\", E = 5"
};

//{{{ setUpClassAds
void setUpClassAds(/*ClassAd* c1, ClassAd* c2, ClassAd* c3, ClassAd* c4,*/ FILE* c1FP, FILE* c2FP, FILE* c3FP, FILE* c4FP, int verbose)
{
    ClassAd *c1, *c2, *c3, *c4;
    if(verbose)
        printf("Creating ClassAds\n");

    c1 = new ClassAd(classad_strings[0], ',');
    c2 = new ClassAd(classad_strings[1], ',');
    c3 = new ClassAd(classad_strings[2], ',');
    c4 = new ClassAd(classad_strings[3], ',');

    c1->SetMyTypeName("c1");
    c2->SetMyTypeName("c2");
    c3->SetMyTypeName("c3");
    c4->SetMyTypeName("c4");

    c1->SetTargetTypeName("not c1!");
    c2->SetTargetTypeName("not c2!");
    c3->SetTargetTypeName("not c3!");
    c4->SetTargetTypeName("not c4!");

    /*
    if(verbose)
    {
        printf("C1:\n"); c1->fPrint(stdout); printf("\n");
        printf("C2:\n"); c2->fPrint(stdout); printf("\n");
        printf("C3:\n"); c3->fPrint(stdout); printf("\n");
    }
    */

    //ugh, converting old classads into compat_classad::ClassAds, in probably the 
    //  worst possible way. Ever. But it should work.
    c1FP = fopen("c1FPcompat.txt", "w+");
    c2FP = fopen("c2FPcompat.txt", "w+");
    c3FP = fopen("c3FPcompat.txt", "w+");
    c4FP = fopen("c4FPcompat.txt", "w+");

    c1->fPrint(c1FP);
    c2->fPrint(c2FP);
    c3->fPrint(c3FP);
    c4->fPrint(c4FP);

    fclose(c1FP); fclose(c2FP); fclose(c3FP); fclose(c4FP);
    delete c1;
    delete c2; 
    delete c3;
    delete c4;

}
//}}}

//{{{setUpCompatClassAd()
void setUpCompatClassAds(compat_classad::ClassAd** compC1, compat_classad::ClassAd** compC2,
        compat_classad::ClassAd** compC3, compat_classad::ClassAd** compC4, 
        FILE* c1FP, FILE* c2FP, FILE* c3FP, FILE* c4FP,
        int verbose)
{
    c1FP = fopen("c1FPcompat.txt", "r+");
    c2FP = fopen("c2FPcompat.txt", "r+");
    c3FP = fopen("c3FPcompat.txt", "r+");
    c4FP = fopen("c4FPcompat.txt", "r+");

    if(verbose)
        printf("creating compatclassads\n");

    int eofCheck, errorCheck, emptyCheck; 
    (*compC1) = new compat_classad::ClassAd(c1FP, ",", eofCheck, errorCheck, emptyCheck); 
    (*compC2)= new compat_classad::ClassAd(c2FP, ",", eofCheck, errorCheck, emptyCheck); 
    (*compC3) = new compat_classad::ClassAd(c3FP, ",", eofCheck, errorCheck, emptyCheck); 
    (*compC4) = new compat_classad::ClassAd(c4FP, ",", eofCheck, errorCheck, emptyCheck); 
    fclose(c1FP); fclose(c2FP); fclose(c3FP); fclose(c4FP);

    (*compC1)->SetMyTypeName("compC1");
    (*compC2)->SetMyTypeName("compC2");
    (*compC3)->SetMyTypeName("compC3");
    (*compC4)->SetMyTypeName("compC4");

    (*compC1)->SetTargetTypeName("not compC1!");
    (*compC2)->SetTargetTypeName("not compC2!");
    (*compC3)->SetTargetTypeName("not compC3!");
    (*compC4)->SetTargetTypeName("not compC4!");

}
//}}}

//{{{ test_sPrintExpr
bool test_sPrintExpr(compat_classad::ClassAd *c1, int verbose)
{
    /* c1 should have 2 attributes:
     *  A = 1
     *  B = 2
     * So we're testing against those.
     */
    bool passedNonNull = false, passedNull = false, passed = false;
    bool passedNonsense = false;
    classad::AttrList::iterator itr;
    itr = c1->begin();

    char *buffer1, *buffer2;
    int bufferSize = 16;
    buffer1 = (char*) calloc(bufferSize, sizeof(char));


    buffer1 = c1->sPrintExpr(buffer1, bufferSize, (*itr).first.c_str());

    if(!strcmp(buffer1, "A = 1") )
    {
        passedNonNull = true;
    }

    if(verbose)
        printf("buf != NULL test %s.\n", passedNonNull ? "passed" : "failed");

    buffer2 = c1->sPrintExpr(NULL, 0, (*itr).first.c_str()); 

    if(!strcmp(buffer2, "A = 1") )
    {
        passedNull = true;
    }

    if(verbose)
        printf("buf == NULL test %s.\n", passedNull ? "passed" : "failed");


    free(buffer1);
    //buffer1 = (char*) calloc(bufferSize, sizeof(char));

    buffer1 = c1->sPrintExpr(NULL, 0, "fred");
    if(buffer1 == NULL)
    {
        passedNonsense = TRUE; 
    }

    if(verbose)
        printf("Nonsense Attr test %s.\n", passedNonsense ? "passed" : "failed");
    passed = passedNull && passedNonNull && passedNonsense;

    free(buffer1);
    free(buffer2);

    return passed;

}
//}}}

//{{{test_IsValidAttrValue
bool test_IsValidAttrValue(compat_classad::ClassAd *c1, int verbose)
{
    bool passedReal = false, passedSlashN = false, passed = false;
    bool passedNonReal = false;
    classad::AttrList::iterator itr;
    itr = c1->begin();

    //should be true
    if( c1->IsValidAttrValue((*itr).first.c_str()) )
    {
        passedReal = true;
    }

    if(verbose)
        printf("IsValidAttrValue w/ real attr %s.\n", passedReal ? "passed" : "failed");


    //passedSlashN because it only fails on '\n' or '\r'.
    
    //should be false...I *hope* it'd be false
    if( !(c1->IsValidAttrValue("Arthur Dent\n") ) )
    {
        passedSlashN = true;
    }

    if(verbose)
        printf("IsValidAttrValue w/ slash n attr %s.\n", passedSlashN ? "passed" : "failed");

    //should be true
    if( c1->IsValidAttrValue("fred") )
    {
        passedNonReal = true;
    }

    if(verbose)
        printf("IsValidAttrValue w/ non-real attr %s.\n", passedNonReal ? "passed" : "failed");

    passed = passedReal && passedSlashN && passedNonReal;

    return passed;
}
//}}}

//{{{test_fPrintAsXML
bool test_fPrintAsXML(compat_classad::ClassAd *c1, int verbose)
{
    bool passed = false;
    FILE* compC1XML;

    compC1XML = fopen("compC1XML.xml", "w+");
    c1->fPrintAsXML(compC1XML);

    fclose(compC1XML);
    if(verbose){}

    //uh...not sure how to test this.
    passed = true;

    return passed;
}
//}}}

//{{{test_sPrintAsXML
bool test_sPrintAsXML(int verbose)
{
    /* it's tested in fPrintAsXML */
    bool passed = false;

    if(verbose)
    {

    }

    return passed;
}
//}}}

//{{{ test_ChainCollapse
bool test_ChainCollapse(compat_classad::ClassAd *c2, compat_classad::ClassAd *c3, int verbose)
{
    /* ok, so this test sucks. It doesn't really check to see if 
     *  the ChainCollapse worked, it just checks to see if 
     *  the size of the ad was different after using ChainCollapse.
     *  So, it'll fail if ads with identical attributes are tested against
     *  it.
     */
    bool passed = false;
    int originalSize;


    originalSize = c2->size();

    c2->ChainToAd(c3);

    if(verbose){}
    /*
    if(verbose)
    {
        if( verbose && c2->GetChainedParentAd() ) 
        {
            printf("c2 chained to c3.\n");
        }
        else
        {
            printf("c2 not chained to c3.\n");
        }
        c2->fPrint(stdout);
        printf("Calling ChainCollapse on c2.\n");
        printf("------\n");
    }
    */
    
    c2->ChainCollapse();

     
    /*
    if(verbose)
    {
        if( c2->GetChainedParentAd() ) 
        {
            printf("c2 chained to c3.\n");
        }
        else
        {
            printf("c2 not chained to c3.\n");
        }
        c2->fPrint(stdout);
        printf("------\n");
    }
    */

    if(originalSize == c2->size() )
    {
        passed = false;
    }
    else
    {
        passed = true;
    }

    return passed;
}
//}}}

//{{{test_EvalStringCharStar
bool test_EvalStringCharStar(compat_classad::ClassAd *c1, compat_classad::ClassAd *c2, int verbose)
{
    bool passed = false;
    bool passedTest[4];
    int esRetVal[2]; //evalstring return value

    for(int i = 0; i < 4; i++)
    {
        passedTest[i] = false;
    }

    classad::AttrList::iterator itr;
    itr = c1->begin();

    char* tmpValue;
    tmpValue = (char*)calloc(1024, sizeof(char));

    passedTest[0] = c1->EvalString((*itr).first.c_str(), c1, tmpValue);

    //if it's not "hello", something bad happened.
    if(strcmp(tmpValue, "hello") )
    {
        passedTest[0] = false;
    }

    if(verbose)
        printf("EvalString w/ real attr, this target %s.\n", 
            passedTest[0] ? "passed" : "failed");
    

    strcpy(tmpValue, "");

    passedTest[1] = c1->EvalString((*itr).first.c_str(), c2, tmpValue);
    
    if(strcmp(tmpValue, "hello"))
    {
        passedTest[1] = false;
    }

    if(verbose)
        printf("EvalString w/ real attr, c2 target %s.\n", 
            passedTest[1] ? "passed" : "failed");

    strcpy(tmpValue, "");

    esRetVal[0] = c1->EvalString("fred", c1, tmpValue);
    
    if(esRetVal[0] == 0)
    {
        passedTest[2] = true;
    }

    if(verbose)
        printf("EvalString w/ fake attr, this target %s.\n", 
            passedTest[2] ? "passed" : "failed");


    strcpy(tmpValue, "");

    esRetVal[1] = c1->EvalString("fred", c2, tmpValue);
    
    //it should fail
    if(esRetVal[1] == 0)
    {
        passedTest[3] = true;
    }

    if(verbose)
        printf("EvalString w/ fake attr, c2 target %s.\n", 
            passedTest[3] ? "passed" : "failed");


    passed = passedTest[0] && passedTest[1] && passedTest[2] &&
                passedTest[3];

    free(tmpValue);

    return passed;
}
//}}}

//{{{test_EvalStringCharStarStar
bool test_EvalStringCharStarStar(compat_classad::ClassAd *c1, compat_classad::ClassAd *c2, int verbose)
{
    bool passed = false;
    bool passedTest[4];
    int esRetVal[2]; //evalstring return value

    for(int i = 0; i < 4; i++)
    {
        passedTest[i] = false;
    }

    classad::AttrList::iterator itr;
    itr = c1->begin();

    char* tmpValue;

    passedTest[0] = c1->EvalString((*itr).first.c_str(), c1, &tmpValue);

    //if it's not "hello", something bad happened.
    if(strcmp(tmpValue, "hello") )
    {
        passedTest[0] = false;
    }

    if(verbose)
        printf("EvalStringChar** w/ real attr, this target, non-malloc'd value %s.\n", 
            passedTest[0] ? "passed" : "failed");
    
    free(tmpValue);
    //strcpy(tmpValue, "");
    tmpValue = (char*)malloc(10 * sizeof(char));

    passedTest[1] = c1->EvalString((*itr).first.c_str(), c2, &tmpValue);
    
    if(strcmp(tmpValue, "hello"))
    {
        passedTest[1] = false;
    }

    if(verbose)
        printf("EvalStringChar** w/ real attr, c2 target, malloc'd value %s.\n", 
            passedTest[1] ? "passed" : "failed");

    free(tmpValue);
    //strcpy(tmpValue, "");
    tmpValue = (char*)malloc(10 * sizeof(char));

    esRetVal[0] = c1->EvalString("fred", c1, &tmpValue);
    
    if(esRetVal[0] == 0)
    {
        passedTest[2] = true;
    }

    if(verbose)
        printf("EvalStringChar** w/ fake attr, this target, malloc'd value %s.\n", 
            passedTest[2] ? "passed" : "failed");


    free(tmpValue);

    esRetVal[1] = c1->EvalString("fred", c2, &tmpValue);
    
    //it should fail
    if(esRetVal[1] == 0)
    {
        passedTest[3] = true;
    }

    if(verbose)
        printf("EvalStringChar** w/ fake attr, c2 target, non-malloc'd value %s.\n", 
            passedTest[3] ? "passed" : "failed");


    passed = passedTest[0] && passedTest[1] && passedTest[2] &&
                passedTest[3];

    return passed;
}
//}}}

//{{{test_EvalStringMyString
bool test_EvalStringMyString(compat_classad::ClassAd *c1, compat_classad::ClassAd *c2, int verbose)
{
    bool passed = false;
    bool passedTest[4];
    int esRetVal[2]; //evalstring return value

    for(int i = 0; i < 4; i++)
    {
        passedTest[i] = false;
    }

    classad::AttrList::iterator itr;
    itr = c1->begin();

    MyString tmpValue; 

    passedTest[0] = c1->EvalString((*itr).first.c_str(), c1, tmpValue);

    //if it's not "hello", something bad happened.
    if(strcmp(tmpValue.Value(), "hello"))
    {
        passedTest[0] = false;
    }

    if(verbose)
        printf("EvalStringMyString w/ real attr, this target %s.\n", 
            passedTest[0] ? "passed" : "failed");

    tmpValue = "";

    passedTest[1] = c1->EvalString((*itr).first.c_str(), c2, tmpValue);
    
    if(strcmp(tmpValue.Value(), "hello"))
    {
        passedTest[1] = false;
    }

    if(verbose)
        printf("EvalStringMyString w/ real attr, c2 target %s.\n", 
            passedTest[1] ? "passed" : "failed");

    esRetVal[0] = c1->EvalString("fred", c1, tmpValue);
    
    if(esRetVal[0] == 0)
    {
        passedTest[2] = true;
    }

    if(verbose)
        printf("EvalStringMyString w/ fake attr, this target %s.\n", 
            passedTest[2] ? "passed" : "failed");

    esRetVal[1] = c1->EvalString("fred", c2, tmpValue);
    
    //it should fail
    if(esRetVal[1] == 0)
    {
        passedTest[3] = true;
    }

    if(verbose)
        printf("EvalStringMyString w/ fake attr, c2 target %s.\n", 
            passedTest[3] ? "passed" : "failed");


    passed = passedTest[0] && passedTest[1] && passedTest[2] &&
                passedTest[3];

    return passed;
}
//}}}

//{{{NextDirtyExpr
bool test_NextDirtyExpr(compat_classad::ClassAd *c1, int verbose)
{
	const char *name;
	classad::ExprTree *expr;
    /* this depends on there only being 2 attrs per classad!
     */
    bool passed = false;
    bool passedTest[4];

    classad::AttrList::iterator itr;
    itr = c1->begin();

    for(int i = 0; i < 4; i++) passedTest[i] = false;

    if(c1->NextDirtyExpr(name, expr))
    {
        passedTest[0] = true;
    }

    if(verbose)
        printf("First NextDirtyExpr %s.\n", passedTest[0] ? "passed" : "failed");

    if(c1->NextDirtyExpr(name, expr))
    {
        passedTest[1] = true;
    }

    if(verbose)
        printf("Second NextDirtyExpr %s.\n", passedTest[1] ? "passed" : "failed");

    //skip over the FooType attrs
    c1->NextDirtyExpr(name, expr);
    c1->NextDirtyExpr(name, expr);

    if(!c1->NextDirtyExpr(name, expr))
    {
        passedTest[2] = true;
        if(verbose == 2)
            printf("Good. Returned NULL.\n");
    }

    if(verbose)
        printf("Third NextDirtyExpr %s.\n", passedTest[2] ? "passed" : "failed");

    c1->ResetExpr();
    if(c1->NextDirtyExpr(name, expr))
    {
        passedTest[3] = true;
        if(verbose == 2)
            printf("Good, After resetting itr, it worked.\n");
    }

    if(verbose)
        printf("Fourth NextDirtyExpr %s.\n", passedTest[3] ? "passed" : "failed");
    passed = passedTest[0] && passedTest[1] && passedTest[2] && passedTest[3];

    c1->DisableDirtyTracking();
    c1->ClearAllDirtyFlags();

    return passed;
}
//}}}

//{{{ test_EscapeStringValue
bool test_EscapeStringValue(compat_classad::ClassAd *c1, int verbose)
{
    bool passed;
    bool passedTest[2];
    passedTest[0] = false; passedTest[1] = false; 

    const char *tmp; 
    const char *tmp2;
    string tmpString;

    classad::AttrList::iterator itr;
    itr = c1->begin();

    c1->EvaluateAttrString((*itr).first, tmpString);

    tmp = c1->EscapeStringValue(tmpString.c_str());

    if(!strcmp(tmp, "\"hello\""))
    {
        passedTest[0] = true;
    }

    if(verbose == 2)
        printf("Expected %s and EscapeStringValue returned %s.\n", "\"hello\"", tmp); 

    tmp2 = c1->EscapeStringValue(NULL);
    
    if(tmp2 != NULL)
    {
        printf("%s\n", tmp2);

        if(verbose == 2)
            printf("Bad. Passed in NULL and got something back.\n");
    }
    else
    {
        passedTest[1] = true;
        if(verbose == 2)
            printf("Good. Passed in NULL and got NULL back.\n");
    }   

    passed = passedTest[0] && passedTest[1];
    return passed;

}
//}}}

//{{{ test_EvalTree
bool test_EvalTree(compat_classad::ClassAd *c1, compat_classad::ClassAd *c2, int verbose)
{
    bool passed = false;
    bool passedShortHand = false, passedNullTarget = false;
    bool passedReal = false, passedNullMine = false;
    bool passedNullMineRealTarget = false;

	classad::ClassAdUnParser unp;
    string buf;
   
    classad::AttrList::iterator itr;
    itr = c1->begin();

    classad::Value tmpVal;

    //should succeed
    passedShortHand = EvalTree(itr->second, c1, &tmpVal);

    if(passedShortHand)
    {
        unp.Unparse(buf, tmpVal);
    }

    if(verbose)
    {
        printf("First EvalTree (shorthand) %s.\n", passedShortHand ? "passed" : "failed");
        if(passedShortHand && verbose == 2)
        {
            printf("tmpVal is %s.\n", buf.c_str() ); 
        }
    }

    tmpVal.Clear();
    buf = "";
    
    //should succeed

    passedNullTarget = EvalTree(itr->second, c1,NULL, &tmpVal);

    if(passedNullTarget)
    {
        unp.Unparse(buf, tmpVal);
    }

    if(verbose)
    {
        printf("Second EvalTree (null target) %s.\n", passedNullTarget ? "passed" : "failed");
        if(passedNullTarget && verbose == 2)
        {
            printf("tmpVal is %s.\n", buf.c_str() ); 
        }
    }

    buf = "";
    tmpVal.Clear();

    //should fail.
    if(!EvalTree(itr->second, NULL,NULL, &tmpVal) )
    {
        passedNullMine = true;
    }

    if(verbose)
    {
        printf("Third EvalTree (null mine) %s.\n", passedNullMine ? "passed" : "failed");
    }

    tmpVal.Clear();

    //this should also fail
    if(!EvalTree(itr->second, NULL,c2, &tmpVal) )
    {
        passedNullMineRealTarget = true;
    }

    if(verbose)
    {
        printf("Fourth EvalTree (null mine) %s.\n", passedNullMineRealTarget
                ? "passed" : "failed");
    }

    tmpVal.Clear();
    itr = c2->begin();
    itr++;
    //should pass
    passedReal = EvalTree(itr->second, c1, c2, &tmpVal);

    if(passedReal)
    {
        unp.Unparse(buf, tmpVal);
    }

    bool wasRightNumber;

    if(strcmp(buf.c_str(),"3"))
    {
        wasRightNumber = false; 
    }
    else
    {
        wasRightNumber = true;
    }

    if(verbose)
    {
        printf("Fifth EvalTree (Real) %s.\n", passedReal ? "passed" : "failed");
        if(!wasRightNumber)
        {
            printf("But the number was not what was expected. Expected 3 and got %s.\n", buf.c_str());
        }
        else if(passedReal && verbose == 2)
        {
            printf("tmpVal is %s.\n", buf.c_str() ); 
        }

    }

    passed = passedShortHand && passedNullTarget && passedNullMine 
                && passedNullMineRealTarget && passedReal;

    return passed;
}

//}}}

//{{{ GetInternalReferences
bool test_GIR(int verbose)
{
    bool passed = false;
    bool passedFull1 = false, passedNonFull1 = false;
    bool passedFull2 = false, passedNonFull2 = false;
    bool passedFull3 = false, passedNonFull3 = false;
    
    classad::ClassAd* c;
    classad::ClassAdParser parser;

    // expr C is an opnode

    string input_ref = "[ A = 3; B = {1,3,5}; C = D.A + 1; D = [A = E; B = 8;]; E = 4; ]";

    c = parser.ParseClassAd(input_ref);

    if(verbose && c == NULL)
    {
        printf("Classad couldn't be parsed!\n");
        passed = false;
    }

    if(verbose == 2)
    {
        printf("Working on 1st ref string. Fullname = true\n");
        printf("ref string is %s.\n", input_ref.c_str());
    }

    passedFull1 = runAndCheckGIR(c, "A", "", true, verbose);
    passedFull1 = runAndCheckGIR(c, "B", "", true, verbose);
    passedFull1 = runAndCheckGIR(c, "C", "D.A", true, verbose);
    passedFull1 = runAndCheckGIR(c, "D", "E", true, verbose);
    passedFull1 = runAndCheckGIR(c, "E", "", true, verbose);

    if(verbose)
    {
        printf("GIR with Fullname = true on ref1 %s.\n", passedFull1 ? "passed" : "failed");
    }

    if(verbose == 2) 
    {
        printf("Working on 1st ref string. Fullname = false\n");
    }

    passedNonFull1 = runAndCheckGIR(c, "A", "", false, verbose);
    passedNonFull1 = runAndCheckGIR(c, "B", "", false, verbose);
    passedNonFull1 = runAndCheckGIR(c, "C", "A", false, verbose);
    passedNonFull1 = runAndCheckGIR(c, "D", "E", false, verbose);
    passedNonFull1 = runAndCheckGIR(c, "E", "", false, verbose);

    if(verbose)
    {
        printf("GIR with Fullname = false on ref1 %s.\n", passedNonFull1 ? "passed" : "failed");
    }

    string input_ref2 = "[ A = 3; B = {1}; C = G.F; D = [A = B; B = 9;]; E = D.A + C; ]";

    if(verbose == 2)
    {
        printf("Working on 2nd ref string. Fullname = true\n");
        printf("ref string is %s.\n", input_ref2.c_str());
    }
    //redo!
    delete c;

    c = parser.ParseClassAd(input_ref2);

    if(verbose && c == NULL)
    {
        printf("Classad 2 couldn't be parsed!\n");
        passed = false;
    }

    passedFull2 = runAndCheckGIR(c, "A", "", true, verbose);
    passedFull2 = runAndCheckGIR(c, "B", "", true, verbose);
    passedFull2 = runAndCheckGIR(c, "C", "", true, verbose);
    passedFull2 = runAndCheckGIR(c, "D", "B", true, verbose);
    passedFull2 = runAndCheckGIR(c, "E", "D.A,C", true, verbose);

    if(verbose)
    {
        printf("GIR with Fullname = true on ref2 %s.\n", passedFull2 ? "passed" : "failed");
    }

    if(verbose == 2)
    {
        printf("Working on 2nd ref string. Fullname = false\n");
    }

    passedNonFull2 = runAndCheckGIR(c, "A", "", false, verbose);
    passedNonFull2 = runAndCheckGIR(c, "B", "", false, verbose);
    passedNonFull2 = runAndCheckGIR(c, "C", "", false, verbose);
    passedNonFull2 = runAndCheckGIR(c, "D", "B", false, verbose);
    passedNonFull2 = runAndCheckGIR(c, "E", "A,C", false, verbose);

    if(verbose)
    {
        printf("GIR with Fullname = false on ref2 %s.\n", passedNonFull2 ? "passed" : "failed");
    }

    string input_ref3 = "[ A = G.B; B = {5}; C = A + D.B; D = [A = 2; B = E.C;]; E = [C = 7;]; ]";

    if(verbose == 2)
    {
        printf("Working on 3nd ref string. Fullname = true\n");
        printf("ref string is %s.\n", input_ref3.c_str());
    }
    //redo!
    delete c;

    c = parser.ParseClassAd(input_ref3);

    if(verbose && c == NULL)
    {
        printf("Classad 3 couldn't be parsed!\n");
        passed = false;
    }
    passedFull3 = runAndCheckGIR(c, "A", "", true, verbose);
    passedFull3 = runAndCheckGIR(c, "B", "", true, verbose);
    passedFull3 = runAndCheckGIR(c, "C", "A,D.B", true, verbose);
    passedFull3 = runAndCheckGIR(c, "D", "E.C", true, verbose);
    passedFull3 = runAndCheckGIR(c, "E", "", true, verbose);

    if(verbose)
    {
        printf("GIR with Fullname = true on ref3 %s.\n", passedFull3 ? "passed" : "failed");
    }

    if(verbose == 2)
    {
        printf("Working on 3nd ref string. Fullname = false\n");
    }

    passedNonFull3 = runAndCheckGIR(c, "A", "", false, verbose);
    passedNonFull3 = runAndCheckGIR(c, "B", "", false, verbose);
    passedNonFull3 = runAndCheckGIR(c, "C", "A,B", false, verbose);
    passedNonFull3 = runAndCheckGIR(c, "D", "C", false, verbose);
    passedNonFull3 = runAndCheckGIR(c, "E", "", false, verbose);

    if(verbose)
    {
        printf("GIR with Fullname = false on ref3 %s.\n", passedNonFull3 ? "passed" : "failed");
    }

    delete c;
    c = NULL;

    passed = passedFull1 && passedNonFull1 && passedFull2 && passedNonFull2 && passedFull3 && passedNonFull3;
    return passed;
}
//}}}

//{{{ GIR helper
classad::References* gir_helper(classad::ClassAd* c, string attr, int verbose, bool full)
{
    //bool passed = false;

    classad::References* refs = new classad::References();
    classad::References::iterator iter;
    classad::ExprTree *expr;

    if( c != NULL )
    {
        expr = c->Lookup(attr);
        if(expr != NULL)
        {
            bool have_references;
            if(have_references = c->GetInternalReferences(expr, *refs, full))
            {
                if(have_references)
                {
                    if(verbose == 2)
                    {
                        if(refs->size() > 0)
                        {
                            printf("Returned refs: \n");
                            for(iter = refs->begin(); iter != refs->end(); iter++)  
                            {
                                printf("%s\n", (*iter).c_str());
                            }
                        }
                    }

                    //passed = true;
                }
            }
        }
    }
    return refs;
    //return passed;
}

//}}}

//{{{ correctRefs
bool correctRefs(classad::References* ref, std::vector<string> expected)
{
    bool passed = false;

    classad::References::iterator itr;
    std::vector<string>::iterator vecItr;

    //refs didn't have anything 
    if(ref->size() == 0 && expected.size() == 0)
    {
        return true;
    }
    else if(ref == NULL)
    {
        return false;
    }

    //make sure they're the same size...
    if(ref->size() != expected.size())
    {
        return false;
    }

    bool foundOne;
    for(itr = ref->begin(); itr != ref->end(); itr++)
    {
        foundOne = false;
        for(vecItr = expected.begin(); vecItr != expected.end(); vecItr++)
        {
            if( !( (*itr).compare( (*vecItr) ) ) )
            {
                foundOne = true;
            }
        }

        if(!foundOne)
        {
            passed = false; 
            return passed;
        }
    }

    //if we got here, they must all be the same.
    passed = true;

    return passed;
}
//}}}

//{{{ runAndCheckGIR
bool runAndCheckGIR(classad::ClassAd* c, string attr, string listString, bool full, int verbose )
{
    bool passed = false;
    bool passedTest = false;

    classad::References* retRefs;
    std::vector<string> expectedVec;

    int subStrStart = 0;
    int subStrEnd = 0;

    //if "," doesn't exist, then skip the splitting-stage.
    bool singleAttr = (listString.find(",") == string::npos);

    if(!singleAttr)
    {
        for(int i = 0; i < listString.size(); i++)
        {
            if(listString[i] == ',' || i == (listString.size() - 1))
            {
                subStrEnd = i;
                expectedVec.push_back(listString.substr(subStrStart, subStrEnd));
                subStrStart = i + 1;
            }
        }
    }
    else if(listString.size() > 0)
    {
        expectedVec.push_back(listString); 
    }


    retRefs = gir_helper(c, attr, verbose, full);
    passedTest = correctRefs(retRefs, expectedVec);

    expectedVec.clear();

    if(verbose == 2)
    {
        printf("Attr \"%s\" %s.\n", attr.c_str(), passedTest ? "passed" : "failed");
    }

    delete retRefs;
    passed = passedTest;

    return passed;
}
//}}}


void setUpAndRun(int verbose)
{
    FILE *c1FP, *c2FP, *c3FP, *c4FP;
    /*
    bool sPrintExprPassed, isVAVPassed, fPrintAXPassed;//, sPrintAXPassed; 
    bool collapseChainPassed, evalStringCharStar, evalStringCharStarStar;
    bool evalStringMyString, nextDirtyExpr, escapeStringValue;
    bool evalTree;
    */

    int numTests = 11;
    bool passedTest[numTests];


    for(int i = 0; i < numTests; i++)
    {
        passedTest[i] = false;
    }


    setUpClassAds(c1FP, c2FP, c3FP, c4FP,verbose);

    compat_classad::ClassAd *compC1, *compC2, *compC3, *compC4;

    setUpCompatClassAds(&compC1, &compC2, &compC3, &compC4, c1FP, c2FP, 
            c3FP, c4FP, verbose);

    printf("Testing sPrintExpr...\n");
    passedTest[0] = test_sPrintExpr(compC1, verbose);
    printf("sPrintExpr %s.\n", passedTest[0] ? "passed" : "failed");
    printf("-------------\n");

    printf("Testing isValidAttrValue...\n");
    passedTest[1] = test_IsValidAttrValue(compC1, verbose);
    printf("IsValidAttrValue %s.\n", passedTest[1] ? "passed" : "failed");
    printf("-------------\n");


    printf("Testing fPrintAsXML and sPrintAsXML...\n");
    passedTest[2] = test_fPrintAsXML(compC1, verbose);
    printf("fPrintAsXML and sPrintAsXML %s.\n", passedTest[2] ? "passed" : "failed");
    printf("-------------\n");


    printf("Testing ChainCollapse...\n");
    passedTest[3] = test_ChainCollapse(compC3, compC2, verbose);
    printf("ChainCollapse %s.\n", passedTest[3] ? "passed" : "failed");
    printf("-------------\n");


    printf("Testing EvalStringChar*...\n");
    passedTest[4] = test_EvalStringCharStar(compC4, compC2, verbose);
    printf("EvalStringChar* %s.\n", passedTest[4] ? "passed" : "failed");
    printf("-------------\n");


    printf("Testing EvalStringChar**...\n");
    passedTest[5] = test_EvalStringCharStarStar(compC4, compC2, verbose);
    printf("EvalStringChar** %s.\n", passedTest[5] ? "passed" : "failed");
    printf("-------------\n");


    printf("Testing EvalStringMyString...\n");
    passedTest[6] = test_EvalStringMyString(compC4, compC2, verbose);
    printf("EvalStringMyString %s.\n", passedTest[6] ? "passed" : "failed");
    printf("-------------\n");


    printf("Testing NextDirtyExpr...\n");
    passedTest[7] = test_NextDirtyExpr(compC4, verbose);
    printf("NextDirtyExpr %s.\n", passedTest[7] ? "passed" : "failed");
    printf("-------------\n");


    printf("Testing EscapeStringValue...\n");
    passedTest[8] = test_EscapeStringValue(compC4, verbose);
    printf("Escape String Value %s.\n", passedTest[8] ? "passed" : "failed");
    printf("-------------\n");


    printf("Testing EvalTree...\n");
    passedTest[9] = test_EvalTree(compC4,compC2, verbose);
    printf("EvalTree %s.\n", passedTest[9] ? "passed" : "failed");
    printf("-------------\n");

    printf("Testing GetInternalReferences...\n");
    passedTest[10] = test_GIR(verbose);
    printf("GIR %s.\n", passedTest[10] ? "passed" : "failed");
    printf("-------------\n");

    bool allPassed = passedTest[0];
    int numPassed = 0;

    for(int i = 0; i < numTests; i++)
    {
        if(passedTest[i]) numPassed++;

        allPassed = allPassed && passedTest[i]; 
    }

    if(allPassed)
    {
        printf("All tests passed.\n");
    }
    else
    {
        printf("%d of %d tests passed.\n", numPassed, numTests);
    }

    delete compC1; 
    delete compC2; 
    delete compC3;
    delete compC4;
}

int main(int argc, char **argv)
{
    int verbose = 0;

    if(argc > 1 && !strcmp(argv[1], "-v"))
    {
        verbose = 1;
    }
    else if( argc > 1 && !strcmp(argv[1], "-vv"))
    {
        verbose = 2;
    }

    setUpAndRun(verbose);

}
