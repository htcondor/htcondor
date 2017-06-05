#ifndef _CONDOR_GENERATE_ID_H
#define _CONDOR_GENERATE_ID_H

void generateAnnexID( std::string & annexID );
void generateCommandID( std::string & commandID );
void generateClientToken(	const std::string & annexID,
							std::string & clientToken );

#endif /* _CONDOR_GENERATE_ID_H */
