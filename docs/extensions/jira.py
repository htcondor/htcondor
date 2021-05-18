import os
import sys

from docutils import nodes
from docutils.parsers.rst import Directive

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

def ticket_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    try:
        ticket_id = int(text)
        if ticket_id > 1000:
            raise ValueError
    except ValueError:
        msg = inliner.reporter.error(
            'HTCondor ticket number must be a number less than or equal to 1000; '
            '"%s" is invalid.' % text, line=lineno)
        prb = inliner.problematic(rawtext, rawtext, msg)
        return [prb], [msg]
    app = inliner.document.settings.env.app
    node = make_link_node(rawtext, app, 'issue', str(ticket_id), options)
    return [node], []

def setup(app):
    app.add_role("jira", ticket_role)

