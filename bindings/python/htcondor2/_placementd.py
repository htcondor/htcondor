from typing import (
    List,
)

from ._common_imports import (
    classad,
    Collector,
    DaemonType,
)

from .htcondor2_impl import (
    _placement_user_login,
    _placement_query_users,
    _placement_query_tokens,
    _placement_query_authorizations,
)


class Placementd():
    """
    The placementd client. Talk with the placementd to request and query about
    placement tokens for an AP.
    """

    def __init__(self, location : classad.ClassAd = None):
        """
        :param location:  A ClassAd with a ``MyAddress`` attribute, such as
            might be returned by :meth:`htcondor2.Collector.locate`.  :py:obj:`None` means the
            the local daemon.
        """
        if location is None:
            c = Collector()
            location = c.locate(DaemonType.Placementd)

        if not isinstance(location, classad.ClassAd):
            raise TypeError("location must be a ClassAd")

        self._addr = location['MyAddress']
        # We never actually use this for anything.
        # self._version = location['CondorVersion']


    def userLogin(self,
        username : str,
        authorizations : str,
        requester : str = None,
    ) -> classad.ClassAd:
        """
        Request a user placement token.

        :param username:  Foreign identifier of the user who'll use the token.
        :param authorizations:  What authorizations token should grant.
        :param requester:  Foreign identifier of user making the request.
            :py:obj:`None` means the same value as username.

        :return:  A ClassAd with the token and additional information.
        """
        return _placement_user_login(self._addr, username, authorizations, requester)

    def queryUsers(self,
        username : str = None,
    ) -> List[classad.ClassAd]:
        """
        Query information about users who are currently authorized.

        :param username:  Foreign dentifier of a specific user.
            :py:obj:`None` will return information about all users.

        :return:  A list ClassAds with information about each user.
        """
        return _placement_query_users(self._addr, username)

    def queryTokens(self,
        username : str = None,
        valid_only : bool = False,
        token_id : str = None,
    ) -> List[classad.ClassAd]:
        """
        Query information about issued tokens.

        :param username:  Foreign dentifier of a specific user.
            :py:obj:`None` will return information about tokens for all users.
        :param valid_only:  If :py:obj:`True`, return information only
            about tokens that haven't expired.
        :param token_id:  Token unique id (i.e. jti claim).
            If not :py:obj:`None`, return information only about the token
            with the given id.

        :return:  A list ClassAds with information about each token.
        """
        return _placement_query_tokens(self._addr, username, valid_only, token_id)

    def queryAuthorizations(self,
        username : str = None,
    ) -> List[classad.ClassAd]:
        """
        Query information about defined authorizations.

        :param username:  Foreign dentifier of a specific user.
            If not :py:obj:`None`, return information only about
            authorizations granted to the specified user.

        :return:  A list ClassAds with information about each user.
        """
        return _placement_query_authorizations(self._addr, username)

