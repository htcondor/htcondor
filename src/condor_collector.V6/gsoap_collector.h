//gsoap condor service name: condorCollector
//gsoap condor service style: document
//gsoap condor service encoding: literal
//gsoap condor service namespace: urn:condor

#import "gsoap_daemon_core_types.h"

#import "gsoap_daemon_core.h"

int condor__queryStartdAds(char *constraint,
						   struct condor__ClassAdStructArray & result);

int condor__queryScheddAds(char *constraint,
						   struct condor__ClassAdStructArray & result);

int condor__queryMasterAds(char *constraint,
						   struct condor__ClassAdStructArray & result);

int condor__querySubmittorAds(char *constraint,
							  struct condor__ClassAdStructArray & result);

int condor__queryLicenseAds(char *constraint,
							struct condor__ClassAdStructArray & result);

int condor__queryStorageAds(char *constraint,
							struct condor__ClassAdStructArray & result);

int condor__queryAnyAds(char *constraint,
						struct condor__ClassAdStructArray & result);


