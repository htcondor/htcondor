#ifndef __DOMAIN_TOOLS_H
#define __DOMAIN_TOOLS_H

// returns true if the two specified accounts and domains match
bool
domainAndNameMatch( const char *account1, 
		                 const char *account2,
						 const char *domain1, 
						 const char *domain2 );

// takes string of the form <DOMAIN_NAME>\<ACCOUNT_NAME> 
// and chops it into two strings
void
getDomainAndName( char* namestr, char* &domain, char* &name );


#endif  /* __DOMAIN_TOOLS_H */
