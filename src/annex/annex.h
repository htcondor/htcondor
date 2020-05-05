#ifndef _CONDOR_ANNEX_H
#define _CONDOR_ANNEX_H

bool validateLease( time_t endOfLease, std::string & validationError );
EC2GahpClient * startOneEC2GahpClient( const std::string & publicKeyFile );
void InsertOrUpdateAd( const std::string & id, ClassAd * command, ClassAdCollection * log );

#endif /* _CONDOR_ANNEX_SETUP_H */
