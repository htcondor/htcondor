/*
    This file implements the Reqexp class, which contains methods and
    data to manipulate the requirements expression of a given
    resource.

   	Written 9/29/97 by Derek Wright <wright@cs.wisc.edu>
*/

#include "startd.h"
static char *_FileName_ = __FILE__;

Reqexp::Reqexp( ClassAd* ca )
{
	this->ca = ca;
	char tmp[1024];
	sprintf( tmp, "%s = %s", ATTR_REQUIREMENTS, "START" );
	origreqexp = strdup( tmp );
	ca->SetRequirements( tmp );
	modified = 0;
}


Reqexp::~Reqexp()
{
	free( origreqexp );
}


int
Reqexp::eval() 
{
	ExprTree *tree;
	ClassAd ad;
	int tmp;
	EvalResult res;

	Parse( origreqexp, tree );
	tree->EvalTree( ca, &ad, &res);
	delete tree;

	if( res.type != LX_BOOL ) {
		return -1;
	} else {
		return res.b;
	}
}


void
Reqexp::restore()
{
	if( modified ) {
		ca->InsertOrUpdate( origreqexp );
		modified = 0;
	}
}


void
Reqexp::unavail() 
{
	char tmp[100];
	sprintf( tmp, "%s = False", ATTR_REQUIREMENTS );
	ca->InsertOrUpdate( tmp );
	modified = 1;
}


void
Reqexp::avail() 
{
	char tmp[100];
	sprintf( tmp, "%s = True", ATTR_REQUIREMENTS );
	ca->InsertOrUpdate( tmp );
	modified = 1;
}


void
Reqexp::pub()
{
	switch( this->eval() ) {
	case 0:							// requirements == false
		this->unavail();
		break;
	case 1:							// requirements == true
		this->avail();		
		break;
	case -1:						// requirements undefined
		this->restore();			
		break;
	}
}
