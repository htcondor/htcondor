#include "server_stat.h"
#include "network2.h"
#include "constants2.h"
#include "gen_lib.h"
#include <iostream.h>
#include <iomanip.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


extern "C" { unsigned int htonl(unsigned int); }
extern "C" { unsigned short int htons(unsigned short int); }
extern "C" { unsigned int ntohl(unsigned int); }
extern "C" { unsigned short int ntohs(unsigned short int); }


ServerStat::ServerStat()
{
	struct sockaddr_in addr_info;
	char*              server_IP;

	ptr = NULL;
	num_files = 0;
	free_capacity = 0.0;
	socket_desc = I_socket();
	if (socket_desc == INSUFFICIENT_RESOURCES) {
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: insufficient resources for a new socket descriptor" 
			<< endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(INSUFFICIENT_RESOURCES);
    } else if (socket_desc == SOCKET_ERROR) {
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: error creating socket" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(SOCKET_ERROR);
    }
	bzero((char*) &addr_info, sizeof(struct sockaddr_in));
	addr_info.sin_family = AF_INET;
	addr_info.sin_port = htons(CKPT_SVR_SERVICE_REQ_PORT);
	server_IP = getserveraddr();
	if (server_IP == NULL) {
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: unable to identify the Condor checkpoint server" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(CANNOT_LOCATE_SERVER);
    }
	memcpy((char*) &server_addr, server_IP, sizeof(struct in_addr));
	memcpy((char*) &addr_info.sin_addr, server_IP, sizeof(struct in_addr));
	if (connect(socket_desc, (struct sockaddr*) &addr_info, 
				sizeof(struct sockaddr_in)) < 0) {
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: cannot connect to checkpoint server" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(-1);
    }
}


ServerStat::~ServerStat()
{
	if (ptr != NULL) {
		delete [] ptr;
		ptr = NULL;
    }
}


void ServerStat::SortInfo()
{
	if (num_files > 0)
		qsort((char*) ptr, num_files, sizeof(file_state_info), compare);
}


void ServerStat::PrintInfo()
{
	const char ch1='=';
	const char ch2='-'; 

	int l_flag=1;
	int x_flag=1;
	int o_flag=1;
	int n_flag=1;
	int u_flag=1;
	int count;
	
	for (count=0; count<num_files; count++) {
		if ((((ptr+count)->state == LOADING) && (l_flag)) ||
			(((ptr+count)->state == XMITTING) && (x_flag)) ||
			(((ptr+count)->state == ON_SERVER) && (o_flag)) ||
			(((ptr+count)->state == NOT_PRESENT) && (n_flag)) ||
			(((ptr+count)->state == UNKNOWN) && (u_flag))) {

			switch ((ptr+count)->state) {
			    case LOADING:
				    cout << "Files being saved to the checkpoint server:" << 
					endl
					<< "*******************************************" << endl
					<< endl;
					l_flag = 0;
					break;
				case XMITTING:
					if (!l_flag)
						cout << endl << endl << endl;
					cout << "Files being retrieved from the checkpoint server:" 
					<< endl
					<< "*************************************************"
					<< endl << endl;
					x_flag = 0;
					break;
				case ON_SERVER:
					if ((!l_flag) || (!x_flag))
						cout << endl << endl << endl;
					cout << "Files residing on the checkpoint server:" << endl
					<< "****************************************" << endl
					<< endl;
					o_flag = 0;
					break;
				case NOT_PRESENT:
					if ((!l_flag) || (!x_flag) || (!o_flag))
						cout << endl << endl << endl;
					cout << "Namespace reserved for incoming files:" << endl
					<< "**************************************" << endl
					<< endl;
					n_flag = 0;
					break;
				case UNKNOWN:
					if ((!l_flag) || (!x_flag) || (!o_flag) || (!n_flag))
						cout << endl << endl << endl;
					cout << "Files with unknwon status:" << endl
					<< "**************************" << endl
					<< endl;
					u_flag = 0;
					break;
				default:
					cerr << endl << "ERROR:" << endl;
					cerr << "ERROR:" << endl;
					cerr << "ERROR: bad file state received" << endl;
					cerr << "ERROR:" << endl;
					cerr << "ERROR:" << endl << endl;
					exit(-1);
				}
			cout << setw(53) << "Submitting" << endl;
			cout << setw(15) << "File Name" << setw(37) << "Machine" << setw(19)
				<< "Owner Name" << setw(20) << "File Size" << setw(5) << " "
					<< "Time Last Modified (local)" << endl;
			MakeLongLine(ch1, ch2);
		}
		cout << setw(6) << " " << setw(STATUS_FILENAME_LENGTH-1) 
			<< setiosflags(ios::left) << (ptr+count)->file_name 
			<< setw(5) << " "
			<< setw(STATUS_MACHINE_NAME_LENGTH-1) << resetiosflags(ios::right)
            << (ptr+count)->machine_IP_name << setw(5) << " "
	        << setw(STATUS_OWNER_NAME_LENGTH-1) << setiosflags(ios::left) 
	        << (ptr+count)->owner_name << resetiosflags(ios::left) 
	        << setw(15) << (ptr+count)->size << setw(6) << " " 
	        << ctime(&(ptr+count)->last_modified_time) << endl;
    }
	MakeLongLine(ch1, ch2);
}


