/***************************************************************
 *
 * Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/


#include "condor_common.h"
#include "transfer_queue.h"
#include "condor_debug.h"
#include "condor_daemon_core.h"
#include "condor_commands.h"
#include "condor_config.h"
#include "dc_transfer_queue.h"
#include "condor_email.h"
#include "algorithm"

TransferQueueRequest::TransferQueueRequest(ReliSock *sock,filesize_t sandbox_size,char const *fname,char const *jobid,char const *queue_user,bool downloading,time_t max_queue_age):
	m_sock(sock),
	m_queue_user(queue_user),
	m_jobid(jobid),
	m_sandbox_size_MB(sandbox_size/1024.0/1024.0),
	m_fname(fname),
	m_downloading(downloading),
	m_gave_go_ahead(false), m_max_queue_age(max_queue_age), m_time_born(time(NULL)), m_time_go_ahead(0)
{
		// the up_down_queue_user name uniquely identifies the user and the direction of transfer
	if( m_downloading ) {
		m_up_down_queue_user = "D";
	}
	else {
		m_up_down_queue_user = "U";
	}
	m_up_down_queue_user += m_queue_user;
}

TransferQueueRequest::~TransferQueueRequest() {
	if( m_sock ) {
		daemonCore->Cancel_Socket( m_sock );
		delete m_sock;
		m_sock = nullptr;
	}
}

char const *
TransferQueueRequest::Description() {
	formatstr(m_description, "%s %s job %s for %s (sandbox size %g MB, initial file %s)",
					m_sock ? m_sock->peer_description() : "",
					m_downloading ? "downloading" : "uploading",
					m_jobid.c_str(),
					m_queue_user.c_str(),
					m_sandbox_size_MB,
					m_fname.c_str());
	return m_description.c_str();
}

char const *
TransferQueueRequest::SinlessDescription() {
	char const *s = Description();
		// get rid of the sinful string, which is at the beginning of the description
	char const *pos = strchr(s,'>');
	if( pos ) {
		if( pos[1] == ' ' ) {
			s = pos+2;
		}
		else {
			s = pos+1;
		}
	}
	return s;
}

bool
TransferQueueRequest::SendGoAhead(XFER_QUEUE_ENUM go_ahead,char const *reason) {
	ASSERT( m_sock );

	m_sock->encode();
	ClassAd msg;
	msg.Assign(ATTR_RESULT,(int)go_ahead);
	if( reason ) {
		msg.Assign(ATTR_ERROR_STRING,reason);
	}

		// how often should transfer processes send a report of I/O activity
		//   0 means never
	int report_interval = param_integer("TRANSFER_IO_REPORT_INTERVAL",10,0);
	msg.Assign(ATTR_REPORT_INTERVAL,report_interval);

	if(!putClassAd( m_sock, msg ) || !m_sock->end_of_message()) {
		dprintf(D_ALWAYS,
				"TransferQueueRequest: failed to send GoAhead to %s\n",
				Description() );
		return false;
	}

	m_gave_go_ahead = true;
	m_time_go_ahead = time(nullptr);
	return true;
}

TransferQueueManager::TransferQueueManager() : m_round_robin_garbage_time(time(nullptr)) {
	m_stat_pool.AddProbe(ATTR_TRANSFER_QUEUE_MAX_UPLOADING,&m_max_uploading_stat,nullptr,IF_BASICPUB|m_max_uploading_stat.PubValue);
	m_stat_pool.AddProbe(ATTR_TRANSFER_QUEUE_MAX_DOWNLOADING,&m_max_downloading_stat,nullptr,IF_BASICPUB|m_max_downloading_stat.PubValue);
	m_stat_pool.AddProbe(ATTR_TRANSFER_QUEUE_NUM_UPLOADING,&m_uploading_stat,nullptr,IF_BASICPUB|m_uploading_stat.PubDefault);
	m_stat_pool.AddProbe(ATTR_TRANSFER_QUEUE_NUM_DOWNLOADING,&m_downloading_stat,nullptr,IF_BASICPUB|m_downloading_stat.PubDefault);
	m_stat_pool.AddProbe(ATTR_TRANSFER_QUEUE_NUM_WAITING_TO_UPLOAD,&m_waiting_to_upload_stat,nullptr,IF_BASICPUB|m_waiting_to_upload_stat.PubDefault);
	m_stat_pool.AddProbe(ATTR_TRANSFER_QUEUE_NUM_WAITING_TO_DOWNLOAD,&m_waiting_to_download_stat,nullptr,IF_BASICPUB|m_waiting_to_download_stat.PubDefault);
	m_stat_pool.AddProbe(ATTR_TRANSFER_QUEUE_UPLOAD_WAIT_TIME,&m_upload_wait_time_stat,nullptr,IF_BASICPUB|m_upload_wait_time_stat.PubDefault);
	m_stat_pool.AddProbe(ATTR_TRANSFER_QUEUE_DOWNLOAD_WAIT_TIME,&m_download_wait_time_stat,nullptr,IF_BASICPUB|m_download_wait_time_stat.PubDefault);
	RegisterStats(nullptr,m_iostats);
}

TransferQueueManager::~TransferQueueManager() {
	for (TransferQueueRequest *client : m_xfer_queue) {
		delete client;
	}
	m_xfer_queue.clear();

	if( daemonCore && m_check_queue_timer != -1 ) {
		daemonCore->Cancel_Timer( m_check_queue_timer );
	}
	if( daemonCore && m_update_iostats_timer != -1 ) {
		daemonCore->Cancel_Timer( m_update_iostats_timer );
	}
}

void
TransferQueueManager::parseThrottleConfig(char const *config_param,bool &enable_throttle,double &low,double &high,std::string &throttle_short_horizon,std::string &throttle_long_horizon,time_t &throttle_increment_wait)
{
	enable_throttle = false;

	std::string throttle_config;
	if( !param(throttle_config,config_param) ) {
		return;
	}

	char *endptr=nullptr;
	low = strtod(throttle_config.c_str(),&endptr);
	if( !endptr || !(isspace(*endptr) || *endptr == '\0') ) {
		EXCEPT("Invalid configuration for %s: %s",config_param,throttle_config.c_str());
		return;
	}

	while( isspace(*endptr) ) endptr++;

	if( *endptr == '\0' ) {
		high = low;
		low = 0.9*high;
	}
	else if( strncmp(endptr,"to",2)==0 && isspace(endptr[2]) ) {
		endptr += 3;
		while( isspace(*endptr) ) endptr++;

		high = strtod(endptr,&endptr);
		if( !endptr || *endptr != '\0' ) {
			EXCEPT("Invalid configuration for %s: %s",config_param,throttle_config.c_str());
			return;
		}
	}
	else {
		EXCEPT("Invalid configuration for %s: %s",config_param,throttle_config.c_str());
	}

	if( high < low ) {
		EXCEPT("Invalid configuration for %s (first value must be less than second): %s",config_param,throttle_config.c_str());
	}
	if( high < 0 || low < 0 ) {
		EXCEPT("Invalid configuration for %s (values must be positive): %s",config_param,throttle_config.c_str());
	}

		// for now, these are hard-coded
	std::string horizon_param;
	formatstr(horizon_param,"%s_SHORT_HORIZON",config_param);
	param(throttle_short_horizon,horizon_param.c_str(),"1m");
	formatstr(horizon_param,"%s_LONG_HORIZON",config_param);
	param(throttle_long_horizon,horizon_param.c_str(),"5m");

	std::string wait_param;
	formatstr(wait_param,"%s_WAIT_BETWEEN_INCREMENTS",config_param);
	throttle_increment_wait = (time_t) param_integer(wait_param.c_str(),60,0);

	m_throttle_disk_load = true;
}

void
TransferQueueManager::InitAndReconfig() {
	m_max_downloads = param_integer("MAX_CONCURRENT_DOWNLOADS",100,0);
	m_max_uploads = param_integer("MAX_CONCURRENT_UPLOADS",100,0);
	m_default_max_queue_age = param_integer("MAX_TRANSFER_QUEUE_AGE",3600*2,0);

	parseThrottleConfig("FILE_TRANSFER_DISK_LOAD_THROTTLE",m_throttle_disk_load,m_disk_load_low_throttle,m_disk_load_high_throttle,m_disk_throttle_short_horizon,m_disk_throttle_long_horizon,m_throttle_disk_load_increment_wait);

	if( m_throttle_disk_load ) {
		m_stat_pool.AddProbe(ATTR_FILE_TRANSFER_DISK_THROTTLE_LOW,&m_disk_throttle_low_stat,nullptr,IF_BASICPUB|m_disk_throttle_low_stat.PubValue);
		m_stat_pool.AddProbe(ATTR_FILE_TRANSFER_DISK_THROTTLE_HIGH,&m_disk_throttle_high_stat,nullptr,IF_BASICPUB|m_disk_throttle_high_stat.PubValue);
		m_stat_pool.AddProbe(ATTR_FILE_TRANSFER_DISK_THROTTLE_LIMIT,&m_disk_throttle_limit_stat,nullptr,IF_BASICPUB|m_disk_throttle_limit_stat.PubValue);
		m_stat_pool.AddProbe(ATTR_FILE_TRANSFER_DISK_THROTTLE_EXCESS,&m_disk_throttle_excess,nullptr,IF_BASICPUB|m_disk_throttle_excess.PubDefault);
		m_stat_pool.AddProbe(ATTR_FILE_TRANSFER_DISK_THROTTLE_SHORTFALL,&m_disk_throttle_shortfall,nullptr,IF_BASICPUB|m_disk_throttle_shortfall.PubDefault);
	}
	else {
		m_stat_pool.RemoveProbe(ATTR_FILE_TRANSFER_DISK_THROTTLE_LOW);
		m_stat_pool.RemoveProbe(ATTR_FILE_TRANSFER_DISK_THROTTLE_HIGH);
		m_stat_pool.RemoveProbe(ATTR_FILE_TRANSFER_DISK_THROTTLE_LIMIT);
		m_stat_pool.RemoveProbe(ATTR_FILE_TRANSFER_DISK_THROTTLE_EXCESS);
		m_stat_pool.RemoveProbe(ATTR_FILE_TRANSFER_DISK_THROTTLE_SHORTFALL);
	}

	m_update_iostats_interval = param_integer("TRANSFER_IO_REPORT_INTERVAL",10,0);
	if( m_update_iostats_interval != 0 ) {
		if( m_update_iostats_timer != -1 ) {
			ASSERT( daemonCore->Reset_Timer_Period(m_update_iostats_timer,m_update_iostats_interval) == 0 );
		}
		else {
			m_update_iostats_timer = daemonCore->Register_Timer(
				m_update_iostats_interval,
				m_update_iostats_interval,
				(TimerHandlercpp)&TransferQueueManager::UpdateIOStats,
				"UpdateIOStats",this);
			ASSERT( m_update_iostats_timer != -1 );
		}
	}

	m_publish_flags = IF_BASICPUB;
	std::string publish_config;
	if( param(publish_config,"STATISTICS_TO_PUBLISH") ) {
		m_publish_flags = generic_stats_ParseConfigString(publish_config.c_str(), "TRANSFER", "TRANSFER", m_publish_flags);
	}

	std::string iostat_timespans;
	param(iostat_timespans,"TRANSFER_IO_REPORT_TIMESPANS");

	std::string iostat_timespans_err;
	if( !ParseEMAHorizonConfiguration(iostat_timespans.c_str(),ema_config,iostat_timespans_err) ) {
		EXCEPT("Error in TRANSFER_IO_REPORT_TIMESPANS=%s: %s",iostat_timespans.c_str(),iostat_timespans_err.c_str());
	}

	m_iostats.ConfigureEMAHorizons(ema_config);

	if( m_throttle_disk_load ) {
		if( !m_iostats.file_read.HasEMAHorizonNamed(m_disk_throttle_short_horizon.c_str()) ) {
			std::string shortest_horizon = m_iostats.file_read.ShortestHorizonEMAName();
			dprintf(D_ALWAYS,"WARNING: FILE_TRANSFER_DISK_LOAD_THROTTLE_SHORT_HORIZON=%s does not match a timespan listed in TRANSFER_IO_REPORT_TIMESPANS=%s; using %s instead\n",
					m_disk_throttle_short_horizon.c_str(),
					iostat_timespans.c_str(),
					shortest_horizon.c_str());
			m_disk_throttle_short_horizon = shortest_horizon;
		}
		if( !m_iostats.file_read.HasEMAHorizonNamed(m_disk_throttle_long_horizon.c_str()) ) {
			std::string shortest_horizon = m_iostats.file_read.ShortestHorizonEMAName();
			dprintf(D_ALWAYS,"WARNING: FILE_TRANSFER_DISK_LOAD_THROTTLE_LONG_HORIZON=%s does not match a timespan listed in TRANSFER_IO_REPORT_TIMESPANS=%s; using %s instead\n",
					m_disk_throttle_long_horizon.c_str(),
					iostat_timespans.c_str(),
					shortest_horizon.c_str());
			m_disk_throttle_long_horizon = shortest_horizon;
		}
	}

	for(auto & m_queue_user : m_queue_users)
	{
		m_queue_user.second.iostats.ConfigureEMAHorizons(ema_config);
	}

	m_disk_throttle_excess.ConfigureEMAHorizons(ema_config);
	m_disk_throttle_shortfall.ConfigureEMAHorizons(ema_config);

	// do this after configuring EMA horizons so that attribute names are known.
	std::string strWhitelist;
	if (param(strWhitelist, "STATISTICS_TO_PUBLISH_LIST")) {
		m_stat_pool.SetVerbosities(strWhitelist.c_str(), m_publish_flags, true);
	}
}

void
TransferQueueManager::RegisterHandlers() {
	int rc = daemonCore->Register_Command(
		TRANSFER_QUEUE_REQUEST,
		"TRANSFER_QUEUE_REQUEST",
		(CommandHandlercpp)&TransferQueueManager::HandleRequest,
		"TransferQueueManager::HandleRequest",
		this,
		WRITE );
	ASSERT( rc >= 0 );
}

int TransferQueueManager::HandleRequest(int cmd,Stream *stream)
{
	auto *sock = dynamic_cast<ReliSock *>(stream);
	ASSERT( cmd == TRANSFER_QUEUE_REQUEST );

	ClassAd msg;
	sock->decode();
	if( !getClassAd( sock, msg ) || !sock->end_of_message() ) {
		dprintf(D_ALWAYS,
				"TransferQueueManager: failed to receive transfer request "
				"from %s.\n", sock->peer_description() );
		return FALSE;
	}

	bool downloading = false;
	std::string fname;
	std::string jobid;
	std::string queue_user;
	filesize_t sandbox_size = 0;
	if( !msg.LookupBool(ATTR_DOWNLOADING,downloading) ||
		!msg.LookupString(ATTR_FILE_NAME,fname) ||
		!msg.LookupString(ATTR_JOB_ID,jobid) ||
		!msg.LookupString(ATTR_USER,queue_user) ||
		!msg.LookupInteger(ATTR_SANDBOX_SIZE,sandbox_size))
	{
		std::string msg_str;
		sPrintAd(msg_str, msg);
		dprintf(D_ALWAYS,"TransferQueueManager: invalid request from %s: %s\n",
				sock->peer_description(), msg_str.c_str());
		return FALSE;
	}

		// Currently, we just create the client with the default max queue
		// age.  If it becomes necessary to customize the maximum age
		// on a case-by-case basis, it should be easy to adjust.

	auto *client =
		new TransferQueueRequest(
			sock,
			sandbox_size,
			fname.c_str(),
			jobid.c_str(),
			queue_user.c_str(),
			downloading,
			m_default_max_queue_age);

	if( !AddRequest( client ) ) {
		delete client;
		return KEEP_STREAM; // we have already closed this socket
	}

	return KEEP_STREAM;
}

bool
TransferQueueManager::AddRequest( TransferQueueRequest *client ) {
	ASSERT( client );

	std::string error_desc;
	if( daemonCore->TooManyRegisteredSockets(client->m_sock->get_file_desc(),&error_desc))
	{
		dprintf(D_FULLDEBUG,"TransferQueueManager: rejecting %s to avoid overload: %s\n",
				client->Description(),
				error_desc.c_str());
		client->SendGoAhead(XFER_QUEUE_NO_GO,error_desc.c_str());
		return false;
	}

	int flag = D_FULLDEBUG;
	if( IsDebugLevel(D_TEST) ) {
		flag = D_TEST;
	}
	dprintf(flag, "TransferQueueManager: enqueueing %s.\n",
			client->Description());

	int rc = daemonCore->Register_Socket(client->m_sock,
		"<file transfer request>",
		(SocketHandlercpp)&TransferQueueManager::HandleReport,
		"TransferQueueManager::HandleReport TRANSFER_QUEUE_REQUEST", this);

	if( rc < 0 ) {
		dprintf(D_ALWAYS,
				"TransferQueueManager: failed to register socket for %s.\n",
				client->Description());
		return false;
	}

	ASSERT( daemonCore->Register_DataPtr( client ) );

	m_xfer_queue.push_back(client);

	TransferQueueChanged();

	return true;
}

int
TransferQueueManager::HandleReport( Stream *sock )
{
	auto it = m_xfer_queue.begin();
	while (it != m_xfer_queue.end()) {
		TransferQueueRequest *client  = *it;
		
		if( client->m_sock == sock ) {
			if( !client->ReadReport(this) ) {
				dprintf(D_FULLDEBUG,
						"TransferQueueManager: dequeueing %s.\n",
						client->Description());

				delete client;

				// This invalidates it, but we are on our way out 
				// anyway by now.
				it = m_xfer_queue.erase(it);

				TransferQueueChanged();
			}
			return KEEP_STREAM;
		}
		it++;
	}

		// should never get here
	std::string clients;
	for (TransferQueueRequest *client : m_xfer_queue) {
		formatstr_cat(clients, " (%p) %s\n",
					 client->m_sock,client->m_sock->peer_description());
	}
	EXCEPT("TransferQueueManager: ERROR: disconnect from client (%p) %s;"
		   " not found in list: %s\n",
		   sock,
		   sock->peer_description(),
		   clients.c_str());
	return FALSE; // close socket
}

bool
TransferQueueRequest::ReadReport(TransferQueueManager *manager) const
{
	std::string report;
	m_sock->decode();
	if( !m_sock->get(report) ||
		!m_sock->end_of_message() )
	{
		return false;
	}

	if( report.empty() ) {
		return false;
	}

	unsigned report_time = 0;
	unsigned report_interval_usec = 0;
	unsigned recent_bytes_sent = 0;
	unsigned recent_bytes_received = 0;
	unsigned recent_usec_file_read = 0;
	unsigned recent_usec_file_write = 0;
	unsigned recent_usec_net_read = 0;
	unsigned recent_usec_net_write = 0;
	IOStats iostats;

	int rc = sscanf(report.c_str(),"%u %u %u %u %u %u %u %u",
					&report_time,
					&report_interval_usec,
					&recent_bytes_sent,
					&recent_bytes_received,
					&recent_usec_file_read,
					&recent_usec_file_write,
					&recent_usec_net_read,
					&recent_usec_net_write);
	if( rc != 8 ) {
		dprintf(D_ALWAYS,"Failed to parse I/O report from file transfer worker %s: %s.\n",
				m_sock->peer_description(),report.c_str());
		return false;
	}

	iostats.bytes_sent = (double)recent_bytes_sent;
	iostats.bytes_received = (double)recent_bytes_received;
	iostats.file_read = (double)recent_usec_file_read   / 1'000'000;
	iostats.file_write = (double)recent_usec_file_write / 1'000'000;
	iostats.net_read = (double)recent_usec_net_read     / 1'000'000;
	iostats.net_write = (double)recent_usec_net_write   / 1'000'000;

	manager->AddRecentIOStats(iostats,m_up_down_queue_user);
	return true;
}

void
TransferQueueManager::TransferQueueChanged() {
	if( m_check_queue_timer != -1 ) {
			// already have a timer set
		return;
	}
	m_check_queue_timer = daemonCore->Register_Timer(
		0,(TimerHandlercpp)&TransferQueueManager::CheckTransferQueue,
		"CheckTransferQueue",this);
}

void
TransferQueueManager::SetRoundRobinRecency(const std::string &user)
{
	unsigned int old_counter = m_round_robin_counter;

	GetUserRec(user).recency = ++m_round_robin_counter;

		// clear history if the counter wraps, so we do not keep favoring those who have wrapped
	if( m_round_robin_counter < old_counter ) {
		ClearRoundRobinRecency();
	}
}

void
TransferQueueManager::ClearRoundRobinRecency()
{
	for(auto & m_queue_user : m_queue_users)
	{
		m_queue_user.second.recency = 0;
	}
}

bool
TransferQueueManager::TransferQueueUser::Stale(unsigned int stale_recency)
{
		// If this user has recently done anything, this record is not stale
	if( !(recency < stale_recency && running==0 && idle==0) ) {
		return false; // not stale
	}
		// Check for non-negligible iostat moving averages, so we
		// don't lose interesting data about big I/O users.
	if( iostats.bytes_sent.BiggestEMAValue()     > 100000 ||
		iostats.bytes_received.BiggestEMAValue() > 100000 ||
		iostats.file_read.BiggestEMAValue()      > 0.1 ||
		iostats.file_write.BiggestEMAValue()     > 0.1 ||
		iostats.net_read.BiggestEMAValue()       > 0.1 ||
		iostats.net_write.BiggestEMAValue()      > 0.1 )
	{
		return false; // not stale
	}
	return true;
}

void
TransferQueueManager::CollectUserRecGarbage(ClassAd *unpublish_ad)
{
		// To prevent unbounded growth, remove user records that have
		// not been touched in the past hour.

	time_t now = time(nullptr);
		// use abs() here so big clock jumps do not cause long
		// periods of no garbage collection
	if( abs((int)(now - m_round_robin_garbage_time)) > 3600 ) {
		int num_removed = 0;

		for( auto itr = m_queue_users.begin();
			 itr != m_queue_users.end(); )
		{
			TransferQueueUser &u = itr->second;
			if( u.Stale(m_round_robin_garbage_counter) ) {
				UnregisterStats(itr->first.c_str(),u.iostats,unpublish_ad);
					// increment the iterator before calling erase, while it is still valid
				m_queue_users.erase(itr++);
				++num_removed;
			}
			else {
				++itr;
			}
		}

		if( num_removed ) {
			dprintf(D_ALWAYS,"TransferQueueManager::CollectUserRecGarbage: removed %d entries.\n",num_removed);
		}

		m_round_robin_garbage_time = now;
		m_round_robin_garbage_counter = m_round_robin_counter;
	}
}

TransferQueueManager::TransferQueueUser &
TransferQueueManager::GetUserRec(const std::string &user)
{
	QueueUserMap::iterator itr;

	itr = m_queue_users.find(user);
	if( itr == m_queue_users.end() ) {
		itr = m_queue_users.insert(QueueUserMap::value_type(user,TransferQueueUser())).first;
		itr->second.iostats.ConfigureEMAHorizons(ema_config);
		itr->second.iostats.Clear();
		RegisterStats(user.c_str(),itr->second.iostats);
	}
	return itr->second;
}

void
TransferQueueManager::RegisterStats(char const *user,IOStats &iostats,bool unregister,ClassAd *unpublish_ad)
{
	std::string attr;
	bool downloading = true;
	bool uploading = true;
	std::string user_attr;
	int flags = IF_BASICPUB;
	if( user && user[0] ) {
		flags = IF_VERBOSEPUB;
			// The first character of the up_down_user tells us if it is uploading or downloading
		if( user[0] == 'U' ) downloading = false;
		if( user[0] == 'D' ) uploading = false;
		char const *at = strchr(user,'@');
		if( at ) {
			user_attr.append(user+1,at-(user+1));
		}
		else {
			user_attr = user+1;
		}
		user_attr += "_";
	}

	const int ema_flags = flags | stats_entry_sum_ema_rate<double>::PubDefault;

	if( downloading ) {
		formatstr(attr,"%sFileTransferDownloadBytes",user_attr.c_str());
		if( unregister ) {
			m_stat_pool.RemoveProbe(attr.c_str());
			iostats.bytes_received.Unpublish(*unpublish_ad,attr.c_str());
		}
		else {
			m_stat_pool.AddProbe(attr.c_str(),&iostats.bytes_received,nullptr,ema_flags);
		}
		formatstr(attr,"%sFileTransferFileWriteSeconds",user_attr.c_str());
		if( unregister ) {
			m_stat_pool.RemoveProbe(attr.c_str());
			iostats.file_write.Unpublish(*unpublish_ad,attr.c_str());
		}
		else {
			m_stat_pool.AddProbe(attr.c_str(),&iostats.file_write,nullptr,ema_flags);
		}
		formatstr(attr,"%sFileTransferNetReadSeconds",user_attr.c_str());
		if( unregister ) {
			m_stat_pool.RemoveProbe(attr.c_str());
			iostats.net_read.Unpublish(*unpublish_ad,attr.c_str());
		}
		else {
			m_stat_pool.AddProbe(attr.c_str(),&iostats.net_read,nullptr,ema_flags);
		}
		formatstr(attr,"%s%s",user_attr.c_str(),"FileTransferMBWaitingToDownload");
		if( unregister ) {
			m_stat_pool.RemoveProbe(attr.c_str());
			iostats.download_MB_waiting.Unpublish(*unpublish_ad,attr.c_str());
		}
		else {
			m_stat_pool.AddProbe(attr.c_str(),&iostats.download_MB_waiting,nullptr,flags|iostats.download_MB_waiting.PubValue);
		}
	}
	if( uploading ) {
		formatstr(attr,"%sFileTransferUploadBytes",user_attr.c_str());
		if( unregister ) {
			m_stat_pool.RemoveProbe(attr.c_str());
			iostats.bytes_sent.Unpublish(*unpublish_ad,attr.c_str());
		}
		else {
			m_stat_pool.AddProbe(attr.c_str(),&iostats.bytes_sent,nullptr,ema_flags);
		}
		formatstr(attr,"%sFileTransferFileReadSeconds",user_attr.c_str());
		if( unregister ) {
			m_stat_pool.RemoveProbe(attr.c_str());
			iostats.file_read.Unpublish(*unpublish_ad,attr.c_str());
		}
		else {
			m_stat_pool.AddProbe(attr.c_str(),&iostats.file_read,nullptr,ema_flags);
		}
		formatstr(attr,"%sFileTransferNetWriteSeconds",user_attr.c_str());
		if( unregister ) {
			m_stat_pool.RemoveProbe(attr.c_str());
			iostats.net_write.Unpublish(*unpublish_ad,attr.c_str());
		}
		else {
			m_stat_pool.AddProbe(attr.c_str(),&iostats.net_write,nullptr,ema_flags);
		}
		formatstr(attr,"%s%s",user_attr.c_str(),"FileTransferMBWaitingToUpload");
		if( unregister ) {
			m_stat_pool.RemoveProbe(attr.c_str());
			iostats.upload_MB_waiting.Unpublish(*unpublish_ad,attr.c_str());
		}
		else {
			m_stat_pool.AddProbe(attr.c_str(),&iostats.upload_MB_waiting,nullptr,flags|iostats.upload_MB_waiting.PubValue);
		}
	}
}

void
TransferQueueManager::ClearTransferCounts()
{
	m_waiting_to_upload = 0;
	m_waiting_to_download = 0;
	m_upload_wait_time = 0;
	m_download_wait_time = 0;
	m_iostats.upload_MB_waiting = 0;
	m_iostats.download_MB_waiting = 0;

	for(auto & m_queue_user : m_queue_users)
	{
		m_queue_user.second.running = 0;
		m_queue_user.second.idle = 0;
		m_queue_user.second.iostats.upload_MB_waiting = 0;
		m_queue_user.second.iostats.download_MB_waiting = 0;
	}
}

void
TransferQueueManager::IOStatsChanged() {
	if( m_throttle_disk_load &&
		(m_waiting_to_upload > 0 || m_waiting_to_download > 0) &&
		m_throttle_disk_load &&
		m_throttle_disk_load_max_concurrency == m_uploading + m_downloading )
	{
		TransferQueueChanged();
	}
}

void
TransferQueueManager::CheckTransferQueue( int /* timerID */ ) {
	int downloading = 0;
	int uploading = 0;
	bool clients_waiting = false;

	m_check_queue_timer = -1;

	ClearTransferCounts();

	for (TransferQueueRequest *client : m_xfer_queue) {
		if( client->m_gave_go_ahead ) {
			GetUserRec(client->m_up_down_queue_user).running++;
			if( client->m_downloading ) {
				downloading += 1;
			}
			else {
				uploading += 1;
			}
		}
		else {
			GetUserRec(client->m_up_down_queue_user).idle++;
		}
	}

	if( m_throttle_disk_load ) {
		int old_concurrency_limit = m_throttle_disk_load_max_concurrency;

		double disk_load_short = m_iostats.file_read.EMAValue(m_disk_throttle_short_horizon.c_str()) +
		                         m_iostats.file_write.EMAValue(m_disk_throttle_short_horizon.c_str());
		double disk_load_long =  m_iostats.file_read.EMAValue(m_disk_throttle_long_horizon.c_str()) +
		                         m_iostats.file_write.EMAValue(m_disk_throttle_long_horizon.c_str());

		if( disk_load_short > m_disk_load_high_throttle ) {
				// above the high water mark, do not start more transfers
			m_throttle_disk_load_max_concurrency = uploading + downloading;
		}
		else if( disk_load_long > m_disk_load_low_throttle || disk_load_short > m_disk_load_low_throttle ) {
				// between the high and low water mark, keep the concurrency limit as is (but at least 1)
			if( m_throttle_disk_load_max_concurrency < 1 ) {
				m_throttle_disk_load_max_concurrency = 1;
				m_throttle_disk_load_incremented = time(nullptr);
			}
		}
		else {
				// below the low water mark, slowly increase the concurrency limit if we are running into it
			if( uploading + downloading == m_throttle_disk_load_max_concurrency ) {
				time_t now = time(nullptr);
				if( m_throttle_disk_load_incremented > now ) {
					m_throttle_disk_load_incremented = now; // clock jumped back
				}
				if( m_throttle_disk_load_incremented == 0 || now-m_throttle_disk_load_incremented >= m_throttle_disk_load_increment_wait ) {
					m_throttle_disk_load_incremented = now;
					m_throttle_disk_load_max_concurrency += 1;
					if( m_throttle_disk_load_max_concurrency < floor(m_disk_load_low_throttle) ) {
						m_throttle_disk_load_max_concurrency = floor(m_disk_load_low_throttle);
					}
				}
			}
		}

		if( old_concurrency_limit != m_throttle_disk_load_max_concurrency ) {
			dprintf(D_ALWAYS,
					"TransferQueueManager: adjusted concurrency limit by %+d based on disk load: "
					"new limit %d, load %s %f %s %f, throttle %f to %f\n",
					m_throttle_disk_load_max_concurrency-old_concurrency_limit,
					m_throttle_disk_load_max_concurrency,
					m_disk_throttle_short_horizon.c_str(),
					disk_load_short,
					m_disk_throttle_long_horizon.c_str(),
					disk_load_long,
					m_disk_load_low_throttle,
					m_disk_load_high_throttle);
		}
	}

		// schedule new transfers
	while( uploading < m_max_uploads || m_max_uploads <= 0 ||
		   downloading < m_max_downloads || m_max_downloads <= 0 )
	{
		TransferQueueRequest *best_client = nullptr;
		int best_recency = 0;
		unsigned int best_running_count = 0;

		if( m_throttle_disk_load && (uploading + downloading >= m_throttle_disk_load_max_concurrency) ) {
			break;
		}

		for (TransferQueueRequest *client : m_xfer_queue) {
			if( client->m_gave_go_ahead ) {
				continue;
			}
			if( (client->m_downloading && 
				(downloading < m_max_downloads || m_max_downloads <= 0)) ||
				((!client->m_downloading) &&
				(uploading < m_max_uploads || m_max_uploads <= 0)) )
			{
				TransferQueueUser &this_user = GetUserRec(client->m_up_down_queue_user);
				unsigned int this_user_active_count = this_user.running;
				int this_user_recency = this_user.recency;

				bool this_client_is_better = false;
				if( !best_client ) {
					this_client_is_better = true;
				}
				else if( best_client->m_downloading != client->m_downloading ) {
						// effectively treat up/down queues independently
					if( client->m_downloading ) {
						this_client_is_better = true;
					}
				}
				else if( best_running_count > this_user_active_count ) {
						// prefer users with fewer active transfers
						// (only counting transfers in one direction for this comparison)
					this_client_is_better = true;
				}
				else if( best_recency > this_user_recency ) {
						// if still tied: round robin
					this_client_is_better = true;
				}

				if( this_client_is_better ) {
					best_client = client;
					best_running_count = this_user_active_count;
					best_recency = this_user_recency;
				}
			}
		}

		TransferQueueRequest *client = nullptr;
		client = best_client;
		if( !client ) {
			break;
		}

		dprintf(D_FULLDEBUG,
				"TransferQueueManager: sending GoAhead to %s.\n",
				client->Description() );

		if( !client->SendGoAhead() ) {
			dprintf(D_FULLDEBUG,
					"TransferQueueManager: failed to send GoAhead; "
					"dequeueing %s.\n",
					client->Description() );

			m_xfer_queue.erase(std::find(m_xfer_queue.begin(), m_xfer_queue.end(), client));
			delete client;

			TransferQueueChanged();
		}
		else {
			SetRoundRobinRecency(client->m_up_down_queue_user);
			TransferQueueUser &user = GetUserRec(client->m_up_down_queue_user);
			user.running += 1;
			user.idle -= 1;
			if( client->m_downloading ) {
				downloading += 1;
			}
			else {
				uploading += 1;
			}
		}
	}


		// now that we have finished scheduling new transfers,
		// examine requests that are still waiting
	for (TransferQueueRequest *client : m_xfer_queue) {
		if( !client->m_gave_go_ahead ) {
			clients_waiting = true;

			TransferQueueUser &user = GetUserRec(client->m_up_down_queue_user);
			time_t age = time(nullptr) - client->m_time_born;
			if( client->m_downloading ) {
				m_waiting_to_download++;
				if( age > m_download_wait_time ) {
					m_download_wait_time = age;
				}
				m_iostats.download_MB_waiting += client->m_sandbox_size_MB;
				user.iostats.download_MB_waiting += client->m_sandbox_size_MB;
			}
			else {
				m_waiting_to_upload++;
				if( age > m_upload_wait_time ) {
					m_upload_wait_time = age;
				}
				m_iostats.upload_MB_waiting += client->m_sandbox_size_MB;
				user.iostats.upload_MB_waiting += client->m_sandbox_size_MB;
			}
		}
	}

	m_uploading = uploading;
	m_downloading = downloading;


	if( clients_waiting ) {
			// queue is full; check for ancient clients
		auto it = m_xfer_queue.begin();
		while (it != m_xfer_queue.end()) {
			TransferQueueRequest *client  = *it;
			if( client->m_gave_go_ahead ) {
				time_t age = time(nullptr) - client->m_time_go_ahead;
				time_t max_queue_age = client->m_max_queue_age;
				if( max_queue_age > 0 && max_queue_age < age ) {
						// Killing this client will not stop the current
						// file that is being transfered by it (which
						// presumably has stalled for some reason).  However,
						// it should prevent any additional files in the
						// sandbox from being transferred.
					dprintf(D_ALWAYS,"TransferQueueManager: forcibly "
							"dequeueing  ancient (%llds old) entry for %s, "
							"because it is older than "
							"MAX_TRANSFER_QUEUE_AGE=%llds.\n",
							(long long)age,
							client->Description(),
							(long long)max_queue_age);


					notifyAboutTransfersTakingTooLong();

					delete client;
					// This invaliates it, but we're breaking out of the loop
					it = m_xfer_queue.erase(it);
					TransferQueueChanged();
						// Only delete more ancient clients if the
						// next pass of this function finds there is pressure
						// on the queue.
					break;
				}
			}
			it++;
		}
	}
}

