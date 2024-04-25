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

The following Python script uses the HTCondor python bindings to get the :ad-attr:`DAG_NodesTotal`
and :ad-attr:`DAG_NodesDone` from the workflows manager job from the local *condor_schedd* queue.

.. code-block:: python3
    :caption: Sample Python Script: dag_percent.py

    #!/usr/bin/env python3

    # Example Script Provided by:
    # https://htcondor.readthedocs.io/en/latest/auto-redirect.html?category=example&tag=bagman-percent-done

    import sys
    try:
        import htcondor
    except ImportError:
        error("Failed to import htcondor python bindings", 1)

    def error(msg: str, code: int):
        """Print error message and exit with specified exit code"""
        print(f"ERROR: {msg}", file=sys.stderr)
        exit(code)


    def parse_args():
        """Parse command line arguments"""
        if len(sys.argv) != 3:
            error("Missing argument(s) Job Id and/or Job return code", 1)

        # Parse this nodes Job ID
        try:
            ID = int(sys.argv[1].split(".")[0])
        except ValueError:
            error(f"Failed to convert Job Id ({sys.argv[1]}) to integer", 1)

        # Parse this nodes exit code to preserve node success/failure based on job exit
        try:
            CODE = int(sys.argv[2])
        except ValueError:
            error(f"Failed to convert Job exit code ({sys.argv[2]}) to integer", 1)

        return (ID, CODE)

    def get_job_ad(job_id: int, exit_code: int):
        """Query and return the parent DAGMan proper job ad"""
        DAG_ATTRS = ["DAG_NodesTotal", "DAG_NodesDone"]
        found = False

        schedd = htcondor.Schedd()

        # Get workflow Job ID from this a job history ad for this node
        for ad in schedd.history(f"ClusterId=={job_id}", ["DAGManJobId"], match=1):
            if "DAGManJobId" in ad:
                found = True
                DAG_ID = int(ad["DAGManJobId"])

        if not found:
            error(f"Failed to query job ad for cluster {job_id}", exit_code)

        # Query workflow information from DAGMan proper job
        ads = schedd.query(f"ClusterId=={DAG_ID}", DAG_ATTRS)
        if len(ads) != 1:
            error(f"Failed to query DAGMan job ad for ID:{DAG_ID}", exit_code)

        dag_ad = ads[0]

        for attr in DAG_ATTRS:
            if attr not in dag_ad:
                error(f"DAGMan job ad is missing '{attr}' attribute", exit_code)

        return dag_ad

    def main():
        # Threshold to exit if 75% of DAG is complete
        THRESHOLD = 0.75

        job_id, exit_code = parse_args()

        ad = get_job_ad(job_id, exit_code)

        # If this node job was successful then add 1 to count of completed nodes
        num_done = int(ad["DAG_NodesDone"]) + int(exit_code == 0)
        print(f"Nodes Successfully Completed: {num_done}")

        p_done = float(num_done / int(ad["DAG_NodesTotal"]))
        print(f"DAG: {p_done}%")

        # If threshold is passed then exit with specific code (124) to inform DAGMan to exit
        if p_done >= THRESHOLD:
            print(f"DAG is {p_done}% done!")
            sys.exit(124)

        sys.exit(exit_code)

    if __name__ == "__main__":
        main()

.. sidebar:: Make Your AP Admins Happy

    .. warning::

        Since the provided example script contacts the *condor_schedd* for information regarding
        the workflow, the number of POST Scripts running at a single time should be limited to
        prevent overloading the *condor_schedd*. This can be achieved by using :tool:`condor_submit_dag`\s
        ``-MaxPost`` flag.

        .. code-block:: console

            $ condor_submit_dag -MaxPost 5 analysis.dag

The above Python script can then be applied in the DAG file in coordination with
the :dag-cmd:`ABORT-DAG-ON` command to inform DAGMan to exit successfully when the
specified threshold is achieved as follows:

.. code-block:: condor-dagman
    :caption: Sample DAG: analysis.dag
    :emphasize-lines: 5,6

    # Bag of 1000 nodes
    # Example DAG Provided By:
    # https://htcondor.readthedocs.io/en/latest/auto-redirect.html?category=example&tag=bagman-percent-done

    SCRIPT POST ALL_NODES dag_percent.py $JOBID $RETURN
    ABORT-DAG-ON ALL_NODES 124 RETURN 0

    JOB Node_1 awesome-science.sub
    JOB Node_2 awesome-science.sub
    ...
    JOB Node_999 awesome-science.sub
    JOB Node_1000 awesome-science.sub

.. note::

    ``ALL_NODES`` will apply the POST Script and abort DAG semantics onto every node
    declared in the DAG.