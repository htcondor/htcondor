# Not all of the dependencies for these will necessarily be installed,
#  so allow them to fail
try:
    from credmon.CredentialMonitors.OAuthCredmon import OAuthCredmon
except ModuleNotFoundError:
    pass
try:
    from credmon.CredentialMonitors.LocalCredmon import LocalCredmon
except ModuleNotFoundError:
    pass
try:
    from credmon.CredentialMonitors.VaultCredmon import VaultCredmon
except ModuleNotFoundError:
    pass
try:
    from credmon.CredentialMonitors.ClientCredmon import ClientCredmon
except ModuleNotFoundError:
    pass
