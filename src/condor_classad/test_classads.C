/***************************Copyright-DO-NOT-REMOVE-THIS-LINE**
 * CONDOR Copyright Notice
 *
 * See LICENSE.TXT for additional notices and disclaimers.
 *
 * Copyright (c)1990-2001 CONDOR Team, Computer Sciences Department, 
 * University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
 * No use of the CONDOR Software Program Source Code is authorized 
 * without the express consent of the CONDOR Team.  For more information 
 * contact: CONDOR Team, Attention: Professor Miron Livny, 
 * 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
 * (608) 262-0856 or miron@cs.wisc.edu.
 *
 * U.S. Government Rights Restrictions: Use, duplication, or disclosure 
 * by the U.S. Government is subject to restrictions as set forth in 
 * subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
 * Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
 * (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
 * 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
 * Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
 * WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
****************************Copyright-DO-NOT-REMOVE-THIS-LINE**/

#include "condor_common.h"
#include "condor_classad.h"
#include "stringSpace.h"

/*----------------------------------------------------------
 *
 *                        Global Variables
 *
 *----------------------------------------------------------*/
#define LARGE_NUMBER_OF_CLASSADS 10000

#ifdef USE_STRING_SPACE_IN_CLASSADS
extern StringSpace classad_string_space; // for debugging only!
#endif

