from ._common_imports import (
    classad,
)


# This is the HTCondor definition; YMMV.
def _isURL(c : str) -> bool:
    if c is None:
        return False
    if len(c) < 5:
        return False

    if not c[0].isalpha():
        return False

    i = 1
    while c[i].isalnum() or c[i] == '+' or c[i] == '-' or c[i] == '.':
        i += 1

    if c[i:i+3] == "://" and i + 3 <= len(c):
        return True

    return False


# The @properties are unnecessary, because we don't care if the attributes
# aren't read-only, but they do provide a convenient place on which to hang
# documentation,
class CredCheck():

    def __init__(self, services, url_or_error):
        self._services = services
        if isURL(url_or_error):
            self._url = url_or_error
            self._error = None
        else:
            self._error = url_or_error
            self._url = None


    def __str__(self):
        if self._url is not None:
            return self._url
        if self._error is not None:
            return self._error
        return self._service


    def __bool__(self):
        # In version 1, a CredCheck was true if an error had happened.
        return self._url is not None


    @property
    def present(self):
        """
        ``True`` if the necessary tokens are present in the CredD, or if there
        are no necessary tokens.  ``False`` if the necessary tokens are not
        present.  If ``False``, either the `url` attribute or the `error`
        attribute will be non-empty.
        """
        return bool(self)


    @property
    def url(self):
        """The URL to visit to acquire the necessary tokens, if any."""
        return self._url


    @property
    def error(self):
        """
        A message from the CredD when the Creds were not present and a URL
        could not be created to acquire them.
        """
        return self._error


    @property
    def services(self):
        # The last line should be :ad-attr:`OauthServicesNeeded`, but that
        # job-ad attribute isn't documented yet.
        """
        The list of services that were requested, as a comma separated
        list.  This will be the same as the job ClassAd attribute
        ``OAuthServicesNeeded``.
        """
        return self._services
