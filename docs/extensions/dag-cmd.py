import os
from htc_helpers import *

DAG_CMDS = []

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
    if cmd_name not in DAG_CMDS:
        docname = inliner.document.settings.env.docname
        warn(f"{docname} @ {lineno} | '{cmd_name}' DAG command not in defined list. Either a typo or not defined.")
    ref_link = f"href=\"{root_dir}/automated-workflows/dagman-reference.html#" + str(cmd_name) + "\""
    return make_ref_and_index_nodes(name, cmd_name, cmd_index,
                                    ref_link, rawtext, inliner, lineno, options)

def setup(app):
    global DAG_CMDS
    DAG_CMDS = find_dag_cmds(app.srcdir)
    app.add_role("dag-cmd", dagcom_role)

