#include "condor_common.h"
#include "matchClassad.h"

BEGIN_NAMESPACE( classad )

MatchClassAd::
MatchClassAd()
{
	lCtx = rCtx = NULL;
	lad = rad = NULL;
	InitMatchClassAd( NULL, NULL );
}


MatchClassAd::
MatchClassAd( ClassAd *adl, ClassAd *adr ) : ClassAd()
{
	lad = rad = lCtx = rCtx = NULL;
	InitMatchClassAd( adl, adr );
}


MatchClassAd::
~MatchClassAd()
{
}


MatchClassAd* MatchClassAd::
MakeMatchClassAd( ClassAd *adl, ClassAd *adr )
{
	return( new MatchClassAd( adl, adr ) );
}

bool MatchClassAd::
InitMatchClassAd( ClassAd *adl, ClassAd *adr )
{
	Source	src;

		// clear out old info
	Clear( );
	lad = rad = NULL;
	lCtx = rCtx = NULL;

		// convenience expressions
	Insert( "symmetricMatch", "leftMatchesRight && rightMatchesLeft" );
	Insert( "leftMatchesRight", "adcr.ad.requirements" );
	Insert( "rightMatchesLeft", "adcl.ad.requirements" );
	Insert( "leftRankValue", "adcl.ad.rank" );
	Insert( "rightRankValue", "adcr.ad.rank" );

		// the left context
	src.SetSource( "[other=.adcr.ad;my=ad;target=other;super=other;ad=[]]" );
	if( !src.ParseClassAd( lCtx ) ) {
		Clear( );
		lCtx = NULL;
		rCtx = NULL;
		return( false );
	}
	if( adl ) {
		lCtx->Insert( "ad", adl );
	} else {
		Value val;
		lCtx->EvaluateAttr( "ad", val );
		val.IsClassAdValue( adl );
	}

		// the right context
	src.SetSource( "[other=.adcl.ad;my=ad;target=other;super=other;ad=[]]" );
	if( !src.ParseClassAd( rCtx ) ) {
		delete lCtx;
		lCtx = rCtx = NULL;
		return( false );
	}
	if( adr ) {
		rCtx->Insert( "ad", adr );
	} else {
		Value val;
		rCtx->EvaluateAttr( "ad", val );
		val.IsClassAdValue( adr );
	}

		// insert the left and right contexts
	Insert( "adcl", lCtx );
	Insert( "adcr", rCtx );

	lad = adl;
	rad = adr;

	return( true );
}


bool MatchClassAd::
ReplaceLeftAd( ClassAd *ad )
{
	lad = ad;
	return( lCtx->Insert( "ad", ad ) );
}


bool MatchClassAd::
ReplaceRightAd( ClassAd *ad )
{
	rad = ad;
	return( rCtx->Insert( "ad", ad ) );
}


ClassAd *MatchClassAd::
GetLeftAd()
{
	return( lad );
}


ClassAd *MatchClassAd::
GetRightAd()
{
	return( rad );
}


ClassAd *MatchClassAd::
GetLeftContext( )
{
	return( lCtx );
}


ClassAd *MatchClassAd::
GetRightContext( )
{
	return( rCtx );
}


ClassAd *MatchClassAd::
RemoveLeftAd( )
{
	lCtx->Remove( "ad" );
	return( lad );
}


ClassAd *MatchClassAd::
RemoveRightAd( )
{
	rCtx->Remove( "ad" );
	return( rad );
}

END_NAMESPACE // classad
