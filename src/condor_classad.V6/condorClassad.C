#include "condor_common.h"
#include "condorClassad.h"


CondorClassAd::
CondorClassAd()
{
}


CondorClassAd::
CondorClassAd( ClassAd *adl, ClassAd *adr ) : ClassAd()
{
	ClassAd	*ad;
	Value	val;

	// the left context
	insert( "adcl", "[self=.adcl.ad;other=.adcr.ad;my=self;target=other]" );
	evaluateAttr( "adcl", val );
	val.isClassAdValue( ad );
	ad->insert( "ad", adl );

	// the right context
	insert( "adcr", "[self=.adcr.ad;other=.adcl.ad;my=self;target=other]" );
	evaluateAttr( "adcr", val );
	val.isClassAdValue( ad );
	ad->insert( "ad", adr );
}


CondorClassAd::
~CondorClassAd()
{
}


CondorClassAd* CondorClassAd::
makeCondorClassAd( ClassAd *adl, ClassAd *adr )
{
	return( new CondorClassAd( adl, adr ) );
}


bool CondorClassAd::
replaceLeftAd( ClassAd *ad )
{
	Value 	val;
	ClassAd *oad;

	evaluateExpr( "adl.ad", val );
	if( !val.isClassAdValue( oad ) ) return false;
	return( oad->insert( "ad", ad ) );
}


bool CondorClassAd::
replaceRightAd( ClassAd *ad )
{
	Value 	val;
	ClassAd *oad;

	evaluateExpr( "adr.ad", val );
	if( !val.isClassAdValue( oad ) ) return false;
	return( oad->insert( "ad", ad ) );
}


ClassAd *CondorClassAd::
getLeftAd()
{
	ClassAd	*ad;
	Value	val;

	evaluateExpr( "adl.ad" , val );
	return( val.isClassAdValue( ad ) ? ad : 0 );
}


ClassAd *CondorClassAd::
getRightAd()
{
	ClassAd	*ad;
	Value	val;

	evaluateExpr( "adr.ad" , val );
	return( val.isClassAdValue( ad ) ? ad : 0 );
}
