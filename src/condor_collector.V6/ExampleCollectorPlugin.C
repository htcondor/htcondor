#include "condor_common.h"

#include "CollectorPlugin.h"
#include "condor_classad.h"

struct ExampleCollectorPlugin : public CollectorPlugin
{
	void
	initialize()
	{
		printf("Init\n");
	}

	void
	update(int command, const ClassAd &ad)
	{
		printf("Update\n");
	}

	void
	invalidate(int command, const ClassAd &ad)
	{
		printf("Invalidate\n");
	}
};

static ExampleCollectorPlugin instance;
