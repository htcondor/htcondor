

class SecurityContext():
    """
    Alterations to the default configuration used when performing
    authentication between with an HTCondor daemon over the network.
    """

    def __init__(self, token: str = None):
        self._preferred_token = token

    def setToken(self, token: str = None):
        self._preferred_token = token

    def setPreferredToken(self, token: str = None):
        self._preferred_token = token

    @property
    def preferredToken(self):
        return self._preferred_token
