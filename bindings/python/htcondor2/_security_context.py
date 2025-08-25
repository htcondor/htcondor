

class SecurityContext():
    """
    Alterations to the default configuration used when performing
    authentication between with an HTCondor daemon over the network.
    """

    def __init__(self, token: str = None):
        self._preferred_token = token

    def _getPreferredToken(self):
        return self._preferred_token

    def _setPreferredToken(self, token: str = None):
        self._preferred_token = token

    def _delPreferredToken(self):
        del self._preferred_token

    preferred_token = property(
        _getPreferredToken,
        _setPreferredToken,
        _delPreferredToken,
        """The token (by value) to try when using this security context."""
    )
