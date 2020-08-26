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

#include "classad/classad_distribution.h"
#include "compat_classad.h"
#include "compat_classad_util.h"

#include "classad/sink.h"

#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <vector>
#include <utility>

using namespace std;

bool test_sPrintExpr(ClassAd *c1, int verbose);
bool test_IsValidAttrValue(ClassAd *c1, int verbose);
bool test_fPrintAdAsXML(ClassAd *c1, int verbose);
bool test_sPrintAdAsXML(int verbose); //I guess if fPrintAdAsXML works, 
                                     //this does too

bool test_ChainCollapse(ClassAd *c2, ClassAd *c3, int verbose);
bool test_EvalStringCharStar(ClassAd *c1, ClassAd *c2, int verbose);
bool test_EvalStringCharStarStar(ClassAd *c1, ClassAd *c2, int verbose);
bool test_EvalStringMyString(ClassAd *c1, ClassAd *c2, int verbose);
bool test_EvalStringStdString(ClassAd *c1, ClassAd *c2, int verbose);

bool test_NextDirtyExpr(ClassAd *c1, int verbose);
bool test_QuoteAdStringValue(ClassAd *c1, int verbose);

bool test_EvalExprTree(ClassAd *c1, ClassAd *c2,int verbose);

void setAllFalse(bool* b);

bool test_GIR(int verbose);
classad::References* gir_helper(classad::ClassAd* c, string attr, int verbose, bool full = true);

bool correctRefs(classad::References* ref, std::vector<string> expected);
bool runAndCheckGIR(classad::ClassAd* c, string attr, string listString, bool full, int verbose );

void printExprType(classad::ClassAd* c, string attr);

void setUpAndRun(int verbose);

class GIRTestCase
{
    private:
        const int numTests;
        int testNumber;
        string refString;
        vector< pair<string, string> > attrExpectedValPair;
        int verbose;
        bool dontRun;

    public:
        GIRTestCase();
        GIRTestCase(int _numTests, string _refString, vector< pair<string, string> > _attrExpectedValPair, int _verbosity, int _testNumber);

        bool runTests(void);

};

GIRTestCase::GIRTestCase() : numTests(0)
{
    refString = "";
    testNumber = 0;
}

GIRTestCase::GIRTestCase(int _numTests, string _refString, 
            vector< pair<string, string> > _attrExpectedValPair,
            int _verbosity, int _testNumber)
            : numTests(_numTests)
{

    //numTests = _numTests;
    testNumber = _testNumber;
    refString = _refString;
    verbose = _verbosity;
    attrExpectedValPair = _attrExpectedValPair;

    if((unsigned)(numTests * 2) != attrExpectedValPair.size())
    {
        printf("ERROR: Number of attr/expected val pairs needs to be 2 times the number of tests.\n");
        dontRun = true;
    }
    else
    {
        dontRun = false;
    }

}


bool 
GIRTestCase::runTests(void)
{
    if(dontRun) return false;

    bool testResultsIndividual[2][numTests];
    bool testResult[2];

    bool passed = false;

    for(int i = 0; i < numTests; i++)
    {
        testResultsIndividual[0][i] = false;
        testResultsIndividual[1][i] = false;
    }

    classad::ClassAd* c;
    classad::ClassAdParser parser;

    c = parser.ParseClassAd(refString);

    if(verbose && c == NULL)
    {
        printf("Classad couldn't be parsed!\n");
        passed = false;
        return passed;
    }

    if(verbose == 2)
    {
        printf("Working on ref string number %d. Fullname = true\n", testNumber);
        printf("ref string is %s.\n", refString.c_str());
    }

    for(int i = 0; i < numTests; i++)
    {
        testResultsIndividual[0][i] = runAndCheckGIR(c, attrExpectedValPair[i].first, 
                                        attrExpectedValPair[i].second,
                                        true,
                                        verbose);
    }

    testResult[0] = testResultsIndividual[0][0];

    for(int i = 1; i < numTests; i++)
    {
        testResult[0] = testResult[0] && testResultsIndividual[0][i];
    }

    if(verbose)
    {

        printf("GIR with Fullname = true on ref number %d %s.\n", testNumber, testResult[0] ? "passed" : "failed");
    }

    if(verbose == 2) 
    {
        printf("Working on ref string number %d. Fullname = false\n", testNumber);
    }

    for(int i = 0; i < numTests; i++)
    {
        testResultsIndividual[1][i] = runAndCheckGIR(c, attrExpectedValPair[i+numTests].first, 
                                        attrExpectedValPair[i+numTests].second,
                                        false,
                                        verbose);
    }

    testResult[1] = testResultsIndividual[1][0];

    for(int i = 1; i < numTests; i++)
    {
        testResult[1] = testResult[1] && testResultsIndividual[1][i];
    }

    if(verbose)
    {
        printf("GIR with Fullname = false on ref number %d %s.\n", testNumber, testResult[1] ? "passed" : "failed");
    }

    delete c;

    passed = testResult[0] && testResult[1];

    return passed;
}

