import os
import re
import pathlib

from docutils.parsers.rst import Directive
from docutils.parsers.rst import directives
from docutils.statemachine import ViewList
from sphinx.util.nodes import nested_parse_with_titles
from docutils import nodes

# Nice variable references for parts of
# a version tuple (MAJOR, MINOR, RELEASE) ex: (24.3.1)
MAJOR = 0
MINOR = 1
RELEASE = 2

# Supported history entry types
HISTORY_TYPES = ["bug", "bugs", "feature", "features"]

class FlattenVersionHistory(Directive):
    """Custom directive to read mega version history files and flatten into one list"""

    # Directive setup
    require_arguments = 1 # Requires an entry type (see HISTORY_TYPES)
    optional_arguments = 10 # Up to 10 versions to parse from
    has_content = True # Allow additional content below decalration
    # Extra options
    option_spec = {
        # List of ticket numbers to exclude in this inclusion
        "exclude" : directives.positive_int_list,
    }

    def _exluded_entry(self, entry: list[tuple[str,int]]) -> bool:
        """Check if parsed version history entry is attributed to excluded tickets"""
        # Check if we are even excluding any tickets
        exclude_list = self.options.get("exclude")
        if exclude_list is None or len(exclude_list) == 0:
            return False

        # Resolve entry into a single string
        entry_str = "\n".join([pair[0] for pair in entry])
        # Count the number of specified jira tickets
        num_tickets = entry_str.count(":jira:")

        # Check each specified exclude ticket number for existance in entry
        match_count = 0
        for ticket in set(exclude_list):
            check = f":jira:`{ticket}`"
            if check in entry_str:
                match_count += 1
            if match_count == num_tickets:
                break

        return match_count == num_tickets

    def run(self) -> list[nodes.Node]:
        """Execute this directive"""
        # Used to parse read version history entries into node
        rst = ViewList()
        # Make paragraph node (end result)
        node = nodes.paragraph()

        # Add any additional contents to the node
        rst.extend(self.content)

        # Get absolute path to version-history directory (where history entry files should live)
        vers_hist_dir = pathlib.Path(self.state.document.settings.env.srcdir) / "version-history"

        # Check if required entry type is valid
        ref_type = self.arguments[0].lower()
        if ref_type not in HISTORY_TYPES:
            raise self.error(f"{self.name}: Invalid version history type '{ref_type}'")
        # Check if there have been specified versions to include/flatten
        elif len(self.arguments) > 1:
            # Dictionary of specified versions {MAJOR : set(full versions)}
            specified_versions = dict()

            # Check each argument is valid and parse into specified versions dict for later
            for arg in self.arguments[1:]:
                if re.match(r"(\d+)\.(\d+)\.(\d+)", arg):
                    version = tuple(int(x) for x in arg.split("."))
                    key = version[MAJOR]
                    if key in specified_versions:
                        specified_versions[key].add(version)
                    else:
                        specified_versions[key] = set([version])
                else:
                    raise self.error(f"{self.name}: Invalid version number ({arg}) provided")

            # Reach each associated major version history entry file (v24-version.hist)
            for major, versions in specified_versions.items():
                version_file = vers_hist_dir / f"v{major}-version.hist"
                # Ignore versions if entry file DNE
                if version_file.exists():
                    # !!!! IMPORTANT !!!! - This informs sphinx to rebuild version histories when entry files are updated
                    self.state.document.settings.env.note_dependency(str(version_file))
                    # Read version history entry file
                    with version_file.open() as f:
                        included_entries = [] # Entries to actually include
                        entry = [] # Current entry being parsed (for exclude checking)
                        found_section = False
                        line_no = 0

                        for line in f:
                            if line.strip() == "" or line.strip()[0] == "#" or line.strip()[:2] == "//":
                                # Ignore blank lines & allow '#' and '//' to denote comments
                                pass

                            elif line.strip()[:3] == "***":
                                # Parse section banner line (*** version type)
                                # Everything below this line belongs to this version type

                                # Break if a new section but we are no longer including from this file
                                if len(versions) == 0:
                                    break

                                found_section = False

                                # Attempt to read section banner (*** version type)
                                try:
                                    _, vers, section_type = line.strip().split()[:3]
                                    section_version = tuple(int(x) for x in vers.split("."))

                                    # Make sure specified type is valid
                                    if section_type.lower() not in HISTORY_TYPES:
                                        raise RuntimeError(f"Unknown version history type '{section_type}'")

                                    # Check that directive type matches section type
                                    match_type = (
                                        (ref_type in HISTORY_TYPES[:2] and section_type in HISTORY_TYPES[:2])
                                        or
                                        (ref_type in HISTORY_TYPES[2:] and section_type in HISTORY_TYPES[2:])
                                    )

                                    # Check if section version was specified and type matches (remove from needed versions)
                                    found_section = (section_version in versions and match_type)
                                    if found_section:
                                        versions.remove(section_version)

                                except Exception as e:
                                    raise self.error(f"{self.name}: Invalid version history file '{version_file}': {e}")

                            elif found_section:
                                # Store section lines for later parsing
                                if len(line) > 1 and line[0] == '-':
                                    # We expect all entries to begin w/ '-' with no padding
                                    if len(entry) > 0:
                                        # Already have an entry stored... add to final list if not excluded and clear
                                        if not self._exluded_entry(entry):
                                            included_entries.extend(entry)
                                        entry.clear()
                                # Store line in temporary entry
                                entry.append((line, line_no))
                            line_no += 1

                        # Make sure to include any final entry information if not excluded
                        if len(entry) > 0 and not self._exluded_entry(entry):
                            included_entries.extend(entry)

                        # Add all non-excluded entries to rst parser object
                        for line, num in included_entries:
                            rst.append(line, str(version_file), num)

        # If no information has been parsed (from files or inline content) then default to '- None'
        if len(rst) == 0:
            rst.append("- None.", "Internal-Parse", self.lineno)

        # Parse list into paragraph node
        nested_parse_with_titles(self.state, rst, node)

        return [node]

def setup(app):
    """Setup sphinx to know of this directive"""
    app.add_directive('include-history',  FlattenVersionHistory)
