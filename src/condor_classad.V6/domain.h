#ifndef __DOMAIN_H__
#define __DOMAIN_H__

#include "stringSpace.h"
#include "extArray.h"

class ClassAdDomainManager
{
 	 public:
		ClassAdDomainManager ();
		~ClassAdDomainManager ();

		void getCanonicalDomain (char *domainName, SSString &canonical)
		{
   			domainNameSpace.getCanonical (domainName, canonical);
		}

		void getDomainSchema (SSString domainName, StringSpace *&schema)
		{
			schema = schemaArray[domainName.getIndex ()];
		}

		void getDomainSchema (int index, StringSpace *&schema)
		{
			schema = schemaArray[index];
		}

		void getDomainSchema (char *, int &, StringSpace *&);

  	private:
		ExtArray<StringSpace *> 	schemaArray;
		StringSpace             	domainNameSpace;
};

extern ClassAdDomainManager domMan;			// domain manager

#endif//__DOMAIN_H__
