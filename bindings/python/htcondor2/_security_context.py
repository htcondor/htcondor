from typing import Optional


class SecurityContext():
    """
    Alterations to the default configuration used when authenticating
    with an HTCondor daemon over the network.

    A :class:`SecurityContext` may be passed to functions and methods that
    contact a daemon -- for example :func:`ping` and the
    :class:`Collector` constructor -- to override how that particular
    connection authenticates.
    """

    def __init__(self, token: Optional[str] = None):
        """
        :param token: The token (by value) to prefer when authenticating
                      using this security context.  See
                      :attr:`preferred_token`.
        """
        self._preferred_token = token

    def _getPreferredToken(self):
        return self._preferred_token

    def _setPreferredToken(self, token: Optional[str] = None):
        self._preferred_token = token

    def _delPreferredToken(self):
        del self._preferred_token

    preferred_token = property(
        _getPreferredToken,
        _setPreferredToken,
        _delPreferredToken,
        """The token (by value) to try when using this security context."""
    )
