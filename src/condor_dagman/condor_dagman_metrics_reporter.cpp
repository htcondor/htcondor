#include <iostream>
#include <string>
#include "condor_random_num.h"
#include <cstring>
#include <fstream>
#include <curl/curl.h>

// Class to support RAII technique for curl
class Curl {
public:
	Curl() { curlp = curl_easy_init(); }
	~Curl() { if(curlp) curl_easy_cleanup(curlp); }
	CURL* get() { return curlp; }
private:
	CURL* curlp;
};

// Class to support RAII technique for curl slist
class Curl_slist {
public:
	Curl_slist() { slist = NULL; }
	~Curl_slist() { if(slist) curl_slist_free_all(slist); }
	struct curl_slist* get() { return slist; }
	struct curl_slist* append(const char* s);
private:
	struct curl_slist* slist;
};

struct curl_slist* Curl_slist::append(const char* s)
{
	return slist = curl_slist_append(slist,s);
}

// The default url we send to
const char* cc_metrics_url = "http://metrics.pegasus.isi.edu/metrics";

int main(int argc,char* argv[])
{
		// Need to read some environment variables here
		// Which ones? TEMPTEMP
	std::string metrics_from_dagman,dagman_status,dag_file,metrics_url;

		// Pull parameters off the command line
	for(int ii=1;ii<argc; ++ii) {
		if(!std::strcmp(argv[ii],"-f")) {
			++ii;
			if(argv[ii]) {
				metrics_from_dagman = argv[ii];
			} else {
				std::cerr << "No metrics file specified" << std::endl;
			}
		}
			// What exactly am I supposed to do with the status?
			// TEMPTEMP
		if(!std::strcmp(argv[ii],"-s")) {
			++ii;
			if(argv[ii]) {
				dagman_status = argv[ii];
			} else {
				std::cerr << "No status specified" << std::endl;
			}
		}
		if(!std::strcmp(argv[ii],"-d")) {
			++ii;
			if(argv[ii]) {
				dag_file = argv[ii];
			} else {
				std::cerr << "No dagfile specified" << std::endl;
			}
		}
		if(!std::strcmp(argv[ii],"-u")) { // For testing
			++ii;
			if(argv[ii]) {
				metrics_url = argv[ii];
			} else {
				std::cerr << "No dagfile specified" << std::endl;
			}
		}
	}
	if(dag_file.empty()) {
		std::cerr << "No dagfile specified.  Terminating now" << std::endl;
		return 1;
	}
		// Now check the command line parameters
	std::string metrics_out_name = dag_file;
	metrics_out_name.append(".metrics.out");
	std::ofstream metrics_out(metrics_out_name.c_str());
	if(!metrics_out) {
		std::cerr << "Cannot open " << metrics_out_name << " for writing" << std::endl;
		return 1;
	}
	if(metrics_from_dagman.empty()) {
		metrics_out << "Metrics from dagman is not specified. Terminating" << std::endl;
		return 1;
	}
	if(dagman_status.empty()) {
		metrics_out << "No dagman status specified. Terminating" << std::endl;
		return 1;
	}
	if(metrics_url.empty()) metrics_url = cc_metrics_url;
	else {
		metrics_out << "Sending data to url " << metrics_url << std::endl;
	}
	bool status = false;

		// Loop ten times in attempts to contact the server
		// Design document says try until 100 seconds are up
	for(int numtries = 0;!status && numtries < 10; ++numtries) {
		sleep(1+(get_random_int() % 10));
		Curl handle;
		Curl_slist slist;
		if(!handle.get()) {
			metrics_out << "Failed to initialize curl. Nothing to do" << std::endl;
			continue;
		}
		curl_easy_setopt(handle.get(),CURLOPT_POST,1);
		if(!slist.append("Content-Type: application/json")) {
			metrics_out << "Failed to set header" << std::endl;
			continue;
		}
		std::ifstream metrics(metrics_from_dagman.c_str());
		if(!metrics) {
			metrics_out << "Failed to open " << metrics_from_dagman << " for reading" << std::endl;
			continue;
		}
		std::string data_to_send;
		std::string data_line;

			// Slurp all the data into a single string
		while(getline(metrics,data_line)){
			std::string::iterator p = data_line.begin();
			for(; p != data_line.end(); ++p) {
				if(!std::isspace(*p)) break;
			}
			data_line.erase(data_line.begin(),p);
			std::string::reverse_iterator q = data_line.rbegin();
			for(; q != data_line.rend(); ++q) {
				if(!std::isspace(*q)) break;
			}
			data_line.erase(q.base(),data_line.end());
			data_to_send.append(data_line);
		}
		metrics.close();

			// Now set curl options
		if(curl_easy_setopt(handle.get(),CURLOPT_POST,1)) {
			metrics_out << "Failed  to set POST option" << std::endl;
			continue;
		}
		if(curl_easy_setopt(handle.get(),CURLOPT_POSTFIELDS,data_to_send.data())) {
			metrics_out << "Failed to set data to send in POST" << std::endl;
			continue;
		}
		if(curl_easy_setopt(handle.get(),CURLOPT_POSTFIELDSIZE,data_to_send.size())) {
			metrics_out << "Failed to set data size to send in POST" << std::endl;
			continue;
		}
		if(curl_easy_setopt(handle.get(),CURLOPT_HTTPHEADER,slist.get())) {
			metrics_out << "Failed to set header option to use json" << std::endl;
			continue;
		}
		if(curl_easy_setopt(handle.get(),CURLOPT_URL,metrics_url.c_str())) {
			metrics_out << "Failed to set URL to send to" << std::endl;
			continue;
		}

			// Finally, send the data
		CURLcode res = curl_easy_perform(handle.get());
		if(!res) {
			metrics_out << "Successfully sent data to server!" << std::endl;
			status = true;
		} else {
			metrics_out << "curl_easy_perform failed with code " << res << std::endl;
		}
	}
	return status?0:1;
}
