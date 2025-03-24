from typing import Union
from typing import Optional
from typing import List
from typing import Tuple

from ._common_imports import classad
from .htcondor2_impl import _send_generic_payload_command
from ._schedd import Schedd

class DagmanConnectionError(Exception):
    """
    Custom error for internal use to be raised when a DAGMan
    connection fails and should be retried.
    """
    def __init__(self, msg: Optional[str] = None) -> None:
        self.message = msg if msg is not None else "Failed to connect to DAGMan"
        super().__init__(self.message)

class DAGMan():
    """
    The DAGMan client that acts as a connection to a running DAGMan
    process to send commands.
    """

    class ContactInfo():
        """
        DAGMan contact information. Sinful address to send command
        and secret DAGMan will use to verify trust.
        """
        def __init__(self, addr: str, secret: str) -> None:
            self.addr = addr
            self.secret = secret

    def __init__(self, dag_id: Optional[int] = None) -> None:
        self.dag_id = dag_id
        self.contact = None

    def connect(self, dag_id: int) -> tuple[int, str]:
        self.contact = None
        self.dag_id = dag_id

        return self.__locate()

    def send_hello(self, echo: Optional[str] = None) -> bool:
        request = classad.ClassAd({"DagCommand" : 1})
        if echo is not None:
            request["Echo"] = echo

        result, ret, err = self.__contact_dagman(10000, request)

        if ret != 0:
            print(f"ERROR: Failed to send hello to DAGMan: {err}")
            return False

        if not result.get("Success", False):
            print(f"ERROR: Command failed: {result.get('FailureReason')}")
            return False

        print(f"DAG [{result['Pid']}]: {result['Response']}")

        return True

    def halt(self, reason: Optional[str] = None, pause: Optional[bool] = None) -> tuple[bool, str]:
        """Tell DAGMan to halt DAG progress"""
        request = classad.ClassAd({"DagCommand" : 2})

        if pause is not None:
            request["IsPause"] = pause

        if reason is not None:
            request["HaltReason"] = reason

        result, ret, err = self.__contact_dagman(10000, request)

        if ret != 0:
            return (False, err)

        if not result.get("Success", False):
            return (False, result.get('FailureReason'))

        action = "Paused" if pause is not None and pause else "Halted"

        return (True, f"{action} DAG {self.dag_id}")

    def resume(self) -> tuple[bool, str]:
        """Tell DAGMan to resume DAG progress"""
        request = classad.ClassAd({"DagCommand" : 3})

        result, ret, err = self.__contact_dagman(10000, request)

        if ret != 0:
            return (False, err)

        if not result.get("Success", False):
            return (False, result.get('FailureReason'))

        return (True, f"Resumed DAG {self.dag_id}")

    def __contact_dagman(self, cmd: int, request: classad.ClassAd) -> tuple[classad.ClassAd, int, str]:
        if self.contact is None:
            ret, err = self.__locate()
            if ret != 0:
                return (None, ret, err)

        request["ContactSecret"] = self.contact.secret

        try:
            result, ret, err = _send_generic_payload_command(self.contact.addr, int(cmd), str(request))
            if ret in [1, 2]:
                raise DagmanConnectionError(f"Connection to {self.contact.addr} ({self.dag_id}) failed: {err}")
            elif not result.get("Trusted", False):
                raise DagmanConnectionError(f"Connection to {self.contact.addr} ({self.dag_id}) failed: {result.get('FailureReason')}")
        except DagmanConnectionError as e:
            self.contact = None
            ret, err = self.__locate()
            if ret != 0:
                return (None, ret, err)

            result, ret, err = _send_generic_payload_command(self.contact.addr, cmd, str(request))

        return (result, ret, err)

    def __locate(self) -> tuple[int, str]:
        if self.dag_id is None:
            return (1, "No DAG ID specified")

        schedd = Schedd()

        try:
            ad = schedd._get_dag_contact_info(self.dag_id)
        except Exception as e:
            return (1, f"Failed to get contact information: {str(e)}")

        if not ad.get("Success", False):
            return (1, ad.get("FailureReason", "(UNKNOWN)"))

        self.contact = DAGMan.ContactInfo(ad["Address"], ad["Secret"])
        return (0, None)
