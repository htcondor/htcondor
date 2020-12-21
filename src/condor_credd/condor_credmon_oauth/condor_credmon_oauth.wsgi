#
# Configure Logging
#

import os
import logging

# credmon library in libexec
import htcondor
import sys
sys.path.append(htcondor.param.get('libexec', '/usr/libexec/condor'))

from credmon.utils import setup_logging

log_path = htcondor.param.get('CREDMON_OAUTH_LOG',
               htcondor.param.get("SEC_CREDENTIAL_MONITOR_OAUTH_LOG",
                   htcondor.param.get("SEC_CREDENTIAL_MONITOR_LOG",
                       "/var/log/condor/CredMonOAuthLog")))
logger = setup_logging(log_path = log_path, log_level = logging.INFO)

#
# Load the session key
#
from credmon.utils import generate_secret_key
mykey = generate_secret_key()

#
# Start Service
#
from credmon.CredentialMonitors.OAuthCredmonWebserver import app
app.secret_key = mykey

application = app
