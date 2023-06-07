from ._common_imports import (
    classad,
    Collector,
    DaemonType,
    CredType,
    CredCheck,
)

from .htcondor2_impl import (
    _startd_drain_jobs,
)


class Credd():

    def __init__(self, location : classad.ClassAd = None):
        if location is None:
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
        pass


    def delete_password(self, user : str = None) -> None:
        pass


    def query_password(self, user : str = None) -> bool:
        pass


    def add_user_cred(self, credtype : CredType, credential : bytes, user : str = None ) -> None:
        pass


    def delete_user_cred(self, credtype : CredType, user : str = None) -> None:
        pass


    def query_user_cred(self, credtype : CredType, user : str = None) -> int:
        pass


    def add_user_service_cred(self, credtype : CredType, credential : bytes, service : str, handle : str = None, user : str = None) -> None:
        pass


    def delete_user_service_cred(self, credtype : CredType, service : str, handle : str = None, user : str = None) -> None:
        pass


    # In version 1, this returned a CredStatus instance, but the CredStatus
    # class defined only __str__(), and no function in the API accepted a
    # CredStatus as an argument.
    def query_user_service_cred(self, credtype : CredType, service : str, handle : str = None, user : str = None) -> str:
        pass


    def check_user_service_creds(self, credtype : CredType, services : list[str], user : str = None) -> CredCheck:
        pass
