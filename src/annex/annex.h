#ifndef _CONDOR_ANNEX_H
#define _CONDOR_ANNEX_H

bool validateLease( time_t endOfLease, std::string & validationError );
EC2GahpClient * startOneGahpClient( const std::string & publicKeyFile, const std::string & /* serviceURL */ );
void InsertOrUpdateAd( const std::string & id, ClassAd * command, ClassAdCollection * log );

#endif /* _CONDOR_ANNEX_SETUP_H */
