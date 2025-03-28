#include "condor_common.h"
#include "condor_config.h"
#include "daemon.h"
#include "dc_schedd.h"

/****************************************************
*	Example of c++ tool to send command to DAGMan.
*	(Until a DCDAGMan object is created)
*****************************************************/

int main(int argc, const char** argv) {

	set_priv_initialize(); // allow uid switching if root
	config();

	if (argc != 2) {
		fprintf(stderr, "Command is missing DAGMan job ID\n");
		return 1;
	}

	int ID = 0;
	try {
		ID = std::stoi(argv[1]);
	} catch (...) {
		fprintf(stderr, "Invalid DAGMan job ID provided\n");
		return 1;
	}

	// Contact Schedd for DAGMan contact information
	DCSchedd schedd;
	if ( ! schedd.locate()) {
		fprintf(stderr, "Failed to connect to local Schedd: %s\n", schedd.error());
		return 1;
	}

	CondorError errStack;
	ClassAd* ad = schedd.getDAGManContact(ID, errStack);
	if ( ! ad) {
		fprintf(stderr, "Failed to get DAGMan %d contact information: %s\n",
		        ID, errStack.getFullText(true).c_str());
		return 1;
	}

	std::string err = "";
	std::string addr = "";
	const char* message = getenv("HALT_REASON");

	Daemon* d = nullptr;
	Sock* sock = nullptr;

	// Parse return ad from Schedd DAGMan contact query
	bool success = false;
	if ( ! ad->LookupBool("Success", success) || ! success) {
		err = "Unknown";
		ad->LookupString("FailureReason", err);
		goto DONE;
	} else if ( ! ad->LookupString("Address", addr) || addr.empty()) {
		err = "No address";
		goto DONE;
	} else {
		// Create generic DAGMan DC object with sinful addr
		printf("DAG %d Address: %s\n", ID, addr.c_str());
		d = new Daemon(DT_GENERIC, addr.c_str());
	}

	// Construct payload Ad to send to DAGMan
	// Being cheap and using the returned contact information
	// ClassAd as request upon success to not copy the 'Secret' attribute

	ad->InsertAttr("DagCommand", 1);

	if (message) {
		ad->InsertAttr("HaltReason", message);
	}

	// Start command
	sock = d->startCommand(DAGMAN_GENERIC, Stream::reli_sock, 0);
	if ( ! sock) {
		err = "Failed to open socket to DAGMan";
		goto DONE;
	}

	// Send request ad
	if ( ! putClassAd(sock, *ad) || ! sock->end_of_message()) {
		err = "Failed to send command information over socket";
		goto DONE;
	}

	// Get result ad
	ad->Clear();
	sock->decode();
	if ( ! getClassAd(sock, *ad) || ! sock->end_of_message()) {
		err = "Failed to get a response for DAGMan";
		goto DONE;
	}

	// Check whether or not command was successful
	success = false;
	if ( ! ad->LookupBool("Success", success) || ! success) {
		err = "Unknown";
		ad->LookupString("FailureReason", err);
		goto DONE;
	} else {
		printf("DAG halted\n");
	}

DONE:
	delete d;
	delete sock;
	delete ad;

	if ( ! err.empty()) {
		fprintf(stderr, "ERROR: Failed to send command to DAG %d: %s\n", ID, err.c_str());
		return 1;
	}

	return 0;
}