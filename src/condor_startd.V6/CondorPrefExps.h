#ifndef PREF_EXP_FILTER_H
#define PREF_EXP_FILTER_H

#include "condor_classad.h"

typedef int tTier;

class CondorPrefExps
{
  private:

  /* an array holding all pointers to PREF_EXPs 
     for easy access */
  static ExprTree* PrefExps[10];

  // lowest PREF_EXP tier: currently, this may not exceed 9
  static tTier LowestTier;

  // set true whenever the ORIGINAL Requirements expression
  // (as read from the config file) is modified
  static bool ReqExModified;

  static char OrigReqEx[200];

  public:

  enum eStatus {eOK, eFAIL,eNOTIERS,eNOMATCH};

  CondorPrefExps(void);

  // sets up data structures to keep track of all PREF_EXP's listed 
  // in the config file
  static void Initialize(ClassAd*);

  // locates and sets t to the PrefExp tier (0 highest, 9 lowest) that 
  // matches against
  // the given pair of classAds: Returns eNOTIERS if
  // no PrefExps are on record (t is undisturbed), eOK if there was a match,
  // eNOMATCH if no match was found. In the last case,
  // the value of t is one greater than the lowest tier

  static eStatus MatchingPrefExp(ClassAd* One, ClassAd* Two, tTier& t);

  // returns the PrefExp associated with t: 0 if none
  static ExprTree* PrefExp(tTier t);

  // generates an expression that is the logical OR of all PrefExps
  // that are of higher tier than t (0 highest).  The generated
  // ExprTree is stored in *e. The routine creates memory for
  // e: de-allocating it is the responsibility of the caller

  // In case there are no PrefExps on record 0 is returned through
  // e, NO memory for e is allocated. The routine returns eOK if all 
  // went well and eFAIL if an exceptional condition (such as inability 
  // to allocate memory) is encountered. It returns eNOTIERS
  // if no PrefTiers were declared in the config file

  static eStatus GenerateModifier(tTier t, ExprTree*& e);

  // modifies the Requirements expression of mc
  // depending on what PrefEx of mc the job satisfies
  // returns {eOK, all went well: ReqEx was modified},
  // {eFAIL, ReqEx was not modified},
  // {eNOTIERS, no PrefExps are on record}
  static eStatus ModifyReqEx(ClassAd* mc, ClassAd* job);

  // restores the Requirements expression of mc
  // to the one read from the config file
  // return value same as above
  static eStatus RestoreReqEx(ClassAd* mc);
};

#endif

