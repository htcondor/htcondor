#import "gsoap_daemon_core_types.h"

//gsoap condor service style: document
//gsoap condor service encoding: literal

int condor__getInfoAd(void *_,
					  struct condor__getInfoAdResponse {
						  struct condor__ClassAdStructAndStatus response;
					  } & ad);
int condor__getVersionString(void *_,
							 struct condor__getVersionStringResponse {
								 struct condor__StringAndStatus response;
							 } & verstring);
int condor__getPlatformString(void *_,
							  struct condor__getPlatformStringResponse {
								  struct condor__StringAndStatus response;
							  } & verstring);