#define NUMBER_OF_CLASSAD_STRINGS (sizeof(classad_strings)/sizeof(char *))
char *classad_strings[] = 
{
	"A = 1, B=2, C = 3",
	"A = 1, B=2, C = 3, D = \"alain\", MyType=\"foo\", TargetType=\"blah\"",

	"Rank = (Memory >= 50)",

    "Env = \"CPUTYPE=i86pc;GROUP=unknown;LM_LICENSE_FILE=/p/multifacet/"
            "projects/simics/dist10/v9-sol7-gcc/sys/flexlm/license.dat;"
            "SIMICS_HOME=.;SIMICS_EXTRA_LIB=./modules;PYTHONPATH=./modules;"
            "MACHTYPE=i386;SHELL=/bin/tcsh;PATH=/s/std/bin:/usr/afsws/bin:"
        	"/usr/ccs/bin:/usr/ucb:/bin:/usr/bin:/usr/X11R6/bin:/unsup/condor/bin:.\"",

    "MyType=\"Job\", TargetType=\"Machine\", Owner = \"alain\", Requirements = (TARGET.Memory > 50)",
    "MyType=\"Machine\", TargetType=\"Job\", Memory = 100,  Requirements = (TARGET.owner == \"alain\")",
    "MyType=\"Machine\", TargetType=\"Job\", Memory = 40, Requirements = (TARGET.owner == \"alain\")",
    "MyType=\"Machine\", TargetType=\"Job\", Memory = 100, Requirements = (TARGET.owner != \"alain\")",

	/*
    "Requirements = ((Memory >= 50) && ((Arch == \"INTEL\") && (Opsys == \"LINUX\")) && (FileSystemDomain == \".cs.wisc.edu\" || FileSystemDomain == \"cs.wisc.edu\")) && (Disk >= DiskUsage),"
    "Rank = ((Memory - (15.000000 / TotalVirtualMachines)) * 1024) > (Imagesize * (2 - (IsDedicated =?= TRUE)))",
	*/
#if 0
	/* A job class ad */
    "MyType = \"Job\","
    "TargetType = \"Machine\","
    "ClusterId = 10141,"
    "QDate = 999707236,"
    "CompletionDate = 0,"
    "Owner = \"sandvik\","
    "NumCkpts = 0,"
    "NumRestarts = 0,"
    "CommittedTime = 0,"
    "SubmitPool = \"condor.cs.wisc.edu\","
    "CondorVersion = \"$CondorVersion: 6.2.1 Jul 27 2001 $\","
    "CondorPlatform = \"$CondorPlatform: INTEL-LINUX-GLIBC21 $\","
    "RootDir = \"/\","
    "Iwd = \"/afs/cs.wisc.edu/p/condor/workspaces/sandvik/sqmc2/ll24/run1\","
    "JobUniverse = 5,"
    "Cmd = \"/afs/cs.wisc.edu/p/condor/workspaces/sandvik/sqmc2/ll24/../d24.out\","
    "MinHosts = 1,"
    "WantRemoteSyscalls = FALSE,"
    "WantCheckpoint = FALSE,"
    "JobPrio = 0,"
    "NiceUser = FALSE,"
    "Env = \"\","
    "JobNotification = 0,"
    "UserLog = \"qmc.log\","
    "CoreSize = 2147483647,"
    "KillSig = 15,"
    "Rank = ((Memory - (15.000000 / TotalVirtualMachines)) * 1024) > (Imagesize * (2 - (IsDedicated =?= TRUE))),"
    "In = \"read.in\","
    "Out = \"worker.out\","
    "Err = \"worker.err\","
    "BufferSize = 524288,"
    "BufferBlockSize = 32768,"
    "TransferFiles = \"ONEXIT\","
    "ExecutableSize = 158,"
    "DiskUsage = 158,"
    "Requirements = ((Memory >= 50) && ((Arch == \"INTEL\") && (Opsys == \"LINUX\")) && (,"
    "FileSystemDomain == \".cs.wisc.edu\" || FileSystemDomain == \"cs.wisc.edu\")) && (Disk >= DiskUsage),"
    "Args = \"\","
    "ProcId = 0,"
    "JobStartDate = 999707387,"
    "FileReadCount = 0.000000,"
    "FileReadBytes = 0.000000,"
    "FileWriteCount = 0.000000,"
    "FileWriteBytes = 0.000000,"
    "FileSeekCount = 0.000000,"
    "ImageSize = 158,"
    "ExitStatus = 0,"
    "LocalUserCpu = 0.000000,"
    "LocalSysCpu = 0.000000,"
    "RemoteUserCpu = 0.000000,"
    "RemoteSysCpu = 0.000000,"
    "BytesSent = 1292408.000000,"
    "BytesRecvd = 2304.000000,"
    "RSCBytesSent = 4776.000000,"
    "RSCBytesRecvd = 2304.000000,"
    "LastVacateTime = 999800544,"
    "RemoteWallClockTime = 83527.000000,"
    "LastRemoteHost = \"c2-149.cs.wisc.edu\","
    "MaxHosts = 1,"
    "User = \"sandvik@cs.wisc.edu\","
    "OrigMaxHosts = 1,"
    "JobStatus = 2,"
    "CurrentHosts = 1,"
    "RemoteHost = \"sneha.cs.wisc.edu\","
    "ShadowBday = 999801260,"
    "ServerTime = 999803649"

	/* A machine class ad */
    "MyType = \"Machine\", "
    "TargetType = \"Job\", "
    "Name = \"coral.cs.wisc.edu\", "
    "Machine = \"coral.cs.wisc.edu\", "
    "Rank = 0.000000, "
    "CpuBusy = ((LoadAvg - CondorLoadAvg) >= 0.600000), "
    "IsInstructional = FALSE, "
    "RebootedDaily = FALSE, "
    "CkptServer = \"condor-ckpt.cs.wisc.edu\", "
    "VacateCkptServer = CkptServer, "
    "NearestStorage = (Name == \"NeST@turkey.cs.wisc.edu\"), "
    "CondorVersion = \"$CondorVersion: 6.3.0 Jul 16 2001 $\", "
    "CondorPlatform = \"$CondorPlatform: INTEL-LINUX-GLIBC21 $\", "
    "VirtualMachineID = 1, "
    "VirtualMemory = 787144, "
    "Disk = 418932, "
    "CondorLoadAvg = 0.000000, "
    "LoadAvg = 0.000000, "
    "KeyboardIdle = 139, "
    "ConsoleIdle = 139, "
    "Memory = 511, "
    "Cpus = 1, "
    "StartdIpAddr = \"<128.105.175.116:1026>\", "
    "Arch = \"INTEL\", "
    "OpSys = \"LINUX\", "
    "UidDomain = \"cs.wisc.edu\", "
    "FileSystemDomain = \"cs.wisc.edu\", "
    "Subnet = \"128.105.175\", "
    "java1Interpreter = \"/s/jdk1.0.2/bin/java\", "
    "java1sunInterpreter = \"/s/jdk1.0.2/bin/java\", "
    "java1sun1_0_2Interpreter = \"/s/jdk1.0.2/bin/java\", "
    "java2sun1_2_2Interpreter = \"/s/jdk1.2.2/bin/java\", "
    "javaInterpreter = \"/s/jdk1.3/bin/java\", "
    "java2Interpreter = \"/s/jdk1.3/bin/java\", "
    "java2sunInterpreter = \"/s/jdk1.3/bin/java\", "
    "java2sun1_3Interpreter = \"/s/jdk1.3/bin/java\", "
    "perl5perl5_005Interpreter = \"/s/perl-5.005/bin/perl\", "
    "perlInterpreter = \"/s/perl-5.004/bin/perl\", "
    "perl5Interpreter = \"/s/perl-5.004/bin/perl\", "
    "perl5perlInterpreter = \"/s/perl-5.004/bin/perl\", "
    "perl5perl5_004Interpreter = \"/s/perl-5.004/bin/perl\", "
    "TotalVirtualMemory = 787144, "
    "TotalDisk = 418932, "
    "KFlops = 236811, "
    "Mips = 713, "
    "LastBenchmark = 999791660, "
    "TotalLoadAvg = 0.000000, "
    "TotalCondorLoadAvg = 0.000000, "
    "ClockMin = 674, "
    "ClockDay = 4, "
    "TotalVirtualMachines = 1, "
    "CpuBusyTime = 0, "
    "CpuIsBusy = FALSE, "
    "State = \"Unclaimed\", "
    "EnteredCurrentState = 999204158, "
    "Activity = \"Idle\", "
    "EnteredCurrentActivity = 999791660, "
    "Start = (((Owner == \"wright\" || Owner == \"johnbent\" || Owner == \"tannenba\" || Owner == \"ballard\" || Owner == \"ncoleman\" || Owner == \"raman\" || Owner == \"thain\" || Owner == \"jfrey\" || Owner == \"psilord\" || Owner == \"sanjeevk\" || Owner == \"bnitin\" || Owner == \"epaulson\" || Owner == \"kloveuw\" || Owner == \"deebo\" || Owner == \"davidw\" || Owner == \"jlockett\" || Owner == \"hbwang\" || Owner == \"jbasney\" || Owner == \"pfc\" || Owner == \"koconnor\" || Owner == \"zmiller\") && (NiceUser =!= TRUE)) || (((((LoadAvg - CondorLoadAvg) <= 0.300000) && KeyboardIdle > 15 * 60) && (TARGET.ImageSize <= ((Memory - 15) * 1024))))), "
    "Requirements = START, "
    "CurrentRank = 0.000000, "
    "LastHeardFrom = 999792864 "
#endif     
};

