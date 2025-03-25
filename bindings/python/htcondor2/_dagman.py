from typing import Union
from typing import Optional
from typing import List
from typing import Tuple

from ._common_imports import classad
from .htcondor2_impl import _send_generic_payload_command
from ._schedd import Schedd

import enum

class DagmanConnectionError(Exception):
    """
    Custom error for internal use to be raised when a DAGMan
    connection fails and should be retried one more time.
    """
    def __init__(self, msg: Optional[str] = None) -> None:
        self.message = msg if msg is not None else "Failed to connect to DAGMan"
        super().__init__(self.message)


class DAGMan():
    """
    The DAGMan client that acts as a connection to a running DAGMan
    process for sending commands.
    """

    # Actual DaemonCore command int from condor_commands.h
    CMD_DAGMAN_GENERIC = 61500

    # Keep inline with dagman_commands.h enumeration
    class GenericCommand(enum.IntEnum):
        """
        Internal mapping of specific generic DAG commands.
        """

        HALT = 1
        RESUME = 2

    class ContactInfo():
        """
        DAGMan contact information. Sinful address to send command
        and secret DAGMan will use to verify trust.
        """
        def __init__(self, addr: str, secret: str) -> None:
            self.addr = addr
            self.secret = secret


    def __init__(self, dag_id: Optional[int] = None) -> None:
        """
        :param dag_id: The :ad-attr:`ClusterId` of a running DAG to
                       connect with once the first command is issued.
        """
        self.dag_id = dag_id
        self.contact = None


    def connect(self, dag_id: int) -> tuple[int, str]:
        """
        :param dag_id: The :ad-attr:`ClusterId` of a running DAG to
                       connect with immediately.
        """
        self.contact = None
        self.dag_id = dag_id

        return self.__locate()


    def halt(self, reason: Optional[str] = None, pause: Optional[bool] = None) -> tuple[bool, str]:
        """
        Inform DAGMan to halt DAG progress.

        :param reason: A message for why the DAG was halted to be
                       printed in the DAGs debug log.
        :return: Tuple containing a command success boolean and a
                 result message string.
        """
        request = classad.ClassAd({"DagCommand" : self.GenericCommand.HALT})

        if pause is not None:
            request["IsPause"] = pause

        if reason is not None:
            request["HaltReason"] = reason

        result, ret, err = self.__contact_dagman(self.CMD_DAGMAN_GENERIC, request)

        if ret != 0:
            return (False, err)

        if not result.get("Success", False):
            return (False, result.get('FailureReason'))

        action = "Paused" if pause is not None and pause else "Halted"

        return (True, f"{action} DAG {self.dag_id}")


    def resume(self) -> tuple[bool, str]:
        """
        Inform DAGMan to resume hatled DAG progress.

        :return: Tuple containing a command success boolean and a
                 result message string.
        """
        request = classad.ClassAd({"DagCommand" : self.GenericCommand.RESUME})

        result, ret, err = self.__contact_dagman(self.CMD_DAGMAN_GENERIC, request)

        if ret != 0:
            return (False, err)

        if not result.get("Success", False):
            return (False, result.get('FailureReason'))

        return (True, f"Resumed DAG {self.dag_id}")


    def __contact_dagman(self, cmd: int, request: classad.ClassAd) -> tuple[classad.ClassAd, int, str]:
        """
        Attempt to send command to DAGMan.

        :param cmd: DAGMan command integer.
        :param request: The :class:`classad.ClassAd` containing the specific
                        commands information.
        """

        # Get the DAGMan contact information if we don't already have it
        if self.contact is None:
            ret, err = self.__locate()
            if ret != 0:
                return (None, ret, err)

        # Add the contact secret to the request ad so DAGMan can verify us
        request["ContactSecret"] = self.contact.secret

        # Attempt to send command to DAGMan
        try:
            result, ret, err = _send_generic_payload_command(self.contact.addr, int(cmd), str(request))

            # Specific errors (initial connection and not trusted) may be a result of DAGMan going away
            if ret in [1, 2]:
                raise DagmanConnectionError(f"Connection to {self.contact.addr} ({self.dag_id}) failed: {err}")
            elif not result.get("Trusted", False):
                raise DagmanConnectionError(f"Connection to {self.contact.addr} ({self.dag_id}) failed: {result.get('FailureReason')}")
        except DagmanConnectionError as e:
            # Reset DAGMan contact information and retry command
            ret, err = self.__locate()
            if ret != 0:
                return (None, ret, err)

            result, ret, err = _send_generic_payload_command(self.contact.addr, int(cmd), str(request))

        return (result, ret, err)


    def __locate(self) -> tuple[int, str]:
        """
        Query the local Schedd for DAGMan contact information.
        """

        # Reset contact information
        self.contact = None

        if self.dag_id is None:
            return (1, "No DAG ID specified")

        try:
            schedd = Schedd()
            ad = schedd._get_dag_contact_info(self.dag_id)
        except Exception as e:
            return (1, f"Failed to get contact information: {str(e)}")

        if not ad.get("Success", False):
            return (1, ad.get("FailureReason", "(UNKNOWN)"))

        self.contact = DAGMan.ContactInfo(ad["Address"], ad["Secret"])
        return (0, None)

