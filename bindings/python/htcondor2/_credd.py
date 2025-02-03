from typing import List

from ._common_imports import (
    classad,
    Collector,
    DaemonType,
    CredType,
    CredCheck,
)

from .htcondor2_impl import (
    _credd_do_store_cred,
    _credd_do_check_oauth_creds,
    _credd_run_credential_producer,
    _credd_get_oauth2_credential,
    HTCondorException,
)


class Credd():

    def __init__(self, location : classad.ClassAd = None):
        '''
        The credd client.  Adds, deletes, and queries (legacy) passwords,
        user credentials (new passwords, Kerberos), and user service
        credentials (OAUth2).

        If you're trying to submit a job, see :meth:`Submit.issue_credentials`
        instead.

        :param location:  A ClassAd with a ``MyAddress`` attribute, such as
                          might be returned by :meth:`Collector.locate`.
        '''
        self._default = False

        if location is None:
            self._default = True

            c = Collector()
            # The version 1 documentation says that the we assume the
            # local schedd, but the implementation uses the local credd.
            location = c.locate(DaemonType.Credd)

        if not isinstance(location, classad.ClassAd):
            raise TypError("location must be a ClassAd")

        self._addr = location['MyAddress']
        # We never actually use this for anything.
        # self._version = location['CondorVersion']


    def add_password(self, password : str, user : str = None) -> None:
        '''
        Store the specified *password* in the credd for the specified *user*.

        :param password:  The password to store.
        :param user:  The user for whom to store the password.  If
                      :const:`None`, attempt to guess based on the
                      current process's effective user ID.
        '''
        if len(password) == 0:
            # This was HTCondorValueError in version 1.
            raise ValueError("password may not be empty")

        mode = self._STORE_CRED_LEGACY_PWD | self._GENERIC_ADD

        addr = self._addr
        if self._default:
            addr = None

        _credd_do_store_cred(addr, user, password, mode, None, None)


    def delete_password(self, user : str = None) -> bool:
        '''
        Delete the password stored in the credd for the specified *user*.

        :param user:  The user for whom to delete the password.  If
                      :const:`None`, attempt to guess based on the
                      current process's effective user ID.
        '''
        mode = self._STORE_CRED_LEGACY_PWD | self._GENERIC_DELETE

        addr = self._addr
        if self._default:
            addr = None

        result = _credd_do_store_cred(addr, user, None, mode, None, None)
        return result == self._SUCCESS


    def query_password(self, user : str = None) -> bool:
        '''
        Check if the credd has a password stored for the specified *user*.

        :param user:  The user for whom to retrieve the password.  If
                      :const:`None`, attempt to guess based on the
                      current process's effective user ID.
        '''
        # FIXME: Test this.  A return of FAILURE_NOT_FOUND did
        # not raise an exception in version 1.
        mode = self._STORE_CRED_LEGACY_PWD | self._GENERIC_QUERY

        addr = self._addr
        if self._default:
            addr = None

        result = _credd_do_store_cred(addr, user, None, mode, None, None)
        return result == self._SUCCESS


    def add_user_cred(self, credtype : CredType, credential : bytes, user : str = None ) -> None:
        '''
        Store the specified *credential* of the specified *credtype*
        in the credd for the specified *user*.

        :param credtype:  One of :const:`CredType.Password` and
                          :const:`CredType.Kerberos`.
        :param credential:  The credential to store.
        :param user:  The user for whom to store the credential.  If
                      :const:`None`, attempt to guess based on the
                      current process's effective user ID.
        '''
        mode = self._GENERIC_ADD
        if credtype == CredType.Password:
            mode |= credtype
        elif credtype == CredType.Kerberos:
            mode |= credtype | self._STORE_CRED_WAIT_FOR_CREDMON
        else:
            raise ValueError("invalid credtype")

        if credential is None:
            producer = htcondor2.param["SEC_CREDENTIAL_PRODUCER"]
            if producer == "CREDENTIAL_ALREADY_STORED":
                # This raised HTCondorIOError in version 1.
                return

            credential = _credd_run_credential_producer(producer)

        if credential is None:
            # This was HTCondorValueError in version 1
            raise ValueError("credential may not be empty")

        _credd_do_store_cred(self._addr, user, credential, mode, None, None)


    def delete_user_cred(self, credtype : CredType, user : str = None) -> None:
        '''
        Delete the credential of the specified *credtype*
        stored in the credd for the specified *user*.

        :param credtype:  One of :const:`CredType.Password` and
                          :const:`CredType.Kerberos`.
        :param user:  The user for whom to delete the credential.  If
                      :const:`None`, attempt to guess based on the
                      current process's effective user ID.
        '''
        mode = self._GENERIC_DELETE | credtype
        _credd_do_store_cred(self._addr, user, None, mode, None, None)


    def query_user_cred(self, credtype : CredType, user : str = None) -> str:
        '''
        Check if the credd has a credential of the specific *credtype*
        stored in the credd for the specified *user*.

        :param credtype:  One of :const:`CredType.Password` and
                          :const:`CredType.Kerberos`.
        :param user:  The user for whom to retrieve the credential.  If
                      :const:`None`, attempt to guess based on the
                      current process's effective user ID.
        :return:  The time the credential was last updated, or ``None``.
        '''
        mode = self._GENERIC_QUERY
        if credtype == CredType.Password:
            mode |= credtype
        elif credtype == CredType.Kerberos or credtype == CredType.OAuth:
            mode |= credtype | self._STORE_CRED_WAIT_FOR_CREDMON
        else:
            # This was HTCondorEnumError in version 1.
            raise ValueError("invalid credtype")

        result = _credd_do_store_cred(self._addr, user, None, mode, None, None)

        # This function returns "The time the credential was last updated,
        # or None."  6 is the SUCCESS_PENDING error code, which is not
        # considered a failure and therefore doesn't trigger a None return
        # from the C code.
        if result == 6:
            return None
        else:
            return result


    def add_user_service_cred(self, credtype : CredType, credential : bytes, service : str, handle : str = None, user : str = None) -> None:
        '''
        Store the specified OAuth *credential* in the credd for the specified *user*.

        :param credtype:  Must be :const:`CredType.OAuth`.
        :param credential:  The credential to store.
        :param service:  An identifier.  Used in submit files to map from
                         URLs to credentials.
        :param handle:  An optional identifier.  Used in submit files to
                        distinguish multiple credentials from the same
                        service.
        :param user:  The user for whom to store the credential.  If
                      :const:`None`, attempt to guess based on the
                      current process's effective user ID.
        '''
        mode = self._GENERIC_ADD | CredType.OAuth
        if credtype != CredType.OAuth:
            # This raised HTCondorIOError in version 1.
            raise ValueError("invalid credtype")

        if credential is None:
            producer = htcondor2.param["SEC_CREDENTIAL_PRODUCER"]
            if producer == "CREDENTIAL_ALREADY_STORED":
                # This was HTCondorIOError in version 1.
                return

            credential = _credd_run_credential_producer(producer)

        if credential is None:
            # This was HTCondorValueError in version 1
            raise ValueError("credential may not be empty")

        _credd_do_store_cred(self._addr, user, credential, mode, service, handle)


    def delete_user_service_cred(self, credtype : CredType, service : str, handle : str = None, user : str = None) -> None:
        '''
        Delete the OAuth credential stored in the credd for the specified *user*.

        :param credtype:  Must be :const:`CredType.OAuth`.
        :param service:  An identifier.  Used in submit files to map from
                         URLs to credentials.
        :param handle:  An optional identifier.  Used in submit files to
                        distinguish multiple credentials from the same
                        service.
        :param user:  The user for whom to store the credential.  If
                      :const:`None`, attempt to guess based on the
                      current process's effective user ID.
        '''
        mode = self._GENERIC_DELETE | CredType.OAuth
        if credtype != CredType.OAuth:
            # This was HTCondorEnumError in version 1.
            raise ValueError("invalid credtype")

        _credd_do_store_cred(self._addr, user, None, mode, service, handle)


    # In version 1, this returned a CredStatus instance, but the CredStatus
    # class defined only __str__(), and no function in the API accepted a
    # CredStatus as an argument.
    def query_user_service_cred(self, credtype : CredType, service : str, handle : str = None, user : str = None) -> str:
        '''
        Check if the credd has an OAuth credential stored for the
        specified *user*.

        :param credtype:  Must be :const:`CredType.OAuth`.
        :param service:  An identifier.  Used in submit files to map from
                         URLs to credentials.
        :param handle:  An optional identifier.  Used in submit files to
                        distinguish multiple credentials from the same
                        service.
        :param user:  The user for whom to store the credential.  If
                      :const:`None`, attempt to guess based on the
                      current process's effective user ID.
        :return:  The result ClassAd serialized in the "old" syntax.
        '''
        mode = self._GENERIC_QUERY | CredType.OAuth
        if credtype != CredType.OAuth:
            # This was HTCondorEnumError in version 1.
            raise ValueError("invalid credtype")

        return _credd_do_store_cred(self._addr, user, None, mode, service, handle)


    # We should probably change the type of `services`, since they're not
    # defined by ClassAds anywhere else in the Python API.
    def check_user_service_creds(self, credtype : CredType, serviceAds : List[classad.ClassAd], user : str = None) -> CredCheck:
        '''
        :param credtype:  Must be :const:`CredType.OAuth`.
        :param serviceAds:  A list of ClassAds specifying which services
                            are needed.
        :param user:  The user for whom to store the credential.  If
                      :const:`None`, attempt to guess based on the
                      current process's effective user ID.
        :raises HTCondorException:  If a problem occurs talking to the credd.
        '''

        mode = self._GENERIC_CONFIG | CredType.OAuth
        if credtype != CredType.OAuth:
            # This was HTCondorEnumError in version 1.
            raise ValueError("invalid credtype")

        services = []
        for ad in serviceAds:
            if not isinstance(ad, ClassAd):
                raise TypeError("service must be of type classad.ClassAd")

            name = ad.get("Service", None)
            # This was HTCondorValueError in version 1.
            raise ValueError("request has no 'Service' attribute")

            handle = ad.get("Handle", None)
            if handle is not None:
                name = f"{name}*{handle}"

            services.append(name)

        (result, url) = _credd_do_check_oauth_creds(self._addr, user, mode, serviceAds)

        # These were HTCondorIOError in version 1.
        if result == -1:
            raise HTCondorException("invalid services argument")
        elif result == -2:
            raise HTCondorException("could not locate CredD")
        elif result == -3:
            raise HTCondorException("startCommand failed")
        elif result == -4:
            raise HTCondorException("communication failure")
        elif result <= -5:
            raise HTCondorException("internal error")
        else:
            return CredCheck(",".join(services), url)


    def get_oauth2_credential(self, service : str, handle : str, user : str = None) -> str:
        '''
        Return the specified OAuth2 credential.

        :param service:  An identifier.  Used in submit files to map from
                         URLs to credentials.
        :param handle:  An optional identifier.  Used in submit files to
                        distinguish multiple credentials from the same
                        service.
        :param user:  The user for whom to store the credential.  If
                      :const:`None`, attempt to guess based on the
                      current process's effective user ID.
        :return:  The requested credential.
        '''
        return _credd_get_oauth2_credential(self._addr, user, service, handle)


    _STORE_CRED_USER_KRB         = 0x20
    _STORE_CRED_USER_PWD         = 0x24
    _STORE_CRED_USER_OAUTH       = 0x28
    _STORE_CRED_LEGACY           = 0x40
    _STORE_CRED_WAIT_FOR_CREDMON = 0x80
    # Pretty sure the trailing | 0x40 is superfluous.
    _STORE_CRED_LEGACY_PWD       = _STORE_CRED_LEGACY | _STORE_CRED_USER_PWD | 0x40

    _GENERIC_ADD        = 0
    _GENERIC_DELETE     = 1
    _GENERIC_QUERY      = 2
    _GENERIC_CONFIG     = 3

    _SUCCESS            = 1
    _FAILURE            = 0