/*----------------------------------------------------------
 *
 *                     Private Datatypes
 *
 *----------------------------------------------------------*/
struct parameters
{
	bool verbose;
};

class TestResults
{
public:
	TestResults()
	{
		number_of_tests        = 0;
		number_of_tests_passed = 0;
		number_of_tests_failed = 0;
		return;
	};

	~TestResults()
	{
		return;
	}

	void AddResult(bool passed)
	{
		number_of_tests++;
		if (passed) {
			number_of_tests_passed++;
		} else {
			number_of_tests_failed++;
		}
		return;
	}

	void PrintResults(void)
	{
		if (number_of_tests_failed == 0) {
			printf("All %d tests passed.\n", number_of_tests);
		} else {
			printf("%d of %d tests failed.\n",
				   number_of_tests_failed, number_of_tests);
		}
		return;
	}

private:
	int number_of_tests;
	int number_of_tests_passed;
	int number_of_tests_failed;
};

/*----------------------------------------------------------
 *
 *                     Private Functions
 *
 *----------------------------------------------------------*/
static void parse_command_line(int argc, char **argv, struct parameters *parameters);
void 
test_integer_value(ClassAd *classad, const char *attribute_name, 
				   int expected_value,int line_number, TestResults *results);
void test_string_value(ClassAd *classad, const char  *attribute_name,
					   const char *expected_value, int line_number,
					   TestResults *results);
void test_mytype(ClassAd *classad, const char *expected_value, int line_number,
			TestResults *results);
void test_targettype(ClassAd *classad, const char *expected_value, int line_number,
			TestResults *results);
void
test_ads_match(
    ClassAd      *classad_1,
	ClassAd      *classad_2,
    int          line_number,
	TestResults  *results);
