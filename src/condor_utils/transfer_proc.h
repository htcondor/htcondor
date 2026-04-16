#ifndef   _CONDOR_TRANSFER_PROC_H
#define   _CONDOR_TRANSFER_PROC_H

//
// We reserve procIDs <= -1000 so we can distinguish different transfer shadows
// from each other.  At some point, we'll figure out a better storage mechanism
// for soft (and hard) state for the schedd than abusing the clusterID/procID
// tuple, and at that point, this should all change.
//
const int FIRST_TRANSFER_PROC_ID = -1000;

inline
int transferToPromptingProcID( int transferShadowProcID ) {
	return (-1 * transferShadowProcID) + FIRST_TRANSFER_PROC_ID;
}

inline
int promptingToTransferProcID( int jobProcID ) {
	return -1 * (jobProcID - FIRST_TRANSFER_PROC_ID);
}

inline
bool isTransferShadowProcID( int procID ) {
	return procID <= FIRST_TRANSFER_PROC_ID;
}

inline
bool isInvalidProcID( int procID ) {
	return FIRST_TRANSFER_PROC_ID < procID && procID < 0;
}

#endif /* _CONDOR_TRANSFER_PROC_H */
