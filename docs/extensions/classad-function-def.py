import os
import sys

from docutils import nodes, utils
from docutils.parsers.rst import Directive
from sphinx import addnodes
from sphinx.errors import SphinxError
from sphinx.util.nodes import split_explicit_title, process_index_entry, set_role_source_info
from htc_helpers import *

FUNCTION_DEF_COUNT = {}

def dump(obj):
    for attr in dir(obj):
        print("obj.%s = %r" % (attr, getattr(obj, attr)))

def classad_func_def_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    global FUNCTION_DEF_COUNT
    docname = inliner.document.settings.env.docname

    function, info = custom_ext_parser(text, "<", ">")

    if ")" not in function or "(" not in function or " " not in function[:function.find("(")]:
        # Expect 'ReturnType FunctionName([args])'
        warn(f"{docname}:{lineno} | Invalid ClassAd Function '{function}' expects 'Return function(...)'")
        return [make_inline_literal_node(text)], []

    fn_end = function.find("(")
    fn_begin = function[:fn_end].find(" ") + 1
    func_name = function[fn_begin : fn_end]

    if func_name in FUNCTION_DEF_COUNT:
        FUNCTION_DEF_COUNT[func_name] += 1
        span_id = f"{func_name}({FUNCTION_DEF_COUNT[func_name]})"
        headerlink_node = make_headerlink_node(span_id, options)
    else:
        span_id = f"{func_name}()"
        headerlink_node = make_headerlink_node(span_id, options)
        FUNCTION_DEF_COUNT[func_name] = 0

    inline_node = make_inline_literal_node(function, span_id)

    index_node = addnodes.index()
    index_node['entries'] = process_index_entry(f"pair: {func_name}() ; ClassAd Functions", f"{func_name}()")
    set_role_source_info(inliner, lineno, index_node)

    return [index_node, inline_node, headerlink_node], []

def setup(app):
    app.add_role("classad-function-def", classad_func_def_role)

