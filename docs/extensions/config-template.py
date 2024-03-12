import os
import html
import sys

from docutils import nodes, utils
from docutils.parsers.rst import Directive
from sphinx import addnodes
from sphinx.errors import SphinxError
from sphinx.util.nodes import split_explicit_title, process_index_entry, set_role_source_info
from htc_helpers import *

CATEGORIES = []

def dump(obj):
    for attr in dir(obj):
        print("obj.%s = %r" % (attr, getattr(obj, attr)))

def conf_temp_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    global CATEGORIES
    # Create a new linkable target using the config template
    item, info = custom_ext_parser(text, "<", ">")
    info = info.upper()

    # The category sections don't expect any extra information i.e. POLICY, FEATURE, etc
    if info == "":
        targetid = item.upper()
        index_cat = "Configuration Templates"
        if item.upper() in CATEGORIES:
            warn(f"Config template category '{item}' defined multiple times.")
        else:
            CATEGORIES.append(item.upper())
        node = nodes.target('', item, ids=[targetid], classes=['config-template'])
    # Handle specific template (FEATURE:GPUs)
    else:
        if info not in CATEGORIES:
            warn(f"Provided category '{info}' for config template '{item}' is not defined.")
        # Find optional function parts| FOO:Bar(ignore this stuff)
        display_text = item
        item = item[:item.find("(")] if "(" in item else item
        targetid = info + ":" + item.upper()
        index_cat = f"{info} Configuration Templates"
        html_parser = html.parser.HTMLParser()
        html_markup = f"""<code class="docutils literal notranslate"><span id="{targetid}" class="pre">{html.escape(display_text)}</span></code>"""
        node = nodes.raw("", html_markup, format="html")

    # Automatically include an index entry for config templates
    indexnode = addnodes.index()
    indexnode['entries'] = process_index_entry(f"pair: {item}; {index_cat}", targetid)
    set_role_source_info(inliner, lineno, indexnode)

    return [indexnode, node], []


def setup(app):
    app.add_role("config-template", conf_temp_role)

