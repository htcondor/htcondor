#include <stdio.h>
#include "predefContexts.h"
#include "source.h"

extern "C" int SetSyscalls () { return 0; }

int main (void)
{
	char			buffer[1024];
	int				buflen = 1024;
	Source			source;
	Sink			sink;
	ClassAd 		*ad1, *ad2, *ad3;
	int				scope;
	StandardContext se;
	Value			val;
	StdScopes		pos;

	sink.setSink (stdout);

	printf ("\n\nAd1: ");
	gets (buffer);
	printf ("ClassAd is: %s\n", buffer);
	source.setSource (buffer, buflen);
	ad1 = ClassAd::fromSource (source);
	if (!ad1) { cerr << "source" << endl; exit (1); }
	ad1->toSink (sink);

	printf ("\n\nAd2: ");
	gets (buffer);
	printf ("ClassAd is: %s\n", buffer);
	source.setSource (buffer, buflen);
	ad2 = ClassAd::fromSource (source);
	if (!ad2) { cerr << "source" << endl; exit (1); }

	printf ("\n\nAd3: ");
	gets (buffer);
	printf ("ClassAd is: %s\n", buffer);
	source.setSource (buffer, buflen);
	ad3 = ClassAd::fromSource (source);
	if (!ad3) { cerr << "source" << endl; exit (1); }

	// plugin ads
	se.pluginAd (STDCTX_LEFT, ad1);
	se.pluginAd (STDCTX_RIGHT,ad2);
	se.pluginAd (STDCTX_LENV, ad3);

	*buffer = 0;
	while (strcmp (buffer, "done"))
	{
		printf ("Enter attrname: ");
		scanf ("%s", buffer);
		printf ("Enter scope: ");
		scanf ("%d", &scope);
		pos = (StdScopes) scope;

		se.evaluate (buffer, pos, val);
		val.toSink (sink);
		sink.flushSink();
		printf ("\n");
	}
	
	delete ad1;
	delete ad2;
	delete ad3;

	return 0;
}
