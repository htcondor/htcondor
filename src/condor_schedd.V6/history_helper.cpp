
#include "condor_common.h"

#include <stdlib.h>

#include "reli_sock.h"
#include "subsystem_info.h"
#include "condor_daemon_core.h"
#include "historyFileFinder.h"
#include "backward_file_reader.h"
#include "condor_config.h"
#include "classad_oldnew.h"

#include <classad/classad.h>
#include <classad/source.h>
#include <classad/sink.h>

long failCount = 0;
long matchCount = 0;
long specifiedMatch = -1;
long maxAds = -1;
long adCount = 0;
Stream *output_sock = NULL;
classad::PrettyPrint sink;
std::vector<std::string> projection;

static void
setError(int code, std::string message)
{
	if (output_sock)
	{
		classad::ClassAd ad;
		ad.InsertAttr(ATTR_OWNER, 0);
		ad.InsertAttr(ATTR_ERROR_CODE, code);
		ad.InsertAttr(ATTR_ERROR_STRING, message);
		if (!putClassAd(output_sock, ad) || !output_sock->end_of_message())
		{
			fprintf(stderr, "Unable to write error message to remote client.\n");
		}
	}
	fprintf(stderr, "%s\n", message.c_str());
	exit(code);
}

// Sigh - mostly copy/paste from history.cpp

static void printJob(std::vector<std::string> & exprs, classad::ExprTree *constraintExpr)
{
	if (!exprs.size())
		return;

	classad::ClassAd ad;
	for (std::vector<std::string>::const_reverse_iterator it = exprs.rbegin(); it != exprs.rend(); it++)
	{
		if ( ! ad.Insert(*it)) {
			failCount++;
			fprintf(stderr, "Failed to create ClassAd expression; bad expr = '%s'\n", it->c_str());
			fprintf(stderr, "\t*** Warning: Bad history file; skipping malformed ad(s)\n" );
			exprs.clear();
			return;
		}
	}
	adCount++;

	classad::Value result;
	if (!ad.EvaluateExpr(constraintExpr, result))
	{
		return;
	}
	bool boolVal; int intVal; double doubleVal;
        if ((result.IsBooleanValue(boolVal) && boolVal) ||
            (result.IsIntegerValue(intVal)  && (intVal != 0)) ||
	    (result.IsRealValue(doubleVal) && ((int)((doubleVal)*100000))))
	{
		classad::ClassAd projected;
		if (projection.size())
		{
			for (std::vector<std::string>::const_iterator it=projection.begin(); it != projection.end(); it++)
			{
				classad::ExprTree *expr = ad.Lookup(*it);
				classad::ExprTree *copy = NULL;
				if (expr) copy = expr->Copy();
				if (copy) projected.Insert(*it, copy);
			}
		}
		else
		{
			projected.Update(ad);
		}
		// NOTE: I considered "not counting" empty ads.
		// However, this can have the side-effect of searching all the history if
		// the projection has a spelling mistake in it.
		//if (!projected.size()) return;
		if (output_sock == NULL)
		{
			std::string buff;
			sink.Unparse(buff, &projected);
			printf("%s\n", buff.c_str());
		}
		else if (!putClassAd(output_sock, projected))
		{
			failCount++;
		}
		matchCount++;
	}
}

static void
readHistoryFromFileEx(const char *filename, classad::ExprTree *constraintExpr)
{
	// In case of rotated history files, check if we have already reached the number of 
	// matches specified by the user before reading the next file
	if (((maxAds > 0) && (adCount >= maxAds)) || ((specifiedMatch > 0) && (matchCount >= specifiedMatch)))
	{
		return;
	}

	// do backwards reading.
	BackwardFileReader reader(filename, O_RDONLY);
	if (reader.LastError())
	{
		// report error??
		setError(5, "Error opening history file");
	}

	std::string line;
	std::string banner_line;

	while (reader.PrevLine(line))
	{
		if (starts_with(line.c_str(), "*** "))
		{
			banner_line = line;
			break;
		}
	}

	std::vector<std::string> exprs; exprs.reserve(100);
	while (reader.PrevLine(line))
	{
                if (starts_with(line.c_str(), "*** "))
		{
			if (exprs.size() > 0) {
				printJob(exprs, constraintExpr);
				exprs.clear();
			}
			banner_line = line;
			if (((maxAds > 0) && (adCount >= maxAds)) || ((specifiedMatch > 0) && (matchCount >= specifiedMatch)))
				break;
		}
		else if (!line.empty())
		{
			const char * psz = line.c_str();
			while (*psz == ' ' || *psz == '\t') ++psz;
			if (*psz != '#')
			{
				exprs.push_back(line);
			}
			printf("%s\n", line.c_str());
		}
	}
	if (exprs.size() > 0)
	{
		if (((maxAds <= 0) || (adCount < maxAds)) && ((specifiedMatch <= 0) || (matchCount < specifiedMatch)))
			printJob(exprs, constraintExpr);
		exprs.clear();
	}
	reader.Close();
}