void
test_ads_dont_match(
    ClassAd      *classad_1,
	ClassAd      *classad_2,
    int          line_number,
	TestResults  *results);
void print_truncated_string(const char *s, int max_characters);

int 
main(
	 int  argc,
	 char **argv)
{
	ClassAd              **classads;
	struct parameters    parameters;
	TestResults          test_results;

	parse_command_line(argc, argv, &parameters);

	if (parameters.verbose) {
		printf("sizeof(ClassAd) = %d\n", sizeof(ClassAd));
		printf("sizeof(AttrListElem) = %d\n", sizeof(AttrListElem));
		printf("sizeof(ExprTree) = %d\n", sizeof(ExprTree));
		printf("We have %d classads.\n", NUMBER_OF_CLASSAD_STRINGS);
	}
	classads = new (ClassAd *)[NUMBER_OF_CLASSAD_STRINGS];

	for (  int classad_index = 0; 
		   classad_index < (int) NUMBER_OF_CLASSAD_STRINGS;
		   classad_index++) {
		classads[classad_index] = new ClassAd(classad_strings[classad_index], ',');
		if (parameters.verbose) {
			printf("ClassAd %d:\n", classad_index);
			classads[classad_index]->fPrint(stdout);
		}
	}

	test_integer_value(classads[0], "A", 1, __LINE__, &test_results);
	test_integer_value(classads[0], "B", 2, __LINE__, &test_results);
	test_integer_value(classads[0], "C", 3, __LINE__, &test_results);
	test_mytype(classads[0], "", __LINE__, &test_results);
	test_targettype(classads[0], "", __LINE__, &test_results);

	test_string_value(classads[1], "D", "alain", __LINE__, &test_results);
	test_mytype(classads[1], "foo", __LINE__, &test_results);
	test_targettype(classads[1], "blah", __LINE__, &test_results);

	test_ads_match(classads[4], classads[5], __LINE__, &test_results);
	test_ads_match(classads[5], classads[4], __LINE__, &test_results);
	test_ads_dont_match(classads[4], classads[6], __LINE__, &test_results);
	test_ads_dont_match(classads[6], classads[4], __LINE__, &test_results);
	test_ads_dont_match(classads[4], classads[7], __LINE__, &test_results);
	test_ads_dont_match(classads[7], classads[4], __LINE__, &test_results);

	//ClassAd *many_ads[LARGE_NUMBER_OF_CLASSADS];
	/*
	for (int i = 0; i < LARGE_NUMBER_OF_CLASSADS; i++) {
		many_ads[i] = new ClassAd(classad_strings[2], ',');
		//print_memory_usage();
	}
	*/

	//	system("ps -v");


	test_results.PrintResults();
#ifdef USE_STRING_SPACE_IN_CLASSADS
	//classad_string_space.dump();
#endif

	// Clean up when we're done.
	for (  int classad_index = 0; 
		   classad_index < (int) NUMBER_OF_CLASSAD_STRINGS;
		   classad_index++) {
		delete classads[classad_index];
	}
	delete [] classads;

	return 0;
}

static void
parse_command_line(
	 int                argc,
	 char               **argv,
	 struct parameters  *parameters)
{
	int   argument_index;
	bool  dump_usage;

	dump_usage = false;

	parameters->verbose = false;

	argument_index = 1;

	while (argument_index < argc) {
		if (!strcmp(argv[argument_index], "-v")
			|| !strcmp(argv[argument_index], "-verbose")) {
			parameters->verbose = !parameters->verbose;
		} else if (!strcmp(argv[argument_index], "-h")
			|| !strcmp(argv[argument_index], "-help")
			|| !strcmp(argv[argument_index], "-?")) {
			dump_usage = true;
		} else {
			fprintf(stderr, "Unknown argument: %s\n", 
					argv[argument_index]);
			dump_usage = true;

		}
		argument_index++;
	}

	if (dump_usage) {
		fprintf(stderr, "Usage: test_classads [-v | -verbose] "
				        "[-h | -help| -?]\n");
		fprintf(stderr, "       [-v | -verbose]   Print oodles of info.\n");
		fprintf(stderr, "       [-h | -help | -?] Print this message.\n");
		exit(1);
	}
	return;
}