service_reply_pkt ServerStat::InitHandshake()
{
	service_req_pkt    req;
	service_reply_pkt  reply;
	int                bytes_written;
	int                bytes_read=0;
	int                temp_len;
	
	req.service = htons((u_short) STATUS);
	req.ticket = htonl((unsigned int) AUTHENTICATION_TCKT);
	bytes_written = net_write(socket_desc, (char*) &req, sizeof(req));
	if (bytes_written != sizeof(req)) {
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: cannot fully send service request to server" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(-1);
    }
	while (bytes_read != sizeof(reply)) {
		temp_len = read(socket_desc, ((char*) &reply)+bytes_read, 
						sizeof(reply)-bytes_read);
		if (temp_len == -1) {
			cerr << endl << "ERROR:" << endl;
			cerr << "ERROR:" << endl;
			cerr << "ERROR: server's reply not received" << endl;
			cerr << "ERROR:" << endl;
			cerr << "ERROR:" << endl << endl;
			exit(-1);	  
		}
		else if (temp_len == 0)
			if (errno != EWOULDBLOCK)
			{
				cerr << endl << "ERROR:" << endl;
				cerr << "ERROR:" << endl;
				cerr << "ERROR: server's socket has been unexpectedly closed" 
					<< endl;
				cerr << "ERROR:" << endl;
				cerr << "ERROR:" << endl << endl;
				exit(-1);	  	    
			}
		bytes_read += temp_len;
    }
	close(socket_desc);
	return reply;
}




