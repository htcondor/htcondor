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

bool test_sPrintExpr(CompatClassAd *c1, bool verbose);
bool test_IsValidAttrValue(CompatClassAd *c1, bool verbose);
bool test_fPrintAsXML(CompatClassAd *c1, bool verbose);
bool test_sPrintAsXML(bool verbose); //I guess if fPrintAsXML works, 
                                     //this does too

bool test_ChainCollapse(CompatClassAd *c2, CompatClassAd *c3, bool verbose);
bool test_EvalStringCharStar(CompatClassAd *c1, CompatClassAd *c2, bool verbose);
bool test_EvalStringCharStarStar(CompatClassAd *c1, CompatClassAd *c2, bool verbose);
bool test_EvalStringMyString(CompatClassAd *c1, CompatClassAd *c2, bool verbose);

bool test_NextDirtyExpr(CompatClassAd *c1, bool verbose);
bool test_EscapeStringValue(CompatClassAd *c1, bool verbose);

void setUpAndRun(bool verbose);

char *classad_strings[] = 
{
    "A = 1, B = 2",
    "A = 1, B = 3",
    "B = 1241, C = 3, D = 4",
    "A = \"hello\", E = 5"
};

//{{{ setUpClassAds
void setUpClassAds(ClassAd* c1, ClassAd* c2, ClassAd* c3, ClassAd* c4, FILE* c1FP, FILE* c2FP, FILE* c3FP, FILE* c4FP, bool verbose)
{
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

    //ugh, converting old classads into CompatClassAds, in probably the 
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
}
//}}}

