#include "CondorPrefExps.h"
#include "condor_debug.h"
#include "condor_attributes.h"
 
#include<iostream.h>
#include<stdlib.h>
#include<string.h>

// initialize static members
ExprTree* CondorPrefExps::PrefExps[10];
tTier CondorPrefExps::LowestTier;
bool CondorPrefExps::ReqExModified;
char CondorPrefExps::OrigReqEx[200];

CondorPrefExps::CondorPrefExps()
{
  LowestTier = -1; // assume no PREF_EXPs are present
}

void CondorPrefExps::Initialize(ClassAd* mc)
{
  char* PrefExpNames[] = { "PREF_EXP0", "PREF_EXP1", "PREF_EXP2", "PREF_EXP3",
			   "PREF_EXP4", "PREF_EXP5", "PREF_EXP6", "PREF_EXP7",
			   "PREF_EXP8", "PREF_EXP9"};
  LowestTier = -1; // assume no PREF_EXPs are present

  ReqExModified = false;

  // store the requirements expression
  (mc->Lookup(ATTR_REQUIREMENTS))->PrintToStr(OrigReqEx);

  dprintf(D_ALWAYS,"Searching for PREF_EXPs..\n");
  for(tTier i = 0; i<=9;i++)
  {
    if(PrefExps[++LowestTier]=mc->Lookup(PrefExpNames[i]))
    {
      // "PREF_EXP"i located
    }
    else LowestTier--;
  }
  dprintf(D_ALWAYS,"Found %d PREF_EXPs\n",(LowestTier+1));
}

ExprTree* CondorPrefExps::PrefExp(tTier t)
{
  return (LowestTier>=0)?PrefExps[t]:0;
}

CondorPrefExps::eStatus CondorPrefExps::RestoreReqEx(ClassAd* mc)
{
  if(ReqExModified)
  {
    if(mc->InsertOrUpdate(OrigReqEx))
    {
      dprintf(D_ALWAYS,"Restored Requirements Expression\n");
      ReqExModified = false;
    }
  }
  return (ReqExModified)?eFAIL:eOK;
}

CondorPrefExps::eStatus CondorPrefExps::ModifyReqEx(ClassAd* mc, ClassAd* job)
{
  dprintf(D_ALWAYS,"Modifying Requirements Expression\n");

  eStatus status;

  if((!mc)||(!job)) return eFAIL; // Requirements not modified

  tTier PrefTier;
  status=CondorPrefExps::MatchingPrefExp(mc,job,PrefTier);
  if(status==eNOTIERS) return status;

  char ModReqEx[200*(PrefTier+1)];
  strcpy(ModReqEx,"");

  if(PrefTier==0)
  {
    // A match at the highest tier
    sprintf(ModReqEx,"%s = FALSE", ATTR_REQUIREMENTS);
  }
  else
  {
    ExprTree* modifier;

    if((status=CondorPrefExps::GenerateModifier(PrefTier,modifier))!=eOK)
    {
      if(modifier) delete modifier;
      return status;
    }

    char modif[200*(PrefTier+1)];
    strcpy(modif,"");
    char temp[200*(PrefTier+1)];
    strcpy(temp,"");

    modifier->PrintToStr(modif);

    (mc->Lookup(ATTR_REQUIREMENTS))->PrintToStr(temp);
    strcat(temp," && ");
    strcat(temp,modif);

    strcat(ModReqEx,temp);
    delete modifier;
  }
  if(mc->InsertOrUpdate(ModReqEx))
  {
    ReqExModified = true;
    return eOK;
  }
  return eFAIL;
}

CondorPrefExps::eStatus CondorPrefExps::MatchingPrefExp(ClassAd* One, 
							ClassAd* Two,
							tTier& t)
{
  if(LowestTier<0)
  {
    // no PrefExps on record
    return eNOTIERS;
  }

  int i;
  EvalResult Result;
  t = LowestTier+1; // assume no tiers match
  for(i=0;i<=LowestTier;i++) 
  {
    // check every PREF_EXP recorded
    //One->EvalBool(PrefExps[i],Two,retval))
    if(PrefExps[i]->EvalTree(One,Two,&Result))
    {
      if((Result.type==LX_BOOL)&&(Result.b))
      {
	t=i;
	return eOK;
      }
    }
  }
  return (t<=LowestTier)?eOK:eNOMATCH;
}

CondorPrefExps::eStatus CondorPrefExps::GenerateModifier(tTier t, ExprTree*& e)
{
  if(LowestTier<0)
  {
    // no PrefExps on record
    e = 0;
    return eNOTIERS;
  }

  if(t==0) // highest tier
  {
    // return false through e
    e = new Boolean(0);
    return (e)?eOK:eFAIL;
  }

  char Modifier[200*t];
  strcpy(Modifier,"");
  for(tTier i=0; i<t; i++)
  {
    char temp[200]="";
    (PrefExps[i]->RArg())->PrintToStr(temp);
    strcat(Modifier,temp);
    dprintf(D_FULLDEBUG,"i=%d, temp = %s, Modifier = %s\n",i,temp,Modifier);
    if(i<(t-1)) strcat(Modifier,"||");
  }

  dprintf(D_ALWAYS,"Modifier=%s\n",Modifier);
  if(Parse(Modifier,e))
  {
    e = 0;
    return eFAIL;
  }
  else
  {
    char temp[200];
    e->PrintToStr(temp);
    return (e)?eOK:eFAIL;
  }
}
