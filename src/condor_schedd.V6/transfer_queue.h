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


#ifndef _CONDOR_TRANSFER_QUEUE_H
#define _CONDOR_TRANSFER_QUEUE_H


#include "dc_service.h"
#include "dc_transfer_queue.h"
#include "reli_sock.h"
#include "generic_stats.h"
#include <vector>

class IOStats {
 public:
	stats_entry_sum_ema_rate<double> bytes_sent;
	stats_entry_sum_ema_rate<double> bytes_received;
	stats_entry_sum_ema_rate<double> file_read;
	stats_entry_sum_ema_rate<double> file_write;
	stats_entry_sum_ema_rate<double> net_read;
	stats_entry_sum_ema_rate<double> net_write;
	stats_entry_abs<double> upload_MB_waiting;
	stats_entry_abs<double> download_MB_waiting;

	void Add(IOStats &s);
	void Clear();
	void ConfigureEMAHorizons(const std::shared_ptr<stats_ema_config>& config);
};

// transfer queue server's representation of a client
class TransferQueueRequest {
 public:
	TransferQueueRequest(ReliSock *sock,filesize_t sandbox_size,char const *fname,char const *jobid,char const *queue_user,bool downloading,time_t max_queue_age);
	~TransferQueueRequest();

	char const *Description();
	char const *SinlessDescription(); // client description without the <IP:port> part

	bool SendGoAhead(XFER_QUEUE_ENUM go_ahead=XFER_QUEUE_GO_AHEAD,char const *reason=NULL);

	bool ReadReport(class TransferQueueManager *manager) const;

	ReliSock *m_sock;
	std::string m_queue_user;   // Name of file transfer queue user. (TRANSFER_QUEUE_USER_EXPR)
	std::string m_up_down_queue_user; // queue user prefixed by "U" or "D" for upload/download
	std::string  m_jobid;   // For information purposes, the job associated with
	                    // this file transfer.
	double m_sandbox_size_MB;
	std::string  m_fname;   // File this client originally requested to transfer.
	                    // In current implementation, it may silently move on
	                    // to a different file without notifying us.
	bool m_downloading; // true if client wants to download a file; o.w. upload
	bool m_gave_go_ahead; // true if we told this client to go ahead

	time_t m_max_queue_age; // clean transfer from queue after this time
	                        // 0 indicates no limit
	time_t m_time_born;
	time_t m_time_go_ahead;

	std::string m_description; // buffer for Description()
};

class TransferQueueManager: public Service {
 public:
	TransferQueueManager();
	~TransferQueueManager();

	void InitAndReconfig();

	void RegisterHandlers();

	int HandleRequest(int cmd,Stream *stream);

	int HandleReport( Stream *sock );

		// Iterate through queue, pruning disconnected clients, and
		// giving the go ahead to those that deserve it.
	void CheckTransferQueue( int timerID = -1 );

		// This is called to register a future call to CheckTransferQueue.
	void TransferQueueChanged();

	bool GetContactInfo(char const *command_sock_addr, std::string &contact_str) const;

	int GetNumUploading() const { return m_uploading; }
	int GetNumDownloading() const { return m_downloading; }
	int GetNumWaitingToUpload() const { return m_waiting_to_upload; }
	int GetNumWaitingToDownload() const { return m_waiting_to_download; }
	time_t GetUploadWaitTime() const { return m_upload_wait_time; }
	time_t GetDownloadWaitTime() const { return m_download_wait_time; }
	int GetMaxUploading() const { return m_max_uploads; }
	int GetMaxDownloading() const { return m_max_downloads; }

	void publish(ClassAd *ad);
	void publish(ClassAd *ad, char const *publish_config);
	void publish(ClassAd *ad,int pubflags);
	void publish_user_stats(ClassAd * ad, const char *user, int pubflags);

	void AddRecentIOStats(IOStats &s,const std::string &up_down_queue_user);
 private:
	std::vector<TransferQueueRequest *> m_xfer_queue;
	int m_max_uploads{0};   // 0 if unlimited
	int m_max_downloads{0}; // 0 if unlimited
	time_t m_default_max_queue_age{0}; // 0 if unlimited

	bool m_throttle_disk_load{false};
	double m_disk_load_low_throttle{0};
	double m_disk_load_high_throttle{0};
	int m_throttle_disk_load_max_concurrency{0};
	time_t m_throttle_disk_load_incremented{0};
	time_t m_throttle_disk_load_increment_wait{60};
	std::string m_disk_throttle_short_horizon;
	std::string m_disk_throttle_long_horizon;

	int m_check_queue_timer{-1};

	int m_uploading{0};
	int m_downloading{0};
	int m_waiting_to_upload{0};
	int m_waiting_to_download{0};
	time_t m_upload_wait_time{0};
	time_t m_download_wait_time{0};

	stats_entry_abs<int> m_max_uploading_stat;
	stats_entry_abs<int> m_max_downloading_stat;
	stats_entry_abs<int> m_uploading_stat;
	stats_entry_abs<int> m_downloading_stat;
	stats_entry_abs<int> m_waiting_to_upload_stat;
	stats_entry_abs<int> m_waiting_to_download_stat;
	stats_entry_abs<int> m_upload_wait_time_stat;
	stats_entry_abs<int> m_download_wait_time_stat;

	stats_entry_abs<double> m_disk_throttle_low_stat;
	stats_entry_abs<double> m_disk_throttle_high_stat;
	stats_entry_abs<int> m_disk_throttle_limit_stat;
	stats_entry_ema<double> m_disk_throttle_excess;
	stats_entry_ema<double> m_disk_throttle_shortfall;

	unsigned int m_round_robin_counter{0}; // increments each time we send GoAhead to a client

	class TransferQueueUser {
	public:
		TransferQueueUser(): running(0), idle(0), recency(0) {}
		bool Stale(unsigned int stale_recency);
		unsigned int running;
		unsigned int idle;
		unsigned int recency; // round robin counter at time of last GoAhead
		IOStats iostats;
	};
	typedef std::map< std::string,TransferQueueUser > QueueUserMap;
	QueueUserMap m_queue_users;      // key = up_down_queue_user, value = TransferQueueUser record

	unsigned int m_round_robin_garbage_counter{0};
	time_t m_round_robin_garbage_time;

	IOStats m_iostats;
	int m_update_iostats_interval{0};
	int m_update_iostats_timer{-1};
	std::shared_ptr<stats_ema_config> ema_config;
	StatisticsPool m_stat_pool;
	int m_publish_flags{0};

	bool AddRequest( TransferQueueRequest *client );
	void RemoveRequest( TransferQueueRequest *client );

	TransferQueueUser &GetUserRec(const std::string &user);
	void SetRoundRobinRecency(const std::string &user);
	void CollectUserRecGarbage(ClassAd *unpublish_ad);
	void ClearRoundRobinRecency();
	void ClearTransferCounts();
	void UpdateIOStats( int timerID = -1 );
	void IOStatsChanged();
	void RegisterStats(char const *user,IOStats &iostats,bool unregister=false,ClassAd *unpublish_ad=NULL);
	void UnregisterStats(char const *user,IOStats &iostats,ClassAd *unpublish_ad) {
		RegisterStats(user,iostats,true,unpublish_ad);
	}

	void parseThrottleConfig(char const *config_param,bool &enable_throttle,double &low,double &high,std::string &throttle_short_horizon,std::string &throttle_long_horizon,time_t &throttle_increment_wait);
	void notifyAboutTransfersTakingTooLong();

};


#endif
