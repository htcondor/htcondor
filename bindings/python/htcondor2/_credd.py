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
)


class Credd():

    def __init__(self, location : classad.ClassAd = None):
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
        if len(password) == 0:
            # This was HTCondorValueError in version 1.
            raise ValueError("password may not be empty")

        mode = self._STORE_CRED_LEGACY_PWD | self._GENERIC_ADD

        addr = self._addr
        if self._default:
            addr = None

        _credd_do_store_cred(addr, user, password, mode, None, None)


    def delete_password(self, user : str = None) -> bool:
        mode = self._STORE_CRED_LEGACY_PWD | self._GENERIC_DELETE

        addr = self._addr
        if self._default:
            addr = None

        result = _credd_do_store_cred(addr, user, None, mode, None, None)
        return result == self._SUCCESS


    def query_password(self, user : str = None) -> bool:
        # FIXME: Test this.  A return of FAILURE_NOT_FOUND did
        # not raise an exception in version 1.
        mode = self._STORE_CRED_LEGACY_PWD | self._GENERIC_QUERY

        addr = self._addr
        if self._default:
            addr = None

        result = _credd_do_store_cred(addr, user, None, mode, None, None)
        return result == self._SUCCESS


    def add_user_cred(self, credtype : CredType, credential : bytes, user : str = None ) -> None:
        mode = self._GENERIC_ADD
        if credtype == CredType.Password:
            mode |= credtype
        elif credtype == CredType.Kerberos:
            mode |= credtype | self._STORE_CRED_WAIT_FOR_CREDMON
        else:
            # This was HTCondorEnumError in version 1.
            raise RuntimeError("invalid credtype")

        if credential is None:
            producer = htcondor2.param["SEC_CREDENTIAL_PRODUCER"]
            if producer == "CREDENTIAL_ALREADY_STORED":
                # This was HTCondorIOError in version 1.
                raise IOError(producer)

            credential = _credd_run_credential_producer(producer)

        if credential is None:
            # This was HTCondorValueError in version 1
            raise ValueError("credential may not be empty")

        _credd_do_store_cred(self._addr, user, credential, mode, None, None)


    def delete_user_cred(self, credtype : CredType, user : str = None) -> None:
        mode = self._GENERIC_DELETE | credtype
        _credd_do_store_cred(self._addr, user, None, mode, None, None)


    def query_user_cred(self, credtype : CredType, user : str = None) -> int:
        mode = self._GENERIC_QUERY
        if credtype == CredType.Password:
            mode |= credtype
        elif credtype == CredType.Kerberos or credtype == CredType.OAuth:
            mode |= credtype | self._STORE_CRED_WAIT_FOR_CREDMON
        else:
            # This was HTCondorEnumError in version 1.
            raise RuntimeError("invalid credtype")

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
        mode = self._GENERIC_ADD | CredType.OAuth
        if credtype != CredType.OAuth:
            # This was HTCondorEnumError in version 1.
            raise RuntimeError("invalid credtype")

        if credential is None:
            producer = htcondor2.param["SEC_CREDENTIAL_PRODUCER"]
            if producer == "CREDENTIAL_ALREADY_STORED":
                # This was HTCondorIOError in version 1.
                raise IOError(producer)

            credential = _credd_run_credential_producer(producer)

        if credential is None:
            # This was HTCondorValueError in version 1
            raise ValueError("credential may not be empty")

        _credd_do_store_cred(self._addr, user, credential, mode, service, handle)


    def delete_user_service_cred(self, credtype : CredType, service : str, handle : str = None, user : str = None) -> None:
        mode = self._GENERIC_DELETE | CredType.OAuth
        if credtype != CredType.OAuth:
            # This was HTCondorEnumError in version 1.
            raise RuntimeError("invalid credtype")

        _credd_do_store_cred(self._addr, user, None, mode, service, handle)


    # In version 1, this returned a CredStatus instance, but the CredStatus
    # class defined only __str__(), and no function in the API accepted a
    # CredStatus as an argument.
    def query_user_service_cred(self, credtype : CredType, service : str, handle : str = None, user : str = None) -> str:
        mode = self._GENERIC_QUERY | CredType.OAuth
        if credtype != CredType.OAuth:
            # This was HTCondorEnumError in version 1.
            raise RuntimeError("invalid credtype")

        return _credd_do_store_cred(self._addr, user, None, mode, service, handle)


    # We should probably change the type of `services`, since they're not
    # defined by ClassAds anywhere else in the Python API.
    def check_user_service_creds(self, credtype : CredType, serviceAds : List[classad.ClassAd], user : str = None) -> CredCheck:
        mode = self._GENERIC_CONFIG | CredType.OAuth
        if credtype != CredType.OAuth:
            # This was HTCondorEnumError in version 1.
            raise RuntimeError("invalid credtype")

        services = []
        for ad in serviceAds:
            if not isinstance(ad, ClassAd):
                raise ValueError("service must be of type classad.ClassAd")

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
            raise IOError("invalid services argument")
        elif result == -2:
            raise IOError("could not locate CredD")
        elif result == -3:
            raise IOError("startCommand failed")
        elif result == -4:
            raise IOError("communication failure")
        elif result <= -5:
            raise IOError("internal error")
        else:
            return CredCheck(",".join(services), url)


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