void
TransferQueueManager::notifyAboutTransfersTakingTooLong()
{
	FILE *email = nullptr;

	static time_t lastNotifiedTime = 0;
	time_t now = time(nullptr);
	const time_t quiet_interval = 3600 * 24;

	for (TransferQueueRequest *client: m_xfer_queue) {
		if (client->m_gave_go_ahead && ((now - lastNotifiedTime) > quiet_interval)) {
			time_t age = now - client->m_time_go_ahead;
			time_t max_queue_age = client->m_max_queue_age;
			if( max_queue_age > 0 && max_queue_age < age ) {
				lastNotifiedTime = now;
				if( !email ) {
					email = email_admin_open("file transfer took too long");
					if( !email ) {
							// Error sending the message
						dprintf( D_ALWAYS, 
								 "ERROR: Can't send email to the Condor "
								 "Administrator\n" );
						return;
					}
					fprintf( email,
							 "Below is a list of file transfers that took longer than\n"
							 "MAX_TRANSFER_QUEUE_AGE=%llds.  When other transfers are waiting\n"
							 "to start, these old transfer attempts will be aborted.\n"
							 "To avoid this timeout, MAX_TRANSFER_QUEUE_AGE may be increased,\n"
							 "but be aware that transfers which take a long time will delay other\n"
							 "transfers from starting if the maximum number of concurrent transfers\n"
							 "is exceeded.  Therefore, it is advisable to also review the settings\n"
							 "of MAX_CONCURRENT_UPLOADS, MAX_CONCURRENT_DOWNLOADS, and/or\n"
							 "FILE_TRANSFER_DISK_LOAD_THROTTLE.\n\n"
							 "The transfer queue currently has %d/%d uploads,\n"
							 "%d/%d downloads, %d transfers waiting %llds to upload,\n"
							 "and %d transfers waiting %llds to download.\n",
							 (long long)max_queue_age,
							 m_uploading,
							 m_max_uploads,
							 m_downloading,
							 m_max_downloads,
							 m_waiting_to_upload,
							 (long long)m_upload_wait_time,
							 m_waiting_to_download,
							 (long long)m_download_wait_time);

					char const *ema_horizon = m_iostats.bytes_sent.ShortestHorizonEMAName();
					if( ema_horizon ) {
						fprintf(email,
								"Upload %s I/O load: %.0f bytes/s  %.3f disk load  %.3f net load\n",
								ema_horizon,
								m_iostats.bytes_sent.EMAValue(ema_horizon),
								m_iostats.file_read.EMAValue(ema_horizon),
								m_iostats.net_write.EMAValue(ema_horizon));

						fprintf(email,
								"Download %s I/O load: %.0f bytes/s  %.3f disk load  %.3f net load\n",
								ema_horizon,
								m_iostats.bytes_received.EMAValue(ema_horizon),
								m_iostats.file_write.EMAValue(ema_horizon),
								m_iostats.net_read.EMAValue(ema_horizon));
					}
					fprintf(email,"\n\nTransfers older than MAX_TRANSFER_QUEUE_AGE=%llds:\n\n",(long long)max_queue_age);
				}

				fprintf( email, "%s\n", client->SinlessDescription() );
			}
		}
	}
	if( email ) {
		email_close ( email );
	}
}