//{{{setUpCompatClassAd()
void setUpCompatClassAds(ClassAd** compC1, ClassAd** compC2,
        ClassAd** compC3, ClassAd** compC4,
        int verbose)
{

    if(verbose)
        printf("creating compatclassads\n");

    classad::ClassAdParser parser;

    classad::ClassAd* c;
    
    c = parser.ParseClassAd("[A = 1; B = 2;]");
    (*compC1) = new ClassAd(*c);
    delete c;
    
    c = parser.ParseClassAd("[A = 1; B = 3;]");
    (*compC2) = new ClassAd(*c);
    delete c;

    c = parser.ParseClassAd("[B = 1241; C = 3; D = 4;]");
    (*compC3) = new ClassAd(*c);
    delete c;

    c = parser.ParseClassAd("[A = \"hello\";E = 5;F=\"abc\\\"\\\\efg\\\\\" ]");
    (*compC4) = new ClassAd(*c);
    delete c;

    SetMyTypeName(*(*compC1), "compC1");
    SetMyTypeName(*(*compC2), "compC2");
    SetMyTypeName(*(*compC3), "compC3");
    SetMyTypeName(*(*compC4), "compC4");

    SetTargetTypeName(*(*compC1), "not compC1!");
    SetTargetTypeName(*(*compC2), "not compC2!");
    SetTargetTypeName(*(*compC3), "not compC3!");
    SetTargetTypeName(*(*compC4), "not compC4!");

}
//}}}

//{{{ test_sPrintExpr
bool test_sPrintExpr(ClassAd *c1, int verbose)
{
    /* c1 should have 2 attributes:
     *  A = 1
     *  B = 2
     * So we're testing against those.
     */
    bool passedValid = false, passed = false;
    bool passedNonsense = false;
    classad::AttrList::iterator itr;
    itr = c1->begin();

    char *buffer;

    buffer = sPrintExpr(*c1, (*itr).first.c_str());

    if(!strcmp(buffer, "A = 1") )
    {
        passedValid = true;
    }

    if(verbose)
        printf("Valid attr test %s.\n", passedValid ? "passed" : "failed");

    free(buffer);

    buffer = sPrintExpr(*c1, "fred");
    if(buffer == NULL)
    {
        passedNonsense = true;
    }

    if(verbose)
        printf("Nonsense Attr test %s.\n", passedNonsense ? "passed" : "failed");
    passed = passedValid && passedNonsense;

    free(buffer);

    return passed;

}
//}}}

//{{{test_IsValidAttrValue
bool test_IsValidAttrValue(ClassAd *c1, int verbose)
{
    bool passedReal = false, passedSlashN = false, passed = false;
    bool passedNonReal = false;
    classad::AttrList::iterator itr;
    itr = c1->begin();

    //should be true
    if( IsValidAttrValue((*itr).first.c_str()) )
    {
        passedReal = true;
    }

    if(verbose)
        printf("IsValidAttrValue w/ real attr %s.\n", passedReal ? "passed" : "failed");


    //passedSlashN because it only fails on '\n' or '\r'.
    
    //should be false...I *hope* it'd be false
    if( !(IsValidAttrValue("Arthur Dent\n") ) )
    {
        passedSlashN = true;
    }

    if(verbose)
        printf("IsValidAttrValue w/ slash n attr %s.\n", passedSlashN ? "passed" : "failed");

    //should be true
    if( IsValidAttrValue("fred") )
    {
        passedNonReal = true;
    }

    if(verbose)
        printf("IsValidAttrValue w/ non-real attr %s.\n", passedNonReal ? "passed" : "failed");

    passed = passedReal && passedSlashN && passedNonReal;

    return passed;
}
//}}}

//{{{test_fPrintAdAsXML
bool test_fPrintAdAsXML(ClassAd *c1, int verbose)
{
    bool passed = false;
    FILE* compC1XML;

    compC1XML = safe_fopen_wrapper("compC1XML.xml", "w+");
    //compC1XML = fopen("compC1XML.xml", "w+");
    fPrintAdAsXML(compC1XML, *c1);

    fclose(compC1XML);
    if(verbose){}

    //uh...not sure how to test this.
    passed = true;

    return passed;
}
//}}}

