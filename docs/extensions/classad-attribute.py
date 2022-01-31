import os
import sys

from docutils import nodes, utils
from docutils.parsers.rst import Directive
from sphinx import addnodes
from sphinx.errors import SphinxError
from sphinx.util.nodes import split_explicit_title, process_index_entry, set_role_source_info


def make_anchor_title_node(attribute_name):
    html_markup = '<code class="docutils literal notranslate"><span id="' + attribute_name + '" class="pre">' + attribute_name + '</span></code>'
    node = nodes.raw("", html_markup, format="html")
    return node


def make_headerlink_node(attribute_name, options):
    ref = '#' + attribute_name
    node = nodes.reference('', '¶', refuri=ref, external=False, classes=['headerlink'], title='Permalink to this headline', **options)
    return node


def classad_attribute_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    app = inliner.document.settings.env.app
    docname = inliner.document.settings.env.docname

    # Create the attribute title with the classad attribute name, also containing an anchor
    anchor_title_node = make_anchor_title_node(text)

    # Create a headerlink node, which can be used to link to the anchor
    headerlink_node = make_headerlink_node(str(text), options)
        
    # Automatically include an index entry
    entries = process_index_entry(text + " (ClassAd Attribute)", text)
    index_node = addnodes.index()
    index_node['entries'] = entries
    set_role_source_info(inliner, lineno, index_node)

    return [index_node, anchor_title_node, headerlink_node], []


def setup(app):
    app.add_role('classad-attribute', classad_attribute_role)

