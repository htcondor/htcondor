#ifndef _CONDOR_DC_ANNEXD_H
#define _CONDOR_DC_ANNEXD_H

class DCAnnexd : public Daemon {
	public:
		DCAnnexd( const char * name = NULL, const char * pool = NULL );
		~DCAnnexd();

		bool sendBulkRequest( ClassAd const * request, ClassAd * reply, int timeout = -1 );
};

#endif