//{{{test_sPrintAdAsXML
bool test_sPrintAdAsXML(int verbose)
{
    /* it's tested in fPrintAdAsXML */
    bool passed = false;

    if(verbose)
    {

    }

    return passed;
}
//}}}

//{{{ test_ChainCollapse
bool test_ChainCollapse(ClassAd *c2, ClassAd *c3, int verbose)
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
        fPrintAd(stdout, *c2);
        printf("Calling ChainCollapse on c2.\n");
        printf("------\n");
    }
    */
    
    ChainCollapse(*c2);

     
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
        fPrintAd(stdout, *c2);
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
bool test_EvalStringCharStar(ClassAd *c1, ClassAd *c2, int verbose)
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
bool test_EvalStringCharStarStar(ClassAd *c1, ClassAd *c2, int verbose)
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
bool test_EvalStringMyString(ClassAd *c1, ClassAd *c2, int verbose)
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

//{{{test_EvalStringStdString
bool test_EvalStringStdString(ClassAd *c1, ClassAd *c2, int verbose)
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

	std::string tmpValue; 

    passedTest[0] = c1->EvalString((*itr).first.c_str(), c1, tmpValue);

    //if it's not "hello", something bad happened.
    if(strcmp(tmpValue.c_str(), "hello"))
    {
        passedTest[0] = false;
    }

    if(verbose)
        printf("EvalStringMyString w/ real attr, this target %s.\n", 
            passedTest[0] ? "passed" : "failed");

    tmpValue = "";

    passedTest[1] = c1->EvalString((*itr).first.c_str(), c2, tmpValue);
    
    if(strcmp(tmpValue.c_str(), "hello"))
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
bool test_NextDirtyExpr(ClassAd *c1, int verbose)
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

//{{{ test_QuoteAdStringValue
bool test_QuoteAdStringValue(ClassAd *c1, int verbose)
{
    bool passed = true;

	const char *ans1 = "\"hello\"";
	const char *ans2 = "\"abc\\\"\\efg\\\"";

    const char *tmp; 
    string tmpString;

    string msTmp;
    c1->EvaluateAttrString("A", tmpString);

    tmp = QuoteAdStringValue(tmpString.c_str(), msTmp);

    if(strcmp(tmp, ans1))
    {
        passed = false;
    }

    if(verbose == 2)
        printf("Expected %s and QuoteAdStringValue returned %s.\n", ans1, tmp);

    c1->EvaluateAttrString("F", tmpString);
    tmp = QuoteAdStringValue(tmpString.c_str(), msTmp);

    if(strcmp(tmp, ans2))
    {
        passed = false;
    }

    if(verbose == 2)
        printf("Expected %s and QuoteAdStringValue returned %s.\n", ans2, tmp);

    tmp = QuoteAdStringValue(NULL, msTmp);
    
    if(tmp != NULL)
    {
		passed = false;
        printf("%s\n", tmp);

        if(verbose == 2)
            printf("Bad. Passed in NULL and got something back.\n");
    }
    else
    {
        if(verbose == 2)
            printf("Good. Passed in NULL and got NULL back.\n");
    }   

    return passed;

}
//}}}

