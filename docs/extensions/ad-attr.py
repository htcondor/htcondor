import os
from htc_helpers import *

# Global table of documented ClassAd attributes (:classad-attribute-def:)
ATTRIBUTE_FILES = {}
# Table of Specified ClassAd attr type to file
AD_TYPE_FILES = {
    "ACCOUNTING" : "accounting-classad-attributes.html",
    "ADDED_COLLECTOR" : "classad-attributes-added-by-collector.html",
    "COLLECTOR" : "collector-classad-attributes.html",
    "DAEMONCORE" : "daemon-core-statistics-attributes.html",
    "DEFRAG" : "defrag-classad-attributes.html",
    "GRID" : "grid-classad-attributes.html",
    "JOB" : "job-classad-attributes.html",
    "MACHINE" : "machine-classad-attributes.html",
    "MASTER" : "daemon-master-classad-attributes.html",
    "NEGOTIATOR" : "negotiator-classad-attributes.html",
    "SCHEDD" : "scheduler-classad-attributes.html",
    "SUBMITTER" : "submitter-classad-attributes.html",
}

def map_attrs(dir: str):
    """Read ClassAd attribute definition files for documented attributes"""
    attrs = {}
    files_dir = os.path.join(dir, "classad-attributes")
    for ad_file in os.listdir(files_dir):
        if ad_file[-4:] != ".rst":
            continue
        path = os.path.join(files_dir, ad_file)
        with open(path, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                for attr in get_all_defined_role("classad-attribute-def", line):
                    html_file = ad_file.replace(".rst", ".html")
                    if attr in attrs:
                        attrs[attr].append(html_file)
                    else:
                        attrs.update({attr : [html_file]})
    return attrs

def descope_classad(attr: str) -> str:
    """Removed ClassAd attribute scope from attribute"""
    if attr.lower().startswith("my."):
        attr = attr[3:]
    elif attr.lower().startswith("target."):
        attr = attr[7:]
    return attr

def classad_attr_role(name, rawtext, text, lineno, inliner, options={}, content=[]):
    """Parse inline ad-attr role"""
    docname = inliner.document.settings.env.docname
    root_dir = get_rel_path_to_root_dir(inliner)[:-1]
    full_name, attr_info = custom_ext_parser(text)
    details = extra_info_parser(attr_info) if attr_info != "" else None

    attr_name = descope_classad(full_name)
    ad_type = details.get("TYPE", "").upper() if details is not None else None
    attr_index = details.get("INDEX", "") if details is not None else ""
    filename = AD_TYPE_FILES.get(ad_type) if ad_type in AD_TYPE_FILES else None

    if attr_name not in ATTRIBUTE_FILES:
        warn(f"{docname}:{lineno} | ClassAd Attribute '{attr_name}' not defined in any ClassAd Documentation files")
        filename = "classad-types.html"
    elif filename is None:
        filename = ATTRIBUTE_FILES[attr_name][0]
        if len(ATTRIBUTE_FILES[attr_name]) > 1:
            warn(f"{docname}:{lineno} | ClassAd Attribute '{attr_name}' is defined in multiple files. Defaulting to {filename}")

    ref_link = f"href=\"{root_dir}/classad-attributes/{filename}#{attr_name}\""
    return make_ref_and_index_nodes(name, full_name, attr_index, ref_link,
                                    rawtext, inliner, lineno, options, attr_name)

def setup(app):
    """Setup ad-attr role"""
    global ATTRIBUTE_FILES
    # Create mapping of ClassAd attribute to source file
    ATTRIBUTE_FILES = map_attrs(app.srcdir)
    app.add_role("ad-attr", classad_attr_role)

