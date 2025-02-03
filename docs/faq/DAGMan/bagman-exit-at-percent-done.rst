Example: DAGMan Completes When Some Percentage of Nodes Succeed
===============================================================

.. sidebar:: Definition: Bag of Nodes

    A bag of nodes is a DAG workflow comprising of nodes bearing
    no :dag-cmd:`PARENT/CHILD` relationships. That is a loose bag
    of nodes without any inter-dependencies.

Sometimes it is desired to have a DAG set up as a bag of nodes that completes
after a certain percentage of successfully executed nodes. The following
example adds a POST :dag-cmd:`SCRIPT` to all nodes in the DAG to calculate the
percent of nodes that finished successfully that informs the DAG to exit when
the specified threshold is achieved.

The following Python script takes relevant information provided by DAGMan at
execution time to calculate the percent of successfully completed nodes in a DAG.

.. code-block:: python3
    :caption: Sample Python Script: dag_percent.py

    #!/usr/bin/env python3

    import sys

    def error(msg: str, code: int):
        """Print error message and exit with specified exit code"""
        print(f"ERROR: {msg}", file=sys.stderr)
        exit(code)


    def parse_args():
        """Parse command line arguments"""
        if len(sys.argv) != 4:
            error("Missing argument(s) DAGMan node counts", 1)

        try:
            TOTAL = int(sys.argv[1])
        except ValueError:
            error(f"Failed to convert total node count ({sys.argv[1]}) to integer", 1)

        try:
            DONE = int(sys.argv[2])
        except ValueError:
            error(f"Failed to convert done node count ({sys.argv[2]}) to integer", 1)

        try:
            CODE = int(sys.argv[3])
        except ValueError:
            error(f"Failed to convert job exit code ({sys.argv[3]}) to integer", 1)

        return (TOTAL, DONE, CODE)

    def main():
        THRESHOLD = 0.75

        total_nodes, num_done, exit_code = parse_args()

        # Include this node in done count if job exit code is 0
        num_done = num_done + int(exit_code == 0)
        print(f"Nodes Successfully Completed: {num_done}")

        p_done = float(num_done / total_nodes)
        print(f"DAG: {p_done}%")

        if p_done >= THRESHOLD:
            print(f"DAG is {p_done}% done!")
            sys.exit(124)

        sys.exit(exit_code)

    if __name__ == "__main__":
        main()


.. sidebar:: Node Counts Precision

    Since DAGMan provides current node counts at script execution time, multiple
    Post Scripts ran concurrently will do the same or similar percent complete
    calculations. To help increase the precision of the calculation you can limit
    the number of Post Scripts executed at once via the ``-MaxPost`` flag for
    :tool:`condor_submit_dag`.

    .. code-block:: console

        $ condor_submit_dag -MaxPost 1 analysis.dag

The above Python script can then be applied in the DAG file in coordination with
the :dag-cmd:`ABORT-DAG-ON` command to inform DAGMan to exit successfully when the
specified threshold is achieved as follows:

.. code-block:: condor-dagman
    :caption: Sample DAG: analysis.dag
    :emphasize-lines: 5,6

    # Bag of 1000 nodes
    # Example DAG Provided By:
    # https://htcondor.readthedocs.io/en/latest/auto-redirect.html?category=example&tag=bagman-percent-done

    SCRIPT POST ALL_NODES dag_percent.py $NODE_COUNT $DONE_COUNT $RETURN
    ABORT-DAG-ON ALL_NODES 124 RETURN 0

    JOB Node_1 awesome-science.sub
    JOB Node_2 awesome-science.sub
    ...
    JOB Node_999 awesome-science.sub
    JOB Node_1000 awesome-science.sub

.. note::

    ``ALL_NODES`` will apply the POST Script and abort DAG semantics onto every node
    declared in the DAG.