#include "condor_common.h"

#include "NegotiatorPlugin.h"
#include "condor_classad.h"

struct ExampleNegotiatorPlugin : public NegotiatorPlugin
{
	void
	initialize()
	{
		printf("Init\n");
	}

	void
	update(const ClassAd &ad)
	{
		printf("Update\n");
	}
};

static ExampleNegotiatorPlugin instance;
