import os
import html
import re
import sys

from docutils import nodes, utils
from docutils.parsers.rst import Directive
from sphinx import addnodes
from sphinx.errors import SphinxError
from sphinx.util.nodes import split_explicit_title, process_index_entry, set_role_source_info
from htc_helpers import make_headerlink_node, warn

ATTRIBUTE_DEFS = {}

def make_anchor_title_node(attribute_name):
    html_parser = html.parser.HTMLParser()
    html_markup = f"""<code class="docutils literal notranslate"><span id="{attribute_name}" class="pre">{html.escape(attribute_name)}</span></code>"""
    node = nodes.raw("", html_markup, format="html")
    return node


def classad_attribute_def_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    global ATTRIBUTE_DEFS
    app = inliner.document.settings.env.app
    docname = inliner.document.settings.env.docname

    # Create the attribute title with the classad attribute name, also containing an anchor
    anchor_title_node = make_anchor_title_node(text)

    # Create a headerlink node, which can be used to link to the anchor
    headerlink_node = make_headerlink_node(str(text), options)

    # Determine the classad type (job, submitted, collector, etc.) by ripping it out of the document title
    attr_type = ""
    type_matches = re.findall(r"/([-\w]*)-classad-attributes", docname)
    for match in type_matches:
        attr_type = match.capitalize() + " "

    if text in ATTRIBUTE_DEFS.get(attr_type, []):
        warn(f"{docname} @ {lineno} | {attr_type} ClassAd attribute '{text}' already defined")
        textnode = nodes.Text(text, " ")
        return [textnode], []
    elif attr_type in ATTRIBUTE_DEFS:
        ATTRIBUTE_DEFS[attr_type].append(text)
    else:
        ATTRIBUTE_DEFS.update({attr_type : [text]})

    # Automatically include an index entry for this attribute
    index_node = addnodes.index()
    index_node['entries'] = process_index_entry(f"pair: {text} ; {attr_type} ClassAd Attribute", text)
    set_role_source_info(inliner, lineno, index_node)

    return [index_node, anchor_title_node, headerlink_node], []


def setup(app):
    app.add_role('classad-attribute-def', classad_attribute_def_role)

