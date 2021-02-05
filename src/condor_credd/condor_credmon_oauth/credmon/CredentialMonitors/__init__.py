# Not all of the dependencies for these will necessarily be installed,
#  so allow them to fail
try:
    from credmon.CredentialMonitors.OAuthCredmon import OAuthCredmon
except:
    pass
try:
    from credmon.CredentialMonitors.LocalCredmon import LocalCredmon
except:
    pass
try:
    from credmon.CredentialMonitors.VaultCredmon import VaultCredmon
except:
    pass