void ServerStat::LoadInfo()
{
	struct sockaddr_in addr_info;
	int                bytes_received=0;
	int                bytes_expected;
	int                temp_len;
	int                buffer_size;
	char*              ref_ptr;
	service_reply_pkt  reply;
	
	reply = InitHandshake();
	free_capacity = atof(reply.capacity_free_ACD);
	num_files = ntohl(reply.num_files);
	reply.req_status = ntohs(reply.req_status);
	if (reply.req_status != OK) {
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: server will not accept STATUS request" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(-1);	  	    
    } else if (num_files < 0) {
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: bad number of files" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(-1);	  	    
    } else if (num_files == 0) {
		cout << endl << "There are no files on the checkpoint server!!!" << endl
		<< endl;
		exit(1);
    }
	
	ptr = new file_state_info[num_files];
	if (ptr == NULL) {
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: unable to allocate dynamic memory for " << num_files
			<< " files" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(-1);	  	    
    }
	ref_ptr = (char*) ptr;
	bytes_expected = num_files * sizeof(file_state_info);
	buffer_size = (DATA_BUFFER_SIZE/sizeof(file_state_info)) * 
		sizeof(file_state_info);
	socket_desc = I_socket();
	if (socket_desc == INSUFFICIENT_RESOURCES) {
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: insufficient resources for a new socket descriptor" 
			<< endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(INSUFFICIENT_RESOURCES);
    } else if (socket_desc == SOCKET_ERROR) {
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: error creating socket" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(SOCKET_ERROR);
    }
	bzero((char*) &addr_info, sizeof(addr_info));
	addr_info.sin_family = AF_INET;
	addr_info.sin_port = reply.port;
	memcpy((char*) &addr_info.sin_addr, (char*) &server_addr, 
		   sizeof(struct in_addr));
	if (setsockopt(socket_desc, SOL_SOCKET, SO_RCVBUF, (char*) &buffer_size,
				   sizeof(buffer_size)) < 0) {
		cerr << endl << "WARNING:" << endl;
		cerr << "WARNING:" << endl;
		cerr << "WARNING: unable to adjust the size of the receiving buffer" 
			<< endl;
		cerr << "WARNING:" << endl;
		cerr << "WARNING:" << endl;
    }
	if (connect(socket_desc, (struct sockaddr*) &addr_info, 
				sizeof(struct sockaddr_in)) < 0) {
		cerr << endl << "ERROR:" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR: cannot connect to server for data transfer" << endl;
		cerr << "ERROR:" << endl;
		cerr << "ERROR:" << endl << endl;
		exit(-1);	  	    
    }
	while (bytes_received < bytes_expected) {
		temp_len = read(socket_desc, ref_ptr+bytes_received, 
						bytes_expected-bytes_received);
		if (temp_len == -1) {
			cerr << endl << "ERROR:" << endl;
			cerr << "ERROR:" << endl;
			cerr << "ERROR: server not responding" << endl;
			cerr << "ERROR:" << endl;
			cerr << "ERROR:" << endl << endl;
			exit(-1);	  	    
		} else if (temp_len == 0)
			if (errno != EWOULDBLOCK) {
				cerr << endl << "ERROR:" << endl;
				cerr << "ERROR:" << endl;
				cerr << "ERROR: server has unexpectedly terminated data "
					<< "transmission" << endl;
				cerr << "ERROR:" << endl;
				cerr << "ERROR:" << endl << endl;
				exit(-1);	  	    
			}
		bytes_received += temp_len;
    }
	bytes_received = htonl(bytes_received);
	net_write(socket_desc, (char*) &bytes_received, sizeof(bytes_received));
	// Do not care if server received this
	close(socket_desc);
}




void ServerStat::ConvertInfo()
{
	int count;
	
	for (count=0; count<num_files; count++)
    {
		(ptr+count)->size = ntohl((ptr+count)->size);
		(ptr+count)->last_modified_time = 
			(time_t) ntohl((ptr+count)->last_modified_time);
		(ptr+count)->state = ntohs((ptr+count)->state);
    }
}




int compare(const void* A_ptr,
			const void* B_ptr)
{
	int temp;
	file_state_info* A;
	file_state_info* B;
	
	A = (file_state_info*) A_ptr;
	B = (file_state_info*) B_ptr;
	if ((temp=A->state - B->state) != 0)
		return temp;
	if ((temp=strncmp(A->owner_name, B->owner_name, STATUS_OWNER_NAME_LENGTH))
		!= 0)
		return temp;
	if ((temp=strncmp(A->machine_IP_name, B->machine_IP_name,
					  STATUS_MACHINE_NAME_LENGTH)) != 0)
		return temp;
	temp = strncmp(A->file_name, B->file_name, STATUS_FILENAME_LENGTH);
	return temp;
}


int main(void)
{
	ServerStat SS;
	
	SS.LoadInfo();
	SS.ConvertInfo();
	SS.SortInfo();
	SS.PrintInfo();
	return 0;
}