void 
test_integer_value(
    ClassAd     *classad,
	const char  *attribute_name,
	int         expected_value,
	int         line_number,
    TestResults *results)
{
	int actual_value;
 	int found_integer;

	classad->LookupInteger(attribute_name, actual_value);
	if (expected_value == actual_value) {
		printf("Passed: %s is %d in line %d\n",
			   attribute_name, expected_value, line_number);
		results->AddResult(true);
	} else if (!found_integer) {
		printf("Failed: Attribute \"%s\" is not found.\n", attribute_name);
		results->AddResult(false);
	} else {
		printf("Failed: %s is %d not %d in line %d\n",
			   attribute_name, actual_value, expected_value, line_number);
		results->AddResult(false);
	}
	return;
}

void 
test_string_value(
    ClassAd     *classad,
	const char  *attribute_name,
	const char  *expected_value,
	int         line_number,
    TestResults *results)
{
	int         found_string;
	static char actual_value[30001];

	found_string = classad->LookupString(attribute_name, actual_value, 30000);
	if (found_string && !strncmp(expected_value, actual_value, 30000)) {
		printf("Passed: %s is \"", attribute_name);
		print_truncated_string(expected_value, 40);
		printf("\" in line %d\n", line_number);
		results->AddResult(true);
	} else if (!found_string) {
		printf("Failed: Attribute \"%s\" is not found.\n", attribute_name);
		results->AddResult(false);
	} else {
		printf("Failed: %s is \"", attribute_name);
		print_truncated_string(actual_value, 30);
		printf("\" not \"");
		print_truncated_string(expected_value, 30);
		printf("\" in line %d\n", line_number);
		results->AddResult(false);
	}
	return;
}

void 
test_mytype(
    ClassAd     *classad,
	const char  *expected_value,
	int         line_number,
    TestResults *results)
{
	static const char *actual_value;

	actual_value = classad->GetMyTypeName();
	if (!strcmp(expected_value, actual_value)) {
		printf("Passed: MyType is \"");
		print_truncated_string(expected_value, 40);
		printf("\" in line %d\n", line_number);
		results->AddResult(true);
	} else {
		printf("Failed: MyType is \"");
		print_truncated_string(actual_value, 30);
		printf("\" not \"");
		print_truncated_string(expected_value, 30);
		printf("\" in line %d\n", line_number);
		results->AddResult(false);
	}
	return;
}

void 
test_targettype(
    ClassAd     *classad,
	const char  *expected_value,
	int         line_number,
    TestResults *results)
{
	static char *actual_value;

	actual_value = classad->GetTargetTypeName();
	if (!strcmp(expected_value, actual_value)) {
		printf("Passed: TargetType is \"");
		print_truncated_string(expected_value, 40);
		printf("\" in line %d\n", line_number);
		results->AddResult(true);
	} else {
		printf("Failed: TargetType is \"");
		print_truncated_string(actual_value, 30);
		printf("\" not \"");
		print_truncated_string(expected_value, 30);
		printf("\" in line %d\n", line_number);
		results->AddResult(false);
	}
	return;
}

void
test_ads_match(
    ClassAd      *classad_1,
	ClassAd      *classad_2,
    int          line_number,
	TestResults  *results)
{
	if (classad_1->IsAMatch(classad_2)) {
		printf("Passed: classads match in line %d.\n", line_number);
		results->AddResult(true);
	} else {
		printf("Failed: classads don't match in line %d.\n", line_number);
		results->AddResult(false);
	}
	return;
}

void
test_ads_dont_match(
    ClassAd      *classad_1,
	ClassAd      *classad_2,
    int          line_number,
	TestResults  *results)
{
	if (!classad_1->IsAMatch(classad_2)) {
		printf("Passed: classads don't match in line %d.\n", line_number);
		results->AddResult(true);
	} else {
		printf("Failed: classads match in line %d.\n", line_number);
		results->AddResult(false);
	}
	return;
}

void 
print_truncated_string(
    const char *s,
	int max_characters)
{
	int length;

	if (max_characters < 1) {
		max_characters = 1;
	}

	length = strlen(s);
	if (length > max_characters) {
		if (max_characters > 3) {
			printf("%.*s...", max_characters - 3, s);
		}
		else {
			printf("%.*s", max_characters, s); 
		}
	}
	else {
		printf("%s", s);
	}
	return;
}