bool
TransferQueueManager::GetContactInfo(char const *command_sock_addr, std::string &contact_str) const
{
	TransferQueueContactInfo contact(
		command_sock_addr,
		m_max_uploads == 0,
		m_max_downloads == 0);
	return contact.GetStringRepresentation(contact_str);
}

void
IOStats::Add(IOStats &s) {
	bytes_sent += s.bytes_sent.value;
	bytes_received += s.bytes_received.value;
	file_read += s.file_read.value;
	file_write += s.file_write.value;
	net_read += s.net_read.value;
	net_write += s.net_write.value;
}

void
IOStats::Clear() {
	bytes_sent.Clear();
	bytes_received.Clear();
	file_read.Clear();
	file_write.Clear();
	net_read.Clear();
	net_write.Clear();
}

void
IOStats::ConfigureEMAHorizons(const std::shared_ptr<stats_ema_config>& config) {
	bytes_sent.ConfigureEMAHorizons(config);
	bytes_received.ConfigureEMAHorizons(config);
	file_read.ConfigureEMAHorizons(config);
	file_write.ConfigureEMAHorizons(config);
	net_read.ConfigureEMAHorizons(config);
	net_write.ConfigureEMAHorizons(config);
}

void
TransferQueueManager::AddRecentIOStats(IOStats &s,const std::string &up_down_queue_user)
{
	m_iostats.Add(s);
	GetUserRec(up_down_queue_user).iostats.Add(s);
}