void
main_init(int argc, char *argv[])
{
	int i=0;
        for (char **ptr = argv + 1; *ptr && (i < argc - 1); ptr++,i++) {
                if(ptr[0][0] == '-') {
                        argv++;
			argc--;
                }
		else break;
	}

	if (argc != 5)
	{
		fprintf(stderr, "Usage: %s -t REQUIREMENT PROJECTION MATCH_COUNT MAX_ADS\n", argv[0]);
		fprintf(stderr, "- Use an empty string to return all attributes\n");
		fprintf(stderr, "- Use a negative number for match count for all matches\n");
		fprintf(stderr, "- Use a negative number for considering an unlimited number of history ads\n");
		fprintf(stderr, "If there are no inherited DaemonCore sockets, print results to stdout\n");
		exit(1);
	}

	classad::ClassAdParser parser;
	classad::ExprTree *requirements;
	if (!parser.ParseExpression(argv[1], requirements))
	{
		setError(6, "Unable to parse the requirements expression");
	}

	StringList projection_sl(argv[2]);
	projection.reserve(projection_sl.number());
	projection_sl.rewind();
	char * attr;
	while ((attr = projection_sl.next()))
	{
		projection.push_back(attr);
	}

	errno = 0;
	specifiedMatch = strtol(argv[3], NULL, 10);
	if (errno)
	{
		setError(7, "Error when converting match count to long");
	}
	maxAds = strtol(argv[4], NULL, 10);
	if (errno)
	{
		setError(8, "Error when converting max ads to long");
	}

	Stream **socks = daemonCore->GetInheritedSocks();
	if (socks && socks[0] && socks[0]->type() == Stream::reli_sock)
	{
		output_sock = socks[0];
	}

	// The user didn't specify the name of the file to read, so we read
	// the history file, and any backups (rotated versions). 
	int numHistoryFiles;
	char **historyFiles;

	historyFiles = findHistoryFiles("HISTORY", &numHistoryFiles);
	if (!historyFiles) {
		setError(8, "Error: No history file is defined\n");
	}
	if (historyFiles && numHistoryFiles > 0) {
		int fileIndex;
		for(fileIndex = numHistoryFiles - 1; fileIndex >= 0; fileIndex--) {
			readHistoryFromFileEx(historyFiles[fileIndex], requirements);
			free(historyFiles[fileIndex]);
		}
		free(historyFiles);
	}

	classad::ClassAd ad;
	ad.InsertAttr(ATTR_OWNER, 0);
	ad.InsertAttr(ATTR_NUM_MATCHES, matchCount);
	ad.InsertAttr("MalformedAds", failCount);
	ad.InsertAttr("AdCount", adCount);
	if (output_sock)
	{
		if (!putClassAd(output_sock, ad) || !output_sock->end_of_message())
		{
			fprintf(stderr, "Failed to write final ad to client");
			exit(1);
		}
	}
	std::string buff;
	sink.Unparse(buff, &ad);
	printf("%s\n", buff.c_str());

	DC_Exit(0);
}


void
main_config()
{
}


void
main_shutdown_fast()
{
	DC_Exit(0);
}


void
main_shutdown_graceful()
{
	DC_Exit(0);
}

int
main(int argc, char **argv)
{
	set_mySubSystem( "HISTORY_HELPER", SUBSYSTEM_TYPE_TOOL );
	dc_main_config = main_config;
	dc_main_shutdown_fast = main_shutdown_fast;
	dc_main_shutdown_graceful = main_shutdown_graceful;
	dc_main_init = main_init;
	return dc_main( argc, argv );
}


