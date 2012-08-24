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
#include "condor_config.h"
#include "gridmanager.h"
#include "transferrequest.h"


TransferRequest::TransferRequest( Proxy *proxy, const StringList &src_list,
								  const StringList &dst_list, int notify_tid )
	: m_src_urls( src_list ), m_dst_urls( dst_list )
{
	m_notify_tid = notify_tid;
	m_gahp = NULL;
	m_status = TransferQueued;

	m_proxy = AcquireProxy( proxy, (TimerHandlercpp)&TransferRequest::CheckRequest, this );

	m_CheckRequest_tid = daemonCore->Register_Timer( 0,
							(TimerHandlercpp)&TransferRequest::CheckRequest,
							"TransferRequest::CheckRequest", (Service*)this );

	if ( m_src_urls.number() != m_dst_urls.number() ) {
		formatstr( m_errMsg, "Unenven number of source and destination URLs" );
		m_status = TransferFailed;
		return;
	}

	std::string buff;
	char *gahp_path = param( "NORDUGRID_GAHP" );
	if ( gahp_path == NULL ) {
		formatstr( m_errMsg, "NORDUGRID_GAHP not defined" );
		m_status = TransferFailed;
		return;
	}
	formatstr( buff, "NORDUGRID/%s", m_proxy->subject->fqan );
	m_gahp = new GahpClient( buff.c_str(), gahp_path );
	m_gahp->setNotificationTimerId( m_CheckRequest_tid );
	m_gahp->setMode( GahpClient::normal );
	m_gahp->setTimeout( param_integer( "GRIDMANAGER_GAHP_CALL_TIMEOUT", 5 * 60 ) );

	free( gahp_path );
}

TransferRequest::~TransferRequest()
{
	if ( m_CheckRequest_tid != TIMER_UNSET ) {
		daemonCore->Cancel_Timer( m_CheckRequest_tid );
	}
	if ( m_proxy ) {
		ReleaseProxy( m_proxy, (TimerHandlercpp)&TransferRequest::CheckRequest, this );
	}
	delete m_gahp;
}

void TransferRequest::CheckRequest()
{
	daemonCore->Reset_Timer( m_CheckRequest_tid, TIMER_NEVER );

	if ( m_status == TransferFailed || m_status == TransferDone ) {
		return;
	}

	if ( !m_gahp->Initialize( m_proxy ) ) {
		m_errMsg = "Failed to start gahp";
		m_status = TransferFailed;
		daemonCore->Reset_Timer( m_notify_tid, 0 );
		return;
	}

	if ( m_status == TransferQueued ) {
		m_src_urls.rewind();
		m_dst_urls.rewind();

		const char *first_src = m_src_urls.next();
		const char *first_dst = m_dst_urls.next();

		int rc = m_gahp->gridftp_transfer( first_src, first_dst );
		if ( rc != GAHPCLIENT_COMMAND_PENDING ) {
			formatstr( m_errMsg, "Failed to start transfer request" );
			m_status = TransferFailed;
			daemonCore->Reset_Timer( m_notify_tid, 0 );
			return;
		}
		m_status = TransferActive;
	} else {
		int rc = m_gahp->gridftp_transfer( NULL, NULL );
		if ( rc == GAHPCLIENT_COMMAND_PENDING ) {
			return;
		}
		if ( rc != 0 ) {
			formatstr( m_errMsg, "Transfer failed: %s", m_gahp->getErrorString() );
			m_status = TransferFailed;
			daemonCore->Reset_Timer( m_notify_tid, 0 );
			return;
		}

		const char *next_src = m_src_urls.next();
		const char *next_dst = m_dst_urls.next();

		if ( !next_src || !next_dst ) {
			m_status = TransferDone;
			daemonCore->Reset_Timer( m_notify_tid, 0 );
			return;
		}

		rc = m_gahp->gridftp_transfer( next_src, next_dst );
		if ( rc != GAHPCLIENT_COMMAND_PENDING ) {
			formatstr( m_errMsg, "Failed to start transfer request" );
			m_status = TransferFailed;
			daemonCore->Reset_Timer( m_notify_tid, 0 );
			return;
		}
	}
}
