#ifndef __DC_STORK_H__
#define __DC_STORK_H__

#define WANT_NAMESPACES
#include "classad_distribution.h"

class DCStork {
 public:
	DCStork(const char * host);
	~DCStork();

	int SubmitJob (const classad::ClassAd * request,
				   const char * cred, 
				   const int cred_size, 
				   char *& id);

	int RemoveJob (const char * id);

	int ListJobs (int & size, 
				  classad::ClassAd ** & result);

	int QueryJob (const char * id, 
						 classad::ClassAd * & result);

	const char * GetLastError() const;

 private:
	void ClearError();
	
	char * error_reason;
	char * sinful_string;

};
#endif
