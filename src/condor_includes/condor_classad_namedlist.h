/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
  *
  * Condor Software Copyright Notice
  * Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
  * University of Wisconsin-Madison, WI.
  *
  * This source code is covered by the Condor Public License, which can
  * be found in the accompanying LICENSE.TXT file, or online at
  * www.condorproject.org.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
  * AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
  * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  * WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
  * FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
  * HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
  * MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
  * ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
  * PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
  * RIGHT.
  *
  ****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/
#ifndef CONDOR_CLASSAD_NAMEDLIST_H
#define CONDOR_CLASSAD_NAMEDLIST_H

#include "condor_common.h"
#include "condor_classad.h"
#include "simplelist.h"

// A name / ClassAd pair to manage together
class NamedClassAd
{
  public:
	NamedClassAd( const char *name, ClassAd *ad = NULL );
	~NamedClassAd( void );
	char *GetName( void ) { return myName; };
	ClassAd *GetAd( void ) { return myClassAd; };
	void ReplaceAd( ClassAd *newAd );

  private:
	char	*myName;
	ClassAd	*myClassAd;
};

class NamedClassAdList
{
  public:
	NamedClassAdList( void );
	~NamedClassAdList( void );

	int Register( const char *name );
	int	Replace( const char *name, ClassAd *ad );
	int	Delete( const char *name );
	int	Publish( ClassAd *ad );

  private:
	SimpleList<NamedClassAd*>		ads;

};

#endif