void
TransferQueueManager::UpdateIOStats( int /* timerID */ )
{
	m_max_uploading_stat = m_max_uploads;
	m_max_downloading_stat = m_max_downloads;
	m_uploading_stat = m_uploading;
	m_downloading_stat = m_downloading;
	m_waiting_to_upload_stat = m_waiting_to_upload;
	m_waiting_to_download_stat = m_waiting_to_download;
	m_upload_wait_time_stat = m_upload_wait_time;
	m_download_wait_time_stat = m_download_wait_time;
	m_disk_throttle_low_stat = m_disk_load_low_throttle;
	m_disk_throttle_high_stat = m_disk_load_high_throttle;
	m_disk_throttle_limit_stat = m_throttle_disk_load_max_concurrency;

	double disk_load_short = m_iostats.file_read.EMAValue(m_disk_throttle_short_horizon.c_str()) +
	                         m_iostats.file_write.EMAValue(m_disk_throttle_short_horizon.c_str());
	double excess = disk_load_short - m_disk_load_high_throttle;
	if( excess < 0 || !m_throttle_disk_load ) {
		excess = 0.0;
	}
	double shortfall = m_disk_load_high_throttle - disk_load_short;
	if( shortfall < 0 || !m_throttle_disk_load ) {
		shortfall = 0.0;
	}
	if( excess > 0 || m_waiting_to_upload>0 || m_waiting_to_download>0 ) {
		m_disk_throttle_excess = excess;
		m_disk_throttle_shortfall = shortfall;
	}
	else {
		m_disk_throttle_excess.SkipInterval();
		m_disk_throttle_shortfall.SkipInterval();
	}

	m_stat_pool.Advance(1);

	IOStatsChanged();
}

