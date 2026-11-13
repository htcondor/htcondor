from typing import (
    List,
    Optional,
    Union,
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
    _placement_query,
    _placement_edit,
)


class Placementd():
    """
    The placementd client. Talk with the placementd to request and query about
    placement tokens for an AP.
    """

    def __init__(self, location : Optional[classad.ClassAd] = None):
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
        project : Optional[str] = None,
        requester : Optional[str] = None,
    ) -> classad.ClassAd:
        """
        Request a user placement token.

        :param username:  Sign-in identity of the user who'll use the token.
        :param authorizations:  What authorizations token should grant.
        :param project:  What AP porject should jobs be tied to
        :param requester:  Sign-in identity of the user making the request.
            :py:obj:`None` means the same value as username.

        :return:  A ClassAd with the token and additional information.
        """
        return _placement_user_login(self._addr, username, authorizations, project, requester)

    def queryUsers(self,
        username : Optional[str] = None,
    ) -> List[classad.ClassAd]:
        """
        Query information about users who are currently authorized.

        :param username:  Sign-in identity of a specific user.
            :py:obj:`None` will return information about all users.

        :return:  A list ClassAds with information about each user.
        """
        return _placement_query_users(self._addr, username)

    def queryTokens(self,
        username : Optional[str] = None,
        valid_only : bool = False,
        token_id : Optional[str] = None,
    ) -> List[classad.ClassAd]:
        """
        Query information about issued tokens.

        :param username:  Sign-in identity of a specific user.
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
        username : Optional[str] = None,
    ) -> List[classad.ClassAd]:
        """
        Query information about defined authorizations.

        :param username:  Sign-in identity of a specific user.
            If not :py:obj:`None`, return information only about
            authorizations granted to the specified user.

        :return:  A list ClassAds with information about each user.
        """
        return _placement_query_authorizations(self._addr, username)


    def query(self,
        target_type : str,
        id : Optional[str] = None,
        name : Optional[str] = None,
        constraint : Optional[Union[str, classad.ExprTree]] = None,
        projection : Optional[Union[List[str], str]] = None,
        limit : Optional[int] = None,
        entity_id : Optional[str] = None,
    ) -> List[classad.ClassAd]:
        """
        Query the placementd's identities, accounts, projects,
        authorizations, map file status, or audit log.

        Every entity ad carries ``Version`` (the id of its latest
        modification; it increases across the whole database, so compare
        it only for equality), ``ModificationId`` (the same value),
        ``CreatedAt``, ``UpdatedAt`` and ``ModifiedBy``.  ``ModifiedBy`` is
        the authenticated identity that made the latest change, or
        ``"mapfile"``; when the change named an end user, it reads
        ``"<actor> on behalf of <end user>"``.

        With ``target_type="Modification"`` the results are the audit log,
        newest first: one ``PlacementModification`` ad per change to one
        entity, with ``Id``, ``ChangesetId``, ``Time``, ``Channel``
        (``"cedar"`` or ``"mapfile"``), ``Actor``, ``OnBehalfOf``,
        ``PeerAddress``, ``Comment``, ``Operation`` (``"create"``,
        ``"update"``, ``"delete"``, ``"adopt"``, ``"reclaim"`` or
        ``"handoff"``), ``EntityType`` (``"identity"``, ``"account"``,
        ``"project"`` or ``"authorization"``), ``EntityId`` and
        ``Changes``, a nested ClassAd mapping each changed attribute to the
        list ``{old, new}``, where ``undefined`` means absent.  A
        modification that revoked tokens also has ``RevokedTokens``, as
        ``{undefined, count}``.

        :param target_type:  One of ``"Identity"``, ``"Account"``,
            ``"Project"``, ``"Authorization"``, ``"MapfileStatus"`` or
            ``"Modification"``.
        :param id:  Return only the entity with this unique id.
        :param name:  Return only the entity with this natural key: the
            identity for an ``"Identity"``, otherwise the name.
        :param constraint:  Return only the ads for which this
            expression evaluates to true.
        :param projection:  The attributes to return.  ``Id`` and
            ``MyType`` are always returned.  :py:obj:`None` returns all.
        :param limit:  Return at most this many ads.
        :param entity_id:  For ``"Modification"``, return only the
            history of the entity with this unique id.  The history of
            a deleted entity remains.

        :return:  A list of ClassAds, one per entity or modification.
        """
        if constraint is not None:
            constraint = str(constraint)
        if isinstance(projection, list):
            projection = ",".join(projection)
        if limit is None:
            limit = -1
        elif limit < 0:
            raise ValueError("limit must not be negative")
        return _placement_query(self._addr, target_type, id, name, constraint, projection, limit, entity_id)

    def edit(self,
        operations : List[Union[dict, classad.ClassAd]],
        dry_run : bool = False,
        on_behalf_of : Optional[str] = None,
        comment : Optional[str] = None,
    ) -> List[classad.ClassAd]:
        """
        Atomically apply a batch of edits.  If any operation fails, none
        are applied and :class:`HTCondorException` is raised, naming the
        error code and the failing operation.  The exception's ``results``
        attribute is the list of result ClassAds: the failing one carries
        ``ErrorCode`` and ``ErrorString``, and none carries an ``Id`` or
        ``Version``, since nothing was written.  A batch that commits is
        recorded as one change set in the audit log.

        :param operations:  The operations, in order.  Each is a ClassAd
            (or a dictionary) with an ``Operation`` (``"Create"``,
            ``"Update"``, ``"Delete"``, ``"Adopt"`` or ``"Revoke"``), a
            ``TargetType``, an ``Id`` or key selector (except for
            ``"Create"``), and the attributes to set.  The key is
            ``Identity`` for an identity and ``Name`` for everything else.
            ``"Revoke"`` revokes the unexpired tokens issued for an
            ``"Identity"`` or an ``"Account"`` without changing it; edits
            that remove or narrow what a token was issued for revoke it
            too.
        :param dry_run:  If :py:obj:`True`, validate the batch and return
            the results it would have had, but change nothing.
        :param on_behalf_of:  The end user this change is made for, such as
            the signed-in user of a web front end.  It is recorded in the
            audit log, but authorization is always the caller's own.
        :param comment:  A note recorded with the change set.

        :return:  One result ClassAd per operation, with the ``Index``,
            ``Id`` and ``Version`` of the row, a ``Warning`` if any, and
            ``RevokedTokens``, the number of tokens the operation revoked,
            if it revoked any (always, for ``"Revoke"``).
        """
        ads = []
        for op in operations:
            if isinstance(op, dict):
                op = classad.ClassAd(op)
            if not isinstance(op, classad.ClassAd):
                raise TypeError("operations must be ClassAds or dictionaries")
            ads.append(op)
        if len(ads) == 0:
            raise ValueError("operations must not be empty")
        return _placement_edit(self._addr, ads, bool(dry_run), on_behalf_of, comment)