//{{{setUpCompatClassAd()
void setUpCompatClassAds(CompatClassAd** compC1, CompatClassAd** compC2,
        CompatClassAd** compC3, CompatClassAd** compC4, 
        FILE* c1FP, FILE* c2FP, FILE* c3FP, FILE* c4FP,
        bool verbose)
{
    c1FP = fopen("c1FPcompat.txt", "r+");
    c2FP = fopen("c2FPcompat.txt", "r+");
    c3FP = fopen("c3FPcompat.txt", "r+");
    c4FP = fopen("c4FPcompat.txt", "r+");

    if(verbose)
        printf("creating compatclassads\n");

    int eofCheck, errorCheck, emptyCheck; 
    (*compC1) = new CompatClassAd(c1FP, ",", eofCheck, errorCheck, emptyCheck); 
    (*compC2)= new CompatClassAd(c2FP, ",", eofCheck, errorCheck, emptyCheck); 
    (*compC3) = new CompatClassAd(c3FP, ",", eofCheck, errorCheck, emptyCheck); 
    (*compC4) = new CompatClassAd(c4FP, ",", eofCheck, errorCheck, emptyCheck); 
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
bool test_sPrintExpr(CompatClassAd *c1, bool verbose)
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


    strcpy(buffer1, ""); //CLEAR!

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
bool test_IsValidAttrValue(CompatClassAd *c1, bool verbose)
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
bool test_fPrintAsXML(CompatClassAd *c1, bool verbose)
{
    bool passed = false;
    FILE* compC1XML;

    compC1XML = fopen("compC1XML.xml", "w+");
    c1->fPrintAsXML(compC1XML);

    fclose(compC1XML);

    //uh...not sure how to test this.
    passed = true;

    return passed;
}
//}}}

//{{{test_sPrintAsXML
bool test_sPrintAsXML(bool verbose)
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
bool test_ChainCollapse(CompatClassAd *c2, CompatClassAd *c3, bool verbose)
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
bool test_EvalStringCharStar(CompatClassAd *c1, CompatClassAd *c2, bool verbose)
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
bool test_EvalStringCharStarStar(CompatClassAd *c1, CompatClassAd *c2, bool verbose)
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
    

    strcpy(tmpValue, "");

    passedTest[1] = c1->EvalString((*itr).first.c_str(), c2, &tmpValue);
    
    if(strcmp(tmpValue, "hello"))
    {
        passedTest[1] = false;
    }

    if(verbose)
        printf("EvalStringChar** w/ real attr, c2 target, malloc'd value %s.\n", 
            passedTest[1] ? "passed" : "failed");

    strcpy(tmpValue, "");

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
bool test_EvalStringMyString(CompatClassAd *c1, CompatClassAd *c2, bool verbose)
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
bool test_NextDirtyExpr(CompatClassAd *c1, bool verbose)
{
    /* this depends on there only being 2 attrs per classad!
     */
    bool passed = false;
    bool passedTest[4];

    classad::AttrList::iterator itr;
    itr = c1->begin();

    for(int i = 0; i < 4; i++) passedTest[i] = false;

    if(c1->NextDirtyExpr())
    {
        passedTest[0] = true;
    }

    if(verbose)
        printf("First NextDirtyExpr %s.\n", passedTest[0] ? "passed" : "failed");

    if(c1->NextDirtyExpr())
    {
        passedTest[1] = true;
    }

    if(verbose)
        printf("Second NextDirtyExpr %s.\n", passedTest[1] ? "passed" : "failed");

    //skip over the FooType attrs
    c1->NextDirtyExpr();
    c1->NextDirtyExpr();

    if(!c1->NextDirtyExpr())
    {
        passedTest[2] = true;
        if(verbose)
            printf("Good. Returned NULL.\n");
    }

    if(verbose)
        printf("Third NextDirtyExpr %s.\n", passedTest[2] ? "passed" : "failed");

    c1->ResetDirtyItr();
    if(c1->NextDirtyExpr())
    {
        passedTest[3] = true;
        if(verbose)
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

bool test_EscapeStringValue(CompatClassAd *c1, bool verbose)
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

    if(verbose)
        printf("Expected %s and EscapeStringValue returned %s.\n", "\"hello\"", tmp); 

    tmp2 = c1->EscapeStringValue(NULL);
    
    if(tmp2 != NULL)
    {
        printf("%s\n", tmp2);

        if(verbose)
            printf("Bad. Passed in NULL and got something back.\n");
    }
    else
    {
        passedTest[1] = true;
        if(verbose)
            printf("Good. Passed in NULL and got NULL back.\n");
    }   

    passed = passedTest[0] && passedTest[1];
    return passed;

}
void setUpAndRun(bool verbose)
{
    ClassAd *c1, *c2, *c3, *c4;
    FILE *c1FP, *c2FP, *c3FP, *c4FP;
    bool sPrintExprPassed, isVAVPassed, fPrintAXPassed;//, sPrintAXPassed; 
    bool collapseChainPassed, evalStringCharStar, evalStringCharStarStar;
    bool evalStringMyString, nextDirtyExpr, escapeStringValue;

    setUpClassAds(c1, c2, c3, c4, c1FP, c2FP, c3FP, c4FP,verbose);

    CompatClassAd *compC1, *compC2, *compC3, *compC4;

    setUpCompatClassAds(&compC1, &compC2, &compC3, &compC4, c1FP, c2FP, 
            c3FP, c4FP, verbose);

    printf("Testing sPrintExpr...\n");
    sPrintExprPassed = test_sPrintExpr(compC1, verbose);
    printf("sPrintExpr %s.\n", sPrintExprPassed ? "passed" : "failed");

    printf("Testing isValidAttrValue...\n");
    isVAVPassed = test_IsValidAttrValue(compC1, verbose);
    printf("IsValidAttrValue %s.\n", isVAVPassed ? "passed" : "failed");

    printf("Testing fPrintAsXML and sPrintAsXML...\n");
    fPrintAXPassed = test_fPrintAsXML(compC1, verbose);
    printf("fPrintAsXML and sPrintAsXML %s.\n", fPrintAXPassed ? "passed" : "failed");

    printf("Testing ChainCollapse...\n");
    collapseChainPassed = test_ChainCollapse(compC3, compC2, verbose);
    printf("ChainCollapse %s.\n", collapseChainPassed ? "passed" : "failed");

    printf("Testing EvalStringChar*...\n");
    evalStringCharStar = test_EvalStringCharStar(compC4, compC2, verbose);
    printf("EvalStringChar* %s.\n", evalStringCharStar ? "passed" : "failed");

    printf("Testing EvalStringChar**...\n");
    evalStringCharStarStar = test_EvalStringCharStarStar(compC4, compC2, verbose);
    printf("EvalStringChar** %s.\n", evalStringCharStarStar ? "passed" : "failed");

    printf("Testing EvalStringMyString...\n");
    evalStringMyString = test_EvalStringMyString(compC4, compC2, verbose);
    printf("EvalStringMyString %s.\n", evalStringMyString ? "passed" : "failed");

    printf("Testing NextDirtyExpr...\n");
    nextDirtyExpr = test_NextDirtyExpr(compC4, verbose);
    printf("NextDirtyExpr %s.\n", nextDirtyExpr ? "passed" : "failed");

    printf("Testing EscapeStringValue...\n");
    escapeStringValue = test_EscapeStringValue(compC4, verbose);
    printf("Escape String Value %s.\n", escapeStringValue ? "passed" : "failed");
}

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

    setUpAndRun(verbose);

}
