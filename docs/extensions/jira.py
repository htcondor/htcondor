import os
import sys

from docutils import nodes
from docutils.parsers.rst import Directive

from htc_helpers import make_headerlink_node, warn

TICKETS_ANCHORED = set()

def make_link_node(rawtext, app, type, slug, options):
    """Create a link to a JIRA ticket.

    :param rawtext: Text being replaced with link node.
    :param app: Sphinx application context
    :param type: Link type (issue, changeset, etc.)
    :param slug: ID of the ticket to link to
    :param options: Options dictionary passed to role func.
    """
    base = "https://opensciencegrid.atlassian.net/browse/HTCONDOR-"
    ref = base + slug
    # set_classes(options)
    node = nodes.reference(rawtext, "(HTCONDOR-" + slug + ")", refuri=ref, **options)
    return node

def make_version_anchor(inliner, options, tag):
    global TICKETS_ANCHORED
    tag = f"{tag}".replace(" ", "-").replace(",", "-")

    if tag in TICKETS_ANCHORED:
        entry_no = inliner.document.settings.env.new_serialno(tag)
        tag += f"-{entry_no}"
    else:
        TICKETS_ANCHORED.add(tag)

    parent = inliner.parent
    if "ids" in parent:
        parent["ids"].append(tag)
    else:
        parent["ids"] = [tag]

    return make_headerlink_node(tag, options)

def vers_hist_anchor_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    tag = "version-history" if text.lower() == "default" else text
    return [make_version_anchor(inliner, options, tag)], []

def ticket_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    nodes = list()
    for ticket_id_str in text.split(","):
        try:
            ticket_id = int(ticket_id_str)
            app = inliner.document.settings.env.app
            node = make_link_node(rawtext, app, 'issue', str(ticket_id), options)
            nodes.append(node)
        except ValueError as e:
            docname = inliner.document.settings.env.docname
            warn(f"{docname}:{lineno} | Failed to link ticket #{ticket_id_str}: {e}")

    if len(nodes) > 0:
        nodes.append(make_version_anchor(inliner, options, text))

    return nodes, []

def setup(app):
    app.add_role("jira", ticket_role)
    app.add_role("hist-anchor", vers_hist_anchor_role)

