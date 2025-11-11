import os
import sys

from docutils import nodes
from docutils.parsers.rst import Directive
from sphinx import addnodes
from sphinx.errors import SphinxError
from sphinx.util.nodes import split_explicit_title, process_index_entry, set_role_source_info
from htc_helpers import *

CLASSAD_FUNCTIONS = []

def find_classad_func_cmds(dir: str):
    classad_functions = []
    classad_ref = os.path.join(dir, "classads", "classad-builtin-functions.rst")
    with open(classad_ref, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            for cmd in get_all_defined_role("classad-function-def", line):
                if "(" not in cmd or " " not in cmd[:cmd.find("(")]:
                    continue
                end = cmd.find("(")
                begin = cmd[:end].find(" ") + 1
                function = cmd[begin : end]
                if function not in classad_functions:
                    classad_functions.append(function)
    classad_functions.sort()
    return classad_functions

def dump(obj):
    for attr in dir(obj):
        print("obj.%s = %r" % (attr, getattr(obj, attr)))

def classad_function_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    root_dir = root_dir = get_rel_path_to_root_dir(inliner)[:-1]
    function, index = custom_ext_parser(text)

    # Allow :classad-function:`foo` and :classad-function:`foo()`
    if "(" in function:
        function = function[:function.find("(")]

    if function not in CLASSAD_FUNCTIONS:
        docname = inliner.document.settings.env.docname
        warn(f"{docname}:{lineno} | '{function}' ClassAd function not in defined list. Either a typo or not defined.")

    ref_link = f"href=\"{root_dir}/classads/classad-builtin-functions.html#" + str(function) + "()\""
    return make_ref_and_index_nodes(name, function, index, ref_link,
                                    rawtext, inliner, lineno, options, f"{function}()")

def setup(app):
    global CLASSAD_FUNCTIONS
    CLASSAD_FUNCTIONS = find_classad_func_cmds(app.srcdir)
    app.add_role("classad-function", classad_function_role)

