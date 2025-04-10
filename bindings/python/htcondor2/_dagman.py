from typing import Optional
from typing import Tuple

from ._common_imports import classad
from .htcondor2_impl import _send_generic_payload_command
from ._schedd import Schedd

import enum

class DagmanConnectionError(Exception):
    """
    Custom error for internal use to be raised when a DAGMan
    connection fails and should be retried one more time.

    :meta private:
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
    _CMD_DAGMAN_GENERIC = 61500

    # Success return code of low level api calls and internal methods
    _SUCCESS = 0

    # Keep consistent with dagman_commands.h enumeration
    class Command(enum.IntEnum):
        """
        Internal mapping of specific generic payload DAGMan commands.

        :meta private:
        """

        HALT = 1
        RESUME = 2

    class Failure(enum.IntEnum):
        """
        Error Codes returned from lower level api calls and internal
        methods.

        :meta private:
        """

        #### Return values from _send_generic_payload
        OPEN_SOCKET = 1        # Failed to open command socker
        SEND_PAYLOAD = 2       # Failed to send request payload to DAGMan
        GET_RESULT = 3         # Failed to get return payload form DAGMan
        #### Internal method failure codes
        NO_ID = 4              # No DAG id set when calling __locate()
        CONNECT_SCHEDD = 5     # Failed to connect with Schedd to get DAGMan contact info
        GET_CONTACT = 6        # Failed to get DAGMan contact info (Schedd failure result ad)
        DENIED = 7             # Failed to get DAGMan contact info (Specifically not permitted)

    class ContactInfo():
        """
        DAGMan contact information. Sinful address to send command
        and secret DAGMan will use to verify trust.

        :meta private:
        """
        def __init__(self, addr: str, secret: str) -> None:
            self.addr = addr
            self.secret = secret


    def __init__(self, dag_id: int) -> None:
        """
        :param dag_id: The :ad-attr:`ClusterId` of a DAGMan job to
                       locate once the first command is issued.
        """
        self._contact = None

        if not isinstance(dag_id, int):
            raise TypeError("dag_id must be an integer")
        self._dag_id = dag_id


    @property
    def dag_id(self) -> int:
        """
        The :ad-attr:`ClusterId` of a DAGMan job this object will send commands.
        """
        return self._dag_id


    # Note: Pause is not a public option
    def halt(self, reason: Optional[str] = None, pause: Optional[bool] = None) -> Tuple[bool, str]:
        """halt(reason = None) -> tuple[bool, str]
        Inform DAGMan to halt a DAGs progress.

        :param reason: A message for why the DAG was halted to be
                       printed in the DAGs debug log.
        :return: Command success and result message.
        """
        request = classad.ClassAd({"DagCommand" : self.Command.HALT})

        if pause is not None:
            request["IsPause"] = pause

        if reason is not None:
            request["HaltReason"] = reason

        result, ret, err = self.__contact_dagman(self._CMD_DAGMAN_GENERIC, request)

        if ret != self._SUCCESS:
            return (False, err)

        if not result.get("Result", False):
            return (False, result.get('ErrorString', 'Unknown'))

        action = "Paused" if pause is not None and pause else "Halted"

        return (True, f"{action} DAG {self.dag_id}")


    def resume(self) -> Tuple[bool, str]:
        """
        Inform DAGMan to resume a hatled DAGs progress.

        :return: Command success and result message.
        """
        request = classad.ClassAd({"DagCommand" : self.Command.RESUME})

        result, ret, err = self.__contact_dagman(self._CMD_DAGMAN_GENERIC, request)

        if ret != self._SUCCESS:
            return (False, err)

        if not result.get("Result", False):
            return (False, result.get('ErrorString', 'Unknown'))

        return (True, f"Resumed DAG {self.dag_id}")


    def __contact_dagman(self, cmd: int, request: classad.ClassAd) -> Tuple[classad.ClassAd, int, Optional[str]]:
        """
        Attempt to send command with payload to DAGMan and recieve a result ClassAd.

        :param cmd: DAGMan command integer.
        :param request: The :class:`classad.ClassAd` containing the specific
                        commands information.

        :return: Result ClassAd, error code, and failure reason.

        :meta private:
        """

        # Get the DAGMan contact information if we don't already have it
        if self._contact is None:
            ret, err = self.__locate()
            if ret != self._SUCCESS:
                return (None, ret, err)

        # Add the contact secret to the request ad so DAGMan can verify us
        request["Secret"] = self._contact.secret

        # Attempt to send command to DAGMan
        try:
            result, ret, err = _send_generic_payload_command(self._contact.addr, int(cmd), request._handle)

            # Specific errors (initial connection and not trusted) may be a result of DAGMan going away
            if ret in [self.Failure.OPEN_SOCKET, self.Failure.SEND_PAYLOAD]:
                raise DagmanConnectionError(f"Connection to {self._contact.addr} ({self.dag_id}) failed: {err}")
            elif not result.get("Trusted", False):
                raise DagmanConnectionError(f"Connection to {self._contact.addr} ({self.dag_id}) failed: {result.get('ErrorString')}")
        except DagmanConnectionError as e:
            # Reset DAGMan contact information and retry command
            ret, err = self.__locate()
            if ret != self._SUCCESS:
                return (None, ret, err)

            result, ret, err = _send_generic_payload_command(self._contact.addr, int(cmd), request._handle)

        return (result, ret, err)


    def __locate(self) -> Tuple[int, str]:
        """
        Query the local Schedd for DAGMan contact information.

        :return: Error code and failure reason.

        :meta private:
        """

        # Reset contact information
        self._contact = None

        if self.dag_id is None:
            return (self.Failure.NO_ID, "No DAG ID specified")

        try:
            schedd = Schedd()
            ad = schedd._get_dag_contact_info(self.dag_id)
        except Exception as e:
            return (self.Failure.CONNECT_SCHEDD, f"Failed to get contact information: {str(e)}")

        if not ad.get("Result", False):
            ret = self.Failure.DENIED if result.get("Permitted", False) else self.Failure.GET_CONTACT
            return (ret, ad.get("ErrorString", "Unknown"))

        self._contact = DAGMan.ContactInfo(ad["Address"], ad["Secret"])
        return (self._SUCCESS, None)