void
TransferQueueManager::publish(ClassAd *ad, char const *publish_config)
{
	int publish_flags = m_publish_flags;
	if (publish_config && publish_config[0]) {
		publish_flags = generic_stats_ParseConfigString(publish_config, "TRANSFER", "TRANSFER", publish_flags);
	}
	publish(ad,publish_flags);
}

void
TransferQueueManager::publish(ClassAd *ad)
{
	publish(ad,m_publish_flags);
}

void
TransferQueueManager::publish(ClassAd *ad,int pubflags)
{
	int d_level = D_FULLDEBUG;
	bool is_active = m_uploading > 0 || m_downloading > 0 || m_waiting_to_upload > 0 || m_waiting_to_download > 0;

	char const *ema_horizon = m_iostats.bytes_sent.ShortestHorizonEMAName();
	double up_bytes_sent=0.0, up_file_read=0.0, up_net_write=0.0;
	double down_bytes_received=0.0, down_file_write=0.0, down_net_read=0.0;
	if( ema_horizon ) {
		up_bytes_sent = m_iostats.bytes_sent.EMAValue(ema_horizon);
		up_file_read = m_iostats.file_read.EMAValue(ema_horizon);
		up_net_write = m_iostats.net_write.EMAValue(ema_horizon);
		if (up_bytes_sent > 0.0 || up_file_read > 0.0 || up_net_write > 0.0) { is_active = true; }

		down_bytes_received = m_iostats.bytes_received.EMAValue(ema_horizon);
		down_file_write = m_iostats.file_write.EMAValue(ema_horizon);
		down_net_read = m_iostats.net_read.EMAValue(ema_horizon);
		if (down_bytes_received > 0.0 || down_file_write > 0.0 || down_net_read > 0.0) { is_active = true; }
	}

	// report transfer queue idle only once until it transitions to non-idle again.
	static bool already_reported_idleness = false;
	if (is_active || !already_reported_idleness) {
		d_level = D_ALWAYS;
	}

	dprintf(d_level,"TransferQueueManager stats: active up=%d/%d down=%d/%d; waiting up=%d down=%d; wait time up=%llds down=%llds\n",
			m_uploading,
			m_max_uploads,
			m_downloading,
			m_max_downloads,
			m_waiting_to_upload,
			m_waiting_to_download,
			(long long)m_upload_wait_time,
			(long long)m_download_wait_time);

	if( ema_horizon ) {
		dprintf(d_level,"TransferQueueManager upload %s I/O load: %.0f bytes/s  %.3f disk load  %.3f net load\n",
				ema_horizon, up_bytes_sent, up_file_read, up_net_write);

		dprintf(d_level,"TransferQueueManager download %s I/O load: %.0f bytes/s  %.3f disk load  %.3f net load\n",
				ema_horizon, down_bytes_received, down_file_write, down_net_read);
	}

	if (is_active) {
		already_reported_idleness = false;
	} else {
		already_reported_idleness = true;
	}

	m_stat_pool.Publish(*ad,pubflags);

	CollectUserRecGarbage(ad);
}

