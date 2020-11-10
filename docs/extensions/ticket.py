import os
import sys

from docutils import nodes
from docutils.parsers.rst import Directive

"""
Example of a Sphinx Directive.
This is useful for for working with large blocks of text. Leaving here as a reference.
Roles are more useful for inline text, which is what we need for tickets.

class Ticket(Directive):
    required_arguments = 1
    def run(self):
        ticket_url = "<a href=\"https://htcondor-wiki.cs.wisc.edu/index.cgi/tktview?tn=" + str(self.arguments[0]) + "\" target=\"blank\">link text</a>"
        paragraph_node = nodes.paragraph(text=ticket_url)
        return [paragraph_node]

def setup(app):
    app.add_directive("ticket", Ticket)

"""

# Custom role reference: https://doughellmann.com/blog/2010/05/09/defining-custom-roles-in-sphinx/

def make_link_node(rawtext, app, type, slug, options):
    """Create a link to an HTCondor ticket.

    :param rawtext: Text being replaced with link node.
    :param app: Sphinx application context
    :param type: Link type (issue, changeset, etc.)
    :param slug: ID of the ticket to link to
    :param options: Options dictionary passed to role func.
    """
    base = "https://htcondor-wiki.cs.wisc.edu/index.cgi/tktview?tn="
    ref = base + slug
    # set_classes(options)
    node = nodes.reference(rawtext, "(Ticket #" + slug + ")", refuri=ref, **options)
    return node

def ticket_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    try:
        ticket_id = int(text)
        if ticket_id <= 1000:
            raise ValueError
    except ValueError:
        msg = inliner.reporter.error(
            'HTCondor ticket number must be a number greater than or equal to 1000; '
            '"%s" is invalid.' % text, line=lineno)
        prb = inliner.problematic(rawtext, rawtext, msg)
        return [prb], [msg]
    app = inliner.document.settings.env.app
    node = make_link_node(rawtext, app, 'issue', str(ticket_id), options)
    return [node], []

def setup(app):
    app.add_role("ticket", ticket_role)

