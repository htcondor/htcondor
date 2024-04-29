################################################
# htc_helpers.py
# This file contains functionality that is shared
# between multiple custom sphinx roles (extensions).
#
# Written By: Cole Bollig
# Date: 2023-10-27
################################################

import os
import sys

from docutils import nodes
from docutils.parsers.rst import Directive
from sphinx import addnodes
from sphinx.errors import SphinxError
from sphinx.util.nodes import split_explicit_title, process_index_entry, set_role_source_info
# Remove exception handling once Centos7 support is dropped (Requires Sphinx V2.0.0)
try:
    from sphinx.util import logging
except ImportError:
    logging = None

def warn(msg: str):
    """Output a warning message during sphinx build"""
    logger = logging.getLogger(__name__) if logging is not None else None
    if logger is not None:
        logger.warning(msg)

def get_all_defined_role(role: str, line: str):
    """Find all definition roles in a single line"""
    # :foo:`bar` :foo:`baz` :foo:`bat` -> bar, baz, bat
    while role in line:
        begin = line.find("`") + 1
        end = line.find("`", begin)
        cmd = line[begin:end]
        yield cmd
        line = line[end+1:]

def get_rel_path_to_root_dir(inliner):
    """Return the ../ sequence to get to root directory"""
    env = inliner.document.settings.env
    doc_path = str(env.doc2path(env.docname)).replace(str(env.srcdir)+"/", "")
    return "../" * doc_path.count("/")
    #return "../" * env.doc2path(env.docname, False).count("/")

def make_headerlink_node(attribute_name, options):
    """Create a headerlink associated with the paragraph symbol"""
    ref = '#' + attribute_name
    node = nodes.reference('', '¶', refuri=ref, reftitle="Permalink to this headline", classes=['headerlink'], **options)
    return node

def extra_info_parser(details, delim=";"):
    """Return a dictionary of information from delimited key=value pairs"""
    info = {}
    for detail in details.split(delim):
        key, value = tuple(detail.strip().split("="))
        info.update({
            key.upper().strip() : value.strip(' "\'')
        })
    return info

def custom_ext_parser(text, info_start="[", info_end="]"):
    """Return the provided text and contained extra information"""
    # Attempt to find detail section in text: INFO[DETAILS]
    index_start = text.find(info_start)
    index_end = text.find(info_end)
    # If no detail section or incomplete/empty section return no index
    if index_start == -1 or index_end == -1 or index_start+1 == index_end:
        return text, ""
    else:
        return text[:index_start], text[index_start+1:index_end]

def make_ref_and_index_nodes(html_class, name, index, ref_link, rawtext, inliner, lineno, options):
    """Automatically handle reference creation and indexing of reference role"""
    # Building Manpages create normal reference node to return
    if os.environ.get('MANPAGES') == 'True':
        node = nodes.reference(rawtext, name, refuri=ref_link, **options)
        return [node], []

    # Building documentation replace '<>' with html representation
    name_html = name.replace("<", "&lt;").replace(">", "&gt;")

    # No Index Specified: Create raw node to reference provided url
    if index == "":
        node = nodes.raw("", f"<a class=\"{html_class}\" " + str(ref_link) + ">" + str(name_html) + "</a>", format="html")
        return [node], []

    # Create target id as 'name-index-#' so when index references the extention call in a page it goes to that section
    targetid = '%s-%s-%s' % (str(name), str(index).replace(" ", "-"), inliner.document.settings.env.new_serialno('index'))
    # Create raw node to reference provided url with specified id for index to target
    node = nodes.raw("", "<a id=\"" + str(targetid) + f"\" class=\"{html_class}\" " + str(ref_link) + ">" + str(name_html) + "</a>", format="html")

    # Create index using the text name and desired index text
    entries = process_index_entry('single: ' + name + '; ' + index, targetid)
    indexnode = addnodes.index()
    indexnode['entries'] = entries
    set_role_source_info(inliner, lineno, indexnode)

    return [indexnode, node], []