void
TransferQueueManager::publish_user_stats(ClassAd * ad, const char *user, int flags)
{
	// FIXME! shouldn't need
	std::string up_down_user;
	up_down_user = "DOwner_"; // D + Owner_ prefix.
	up_down_user += user;

	const int ema_flags = flags | stats_entry_sum_ema_rate<double>::PubDefault;

	auto itr = m_queue_users.find(up_down_user);
	if (itr != m_queue_users.end()) {
		TransferQueueUser &dn = itr->second;

		dn.iostats.bytes_received.Publish(*ad, "FileTransferDownloadBytes", ema_flags);
		dn.iostats.file_write.Publish(*ad, "FileTransferFileWriteSeconds", ema_flags);
		dn.iostats.net_read.Publish(*ad, "FileTransferNetReadSeconds", ema_flags);
		dn.iostats.download_MB_waiting.Publish(*ad, "FileTransferMBWaitingToDownload", flags|dn.iostats.download_MB_waiting.PubValue);
	} else {
		// if there are now counters for this user, then remove the attributes
		// we can use the overall stats to unpublish, since that have the same EMA config as the per-user stats.
		m_iostats.bytes_received.Unpublish(*ad, "FileTransferDownloadBytes");
		m_iostats.file_write.Unpublish(*ad, "FileTransferFileWriteSeconds");
		m_iostats.net_read.Unpublish(*ad, "FileTransferNetReadSeconds");
		m_iostats.download_MB_waiting.Unpublish(*ad, "FileTransferMBWaitingToDownload");
	}

	up_down_user[0] = 'U';
	itr = m_queue_users.find(up_down_user);
	if (itr != m_queue_users.end()) {
		TransferQueueUser &up = itr->second;

		up.iostats.bytes_sent.Publish(*ad,"FileTransferUploadBytes",ema_flags);
		up.iostats.file_read.Publish(*ad,"FileTransferFileReadSeconds",ema_flags);
		up.iostats.net_write.Publish(*ad,"FileTransferNetWriteSeconds",ema_flags);
		up.iostats.upload_MB_waiting.Publish(*ad,"FileTransferMBWaitingToUpload", flags|up.iostats.upload_MB_waiting.PubValue);
	} else {
		// if there are now counters for this user, then remove the attributes
		// we can use the overall stats to unpublish, since that have the same EMA config as the per-user stats.
		m_iostats.bytes_sent.Unpublish(*ad,"FileTransferUploadBytes");
		m_iostats.file_read.Unpublish(*ad,"FileTransferFileReadSeconds");
		m_iostats.net_write.Unpublish(*ad,"FileTransferNetWriteSeconds");
		m_iostats.upload_MB_waiting.Unpublish(*ad,"FileTransferMBWaitingToUpload");
	}
}