//{{{ test_EvalExprTree
bool test_EvalExprTree(ClassAd *c1, ClassAd *c2, int verbose)
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
    passedShortHand = EvalExprTree(itr->second, c1, tmpVal);

    if(passedShortHand)
    {
        unp.Unparse(buf, tmpVal);
    }

    if(verbose)
    {
        printf("First EvalExprTree (shorthand) %s.\n", passedShortHand ? "passed" : "failed");
        if(passedShortHand && verbose == 2)
        {
            printf("tmpVal is %s.\n", buf.c_str() ); 
        }
    }

    tmpVal.Clear();
    buf = "";
    
    //should succeed

    passedNullTarget = EvalExprTree(itr->second, c1,NULL, tmpVal);

    if(passedNullTarget)
    {
        unp.Unparse(buf, tmpVal);
    }

    if(verbose)
    {
        printf("Second EvalExprTree (null target) %s.\n", passedNullTarget ? "passed" : "failed");
        if(passedNullTarget && verbose == 2)
        {
            printf("tmpVal is %s.\n", buf.c_str() ); 
        }
    }

    buf = "";
    tmpVal.Clear();

    //should fail.
    if(!EvalExprTree(itr->second, NULL,NULL, tmpVal) )
    {
        passedNullMine = true;
    }

    if(verbose)
    {
        printf("Third EvalExprTree (null mine) %s.\n", passedNullMine ? "passed" : "failed");
    }

    tmpVal.Clear();

    //this should also fail
    if(!EvalExprTree(itr->second, NULL,c2, tmpVal) )
    {
        passedNullMineRealTarget = true;
    }

    if(verbose)
    {
        printf("Fourth EvalExprTree (null mine) %s.\n", passedNullMineRealTarget
                ? "passed" : "failed");
    }

    tmpVal.Clear();
    itr = c2->begin();
    itr++;
    //should pass
    passedReal = EvalExprTree(itr->second, c1, c2, tmpVal);

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
        printf("Fifth EvalExprTree (Real) %s.\n", passedReal ? "passed" : "failed");
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

    const static int numTests = 6;

    bool passedTest[numTests];

    GIRTestCase *testcase[numTests];

    //classad::ClassAd* c;
    classad::ClassAdParser parser;

    // expr C is an opnode

    string input_ref = "[ A = 3; B = {1,3,5}; C = 1 + D.A; D = [A = E; B = 8;]; E = 4; ]";

    vector<pair<string, string> > expected;
    
    expected.push_back(make_pair("A", ""));
    expected.push_back(make_pair("B", ""));
    expected.push_back(make_pair("C", "D.A E"));
    expected.push_back(make_pair("D", "E"));
    expected.push_back(make_pair("E", ""));

    expected.push_back(make_pair("A", ""));
    expected.push_back(make_pair("B", ""));
    expected.push_back(make_pair("C", "D E"));
    expected.push_back(make_pair("D", "E"));
    expected.push_back(make_pair("E", ""));

    testcase[0] = new GIRTestCase(5, input_ref, expected, verbose, 1);
    passedTest[0] = testcase[0]->runTests();


    expected.clear();
    
    //test case 2

    string input_ref2 = "[ A = 3; B = {1}; C = G.F; D = [A = B; B = 9;]; E = C + D.A; ]";

    expected.push_back(make_pair("A", ""));
    expected.push_back(make_pair("B", ""));
    expected.push_back(make_pair("C", ""));
    expected.push_back(make_pair("D", "B"));
    expected.push_back(make_pair("E", "D.A B C"));

    expected.push_back(make_pair("A", ""));
    expected.push_back(make_pair("B", ""));
    expected.push_back(make_pair("C", ""));
    expected.push_back(make_pair("D", "B"));
    expected.push_back(make_pair("E", "D B C"));

    testcase[1] = new GIRTestCase(5, input_ref2, expected, verbose, 2);
    passedTest[1] = testcase[1]->runTests();


    expected.clear();

    string input_ref3 = "[ A = G.B; B = {5}; C = D.B + A; D = [A = 2; B = E.C;]; E = [C = 7;]; ]";

    expected.push_back(make_pair("A", ""));
    expected.push_back(make_pair("B", ""));
    expected.push_back(make_pair("C", "A D.B E.C"));
    expected.push_back(make_pair("D", "E.C"));
    expected.push_back(make_pair("E", ""));

    expected.push_back(make_pair("A", ""));
    expected.push_back(make_pair("B", ""));
    expected.push_back(make_pair("C", "A D E"));
    expected.push_back(make_pair("D", "E"));
    expected.push_back(make_pair("E", ""));

    testcase[2] = new GIRTestCase(5, input_ref3, expected, verbose, 3);
    passedTest[2] = testcase[2]->runTests();


    expected.clear();

    string input_ref4 = "[ A = G.B; B = {5}; C = A + D.B; D = [A = 2; B = E.C;]; E = [C = A;]; ]";

    expected.push_back(make_pair("A", ""));
    expected.push_back(make_pair("B", ""));
    expected.push_back(make_pair("C", "A D.B E.C"));
    expected.push_back(make_pair("D", "A E.C"));
    expected.push_back(make_pair("E", "A"));

    expected.push_back(make_pair("A", ""));
    expected.push_back(make_pair("B", ""));
    expected.push_back(make_pair("C", "A D E"));
    expected.push_back(make_pair("D", "E A"));
    expected.push_back(make_pair("E", "A"));

    testcase[3] = new GIRTestCase(5, input_ref4, expected, verbose, 4);
    passedTest[3] = testcase[3]->runTests();

    expected.clear();

    string input_ref5 = "[ A = G.B; B = [X = A; Y = D.A;]; C = A + D.B.G; D = [A = 2; B = [A = 3; G = E;]]; E = [C = A;]; ]";

    //these are non-sensical for now.
    expected.push_back(make_pair("A", ""));
    expected.push_back(make_pair("B", "A D.A"));
    expected.push_back(make_pair("C", "A D.B.G E ")); // A D.B.G C ?
    expected.push_back(make_pair("D", "E A")); //B.G A ?
    expected.push_back(make_pair("E", "A"));

    expected.push_back(make_pair("A", ""));
    expected.push_back(make_pair("B", "A D"));
    expected.push_back(make_pair("C", "A D C"));
    expected.push_back(make_pair("D", "E A"));
    expected.push_back(make_pair("E", "A"));

    testcase[4] = new GIRTestCase(5, input_ref5, expected, verbose, 5);
    passedTest[4] = testcase[4]->runTests();

    expected.clear();

    string input_ref6 = "[ A = G.B; B = [X = A; Y = D.A;]; C = A + D.B; D = [A = 2; B = E.C;]; E = [C = B.Y;]; ]";

    //these are non-sensical for now.
    expected.push_back(make_pair("A", ""));
    expected.push_back(make_pair("B", ""));
    expected.push_back(make_pair("C", "A D.B E.C"));
    expected.push_back(make_pair("D", "A E.C"));
    expected.push_back(make_pair("E", "A"));

    expected.push_back(make_pair("A", ""));
    expected.push_back(make_pair("B", ""));
    expected.push_back(make_pair("C", "A D E"));
    expected.push_back(make_pair("D", "E A"));
    expected.push_back(make_pair("E", "A"));

    testcase[5] = new GIRTestCase(5, input_ref6, expected, verbose, 6);
    passedTest[5] = testcase[5]->runTests();


    passed = passedTest[0];

    for(int i = 0; i < numTests; i++)
    {
        delete testcase[i];
        passed = passed && passedTest[i];
    }

    passed = passedTest[0] && passedTest[1] && 
        passedTest[2] && passedTest[3] && passedTest[4];
    return passed;
}
//}}}

