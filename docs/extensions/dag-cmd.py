import os
from htc_helpers import *

DAG_CMDS = []
DAG_CMDS_CI = {}

def find_dag_cmds(dir: str):
    dag_cmds = []
    dag_ref = os.path.join(dir, "automated-workflows", "dagman-reference.rst")
    with open(dag_ref, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            for cmd in get_all_defined_role("dag-cmd-def", line):
                if cmd not in dag_cmds:
                    dag_cmds.append(cmd)
    dag_cmds.sort()
    return dag_cmds

def dump(obj):
    for attr in dir(obj):
        print("obj.%s = %r" % (attr, getattr(obj, attr)))

def dagcom_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    root_dir = root_dir = get_rel_path_to_root_dir(inliner)[:-1]
    cmd_name, cmd_index = custom_ext_parser(text)
    # Case-insensitive lookup: exact match first, then fall back to the
    # canonical (as-defined) case so the anchor we link to actually exists.
    canonical = cmd_name if cmd_name in DAG_CMDS else DAG_CMDS_CI.get(cmd_name.lower())
    if canonical is None:
        docname = inliner.document.settings.env.docname
        warn(f"{docname}:{lineno} | '{cmd_name}' DAG command not in defined list. Either a typo or not defined.")
        canonical = cmd_name
    ref_link = f"href=\"{root_dir}/automated-workflows/dagman-reference.html#" + str(canonical) + "\""
    return make_ref_and_index_nodes(name, cmd_name, cmd_index,
                                    ref_link, rawtext, inliner, lineno, options)

def setup(app):
    global DAG_CMDS
    global DAG_CMDS_CI
    DAG_CMDS = find_dag_cmds(app.srcdir)
    DAG_CMDS_CI = build_ci_index(DAG_CMDS, "DAG command")
    app.add_role("dag-cmd", dagcom_role)

