# credmon library in libexec
import htcondor
import sys
sys.path.append(htcondor.param.get('libexec', '/usr/libexec/condor'))

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