//{{{ GIR helper
classad::References* gir_helper(classad::ClassAd* c, string attr, int verbose, bool full)
{

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
    classad::References::iterator itr2;
    std::vector<string>::iterator vecItr;

    //refs didn't have anything 
    if(ref->size() == 0 && expected.size() == 0)
    {
        return true;
    }
    else if(ref == NULL)
    {
        printf("ERROR: ref was null\n");
        return false;
    }

    //make sure they're the same size...
    if(ref->size() != expected.size())
    {
        printf("ERROR: size mismatch. ref size: %d expected size: %d\n", 
                ref->size(), expected.size());
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
                continue;
            }
        }

        if(!foundOne)
        {
            printf("ERROR: didn't find \"%s\" in expected list. ", (*itr).c_str());
            for(vecItr = expected.begin(); vecItr != expected.end(); vecItr++)
            {
                printf("%s ", (*vecItr).c_str());
            }
            printf("\n");
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
bool 
runAndCheckGIR(classad::ClassAd* c, string attr, string listString, bool full, int verbose )
{
    bool passed = false;
    bool passedTest = false;

    classad::References* retRefs;
    std::vector<string> expectedVec;

    /*
    int subStrStart = 0;
    int subStrEnd = 0;
    */

    //if "," doesn't exist, then skip the splitting-stage.
    bool singleAttr = (listString.find(" ") == string::npos);

    if(!singleAttr)
    {
    
        string word;
        istringstream iss(listString, istringstream::in);

        while ( iss >> word )
        {
            expectedVec.push_back(word);
        }

    }
    else if(listString.size() > 0)
    {
        expectedVec.push_back(listString); 
    }

    if(verbose == 2)
    {
        printf("Working on attr \"%s\".\n", attr.c_str());
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

char* getType(classad::ExprTree* expr)
{
    enum{
        LITERAL_NODE,
        ATTRREF_NODE,
        OP_NODE,
        FN_CALL_NODE,
        CLASSAD_NODE,
        EXPR_LIST_NODE
    };

    switch(expr->GetKind())
    {
        case LITERAL_NODE:
            return "LITERAL NODE";
        break;

        case ATTRREF_NODE:
            return "ATTRREF_NODE";
        break;

        case OP_NODE:
            return "OP_NODE";
        break;

        case FN_CALL_NODE:
            return "FN_CALL_NODE";
        break;

        case CLASSAD_NODE:
            return "CLASSAD_NODE";
        break;

        case EXPR_LIST_NODE:
            return "EXPR_LIST_NODE";
        break;


    }

    return "Huh?";
}

void printExprType(classad::ClassAd* c, string attr)
{

    enum{
        LITERAL_NODE,
        ATTRREF_NODE,
        OP_NODE,
        FN_CALL_NODE,
        CLASSAD_NODE,
        EXPR_LIST_NODE
    };
    classad::ExprTree *expr;

    if(c != NULL)
    {
        expr = c->Lookup(attr);
        if(expr != NULL)
        {
            switch(expr->GetKind() )
            {
                case LITERAL_NODE:
                    printf("attr \"%s\" is a LITERAL_NODE\n", attr.c_str());
                break;

                case ATTRREF_NODE:
                    printf("attr \"%s\" is an ATTRREF_NODE\n", attr.c_str());
                break;

                case OP_NODE:
                    printf("attr \"%s\" is an OP_NODE\n", attr.c_str());
                break;

                case FN_CALL_NODE:
                    printf("attr \"%s\" is an FN_CALL_NODE\n", attr.c_str());
                break;

                case CLASSAD_NODE:
                    {
                    printf("attr \"%s\" is an CLASSAD_NODE\n", attr.c_str());
                    vector< pair<string, classad::ExprTree*> >               attrs;
                    vector< pair<string, classad::ExprTree*> >:: iterator    itr; 

                    ((classad::ClassAd*)expr)->GetComponents(attrs);
                    for(itr = attrs.begin(); itr != attrs.end(); itr++){
                        printf("subattr of \"%s\" is \"%s\", with type %s.\n", attr.c_str(), (itr->first).c_str(), getType(itr->second) );
                    }
                    }
                break;

                case EXPR_LIST_NODE:
                    printf("attr \"%s\" is an EXPR_LIST_NODE\n", attr.c_str());
                break;

                default:
                    printf("No clue what attr \"%s\" is.\n", attr.c_str());
            }
        }
    }
}

void setAllFalse(bool* b)
{
    for(int i = 0; i < 5; i++)
    {
        b[i] = false;
    }
}

void setUpAndRun(int verbose)
{

    int numTests = 12;
    bool passedTest[numTests];


    for(int i = 0; i < numTests; i++)
    {
        passedTest[i] = false;
    }



    ClassAd *compC1, *compC2, *compC3, *compC4;

    setUpCompatClassAds(&compC1, &compC2, &compC3, &compC4, verbose);

    printf("Testing sPrintExpr...\n");
    passedTest[0] = test_sPrintExpr(compC1, verbose);
    printf("sPrintExpr %s.\n", passedTest[0] ? "passed" : "failed");
    printf("-------------\n");

    printf("Testing isValidAttrValue...\n");
    passedTest[1] = test_IsValidAttrValue(compC1, verbose);
    printf("IsValidAttrValue %s.\n", passedTest[1] ? "passed" : "failed");
    printf("-------------\n");


    printf("Testing fPrintAdAsXML and sPrintAdAsXML...\n");
    passedTest[2] = test_fPrintAdAsXML(compC1, verbose);
    printf("fPrintAdAsXML and sPrintAdAsXML %s.\n", passedTest[2] ? "passed" : "failed");
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


    printf("Testing EvalStringStdString...\n");
    passedTest[6] = test_EvalStringStdString(compC4, compC2, verbose);
    printf("EvalStringStdString %s.\n", passedTest[7] ? "passed" : "failed");
    printf("-------------\n");


    printf("Testing NextDirtyExpr...\n");
    passedTest[7] = test_NextDirtyExpr(compC4, verbose);
    printf("NextDirtyExpr %s.\n", passedTest[8] ? "passed" : "failed");
    printf("-------------\n");


    printf("Testing QuoteAdStringValue...\n");
    passedTest[8] = test_QuoteAdStringValue(compC4, verbose);
    printf("Quote String Value %s.\n", passedTest[9] ? "passed" : "failed");
    printf("-------------\n");


    printf("Testing EvalExprTree...\n");
    passedTest[9] = test_EvalExprTree(compC4,compC2, verbose);
    printf("EvalExprTree %s.\n", passedTest[10] ? "passed" : "failed");
    printf("-------------\n");

    printf("Testing GetInternalReferences...\n");
    passedTest[10] = test_GIR(verbose);
    printf("GIR %s.\n", passedTest[11] ? "passed" : "failed");
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
