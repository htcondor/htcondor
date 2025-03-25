from flask import Blueprint, render_template, request
import csv
import urllib
import urllib.parse
import urllib.request
import functools
import xml.etree.ElementTree as ET
from utils import cache_response_to_disk, make_data_response

#######################
# Functions to query Topology and cache responses in RAM
#######################

@functools.lru_cache(maxsize=1)
def _get_projects_tree() -> ET.Element:
    # may raise urllib.error.HTTPError

    handle = urllib.request.urlopen("https://topology.opensciencegrid.org/miscproject/xml")
    result_bytes = handle.read()

    return ET.fromstring(result_bytes.decode(encoding="utf-8", errors="surrogateescape"))


@functools.lru_cache(maxsize=2)
def _get_project_fos_hash(field: str):
    def safe_elem_text(elem: ET.Element) -> str:
        try:
            return elem.text.strip()
        except AttributeError:  # we got a None
            return ""

    project_hash = {}

    projects_tree = _get_projects_tree()
    for project in projects_tree.findall("./Project"):
        name = safe_elem_text(project.find("./Name"))
        if not name:
            continue  # malformed project?
        field_of_science = safe_elem_text(project.find("./" + field))
        project_hash[name] = field_of_science

    return project_hash


def organization_from_project(project_name: str):
    return _get_project_fos_hash("Organization").get(project_name, "Unknown").replace(',',' -')


def field_of_science_from_project(project_name: str):
    return _get_project_fos_hash("FieldOfScience").get(project_name, "Unknown").replace(',',' -')


#######################
# Functions to query Ganglia Web and format response for HTCondorView
#######################

@cache_response_to_disk()
def get_data_from_ganglia():
    # Grab parameters host (CE name), r (range: day, week, or month)
    host = request.args.get('host','chtc-spark-ce1.svc.opensciencegrid.org')
    r = request.args.get('r','day')

    # Grab data as a raw csv from ganglia, which was stashed there by the condor_gandliad
    import pandas as pd
    df=pd.read_csv('https://display.ospool.osg-htc.org/ganglia/graph.php?r=' + r + '&hreg[]=' + host + '&mreg[]=%5E' + 'Cpus' + '&mreg[]=%5E' + 'Memory' + '&mreg[]=%5E' + 'Disk' + '&mreg[]=%5E' + 'Bcus' + '&aggregate=1&csv=1',skipfooter=1,engine='python')

    # Transpose the data received from ganglia into the format we need for the CE Dashboard frontend

    # Rename 'Timestamp' column to 'Date' for clarity and set it as the index
    df.rename({'Timestamp':'Date'}, axis='columns', inplace=True)
    df.set_index('Date', inplace=True)
    # Split column names into multiple levels based on specific delimiters
    df.columns = df.columns.str.split(' |_Project |Use ', n=2, expand=True)
    # Define new names for the column levels
    new_names = ['Host', 'Resource', 'Project']
    df.columns.names = new_names
    # Reshape the DataFrame from wide to long format
    df = df.stack(new_names).rename('value').reset_index()
    # Drop the 'Host' column as it is no longer needed
    df.drop(columns=['Host'], inplace=True)
    # Strip whitespace from 'Project' column and handle empty project names
    df['Project'] = df['Project'].map(str.strip)
    df.loc[df['Project'] == '', 'Project'] = "____meta_" + df['Resource']
    # Simplify 'Resource' column by removing specific suffixes
    df['Resource'] = df['Resource'].str.replace('(In|NotIn)$', '', regex=True)
    # Pivot the DataFrame to create a schema with 'Date' and 'Project' as indices
    df = df.pivot(index=['Date', 'Project'], columns='Resource', values='value')
    # Fill missing values with zeros to handle incomplete data
    df.fillna(0, inplace=True)
    # Remove axis names and reset the index for a flat structure
    df = df.rename_axis(None, axis=1).reset_index()
    # Group by 'Date' and 'Project' to aggregate data
    df = df.groupby(['Date', 'Project'], as_index=False).sum()
    # Add a 'Type' column to distinguish between project usage (with P) and total resource (with R) entries
    df['Type'] = df['Project'].apply(lambda x: 'R' if x.startswith('____meta_') else 'P')
    # Replace meta entries with more descriptive names for better readability
    df['Project'] = df['Project'].str.replace('____meta_CpusIn', 'CpusAllocated')
    df['Project'] = df['Project'].str.replace('____meta_CpusNotIn', 'CpusUnallocated')
    df['Project'] = df['Project'].str.replace('____meta_MemoryIn', 'MemoryAllocated')
    df['Project'] = df['Project'].str.replace('____meta_MemoryNotIn', 'MemoryUnallocated')
    df['Project'] = df['Project'].str.replace('____meta_DiskIn', 'DiskAllocated')
    df['Project'] = df['Project'].str.replace('____meta_DiskNotIn', 'DiskUnallocated')
    df['Project'] = df['Project'].str.replace('____meta_BcusIn', 'BcusAllocated')
    df['Project'] = df['Project'].str.replace('____meta_BcusNotIn', 'BcusUnallocated')
    # Get rid of columns that are not needed; specifically, we don't want info per user, just per project   
    df.drop(columns=['Cpus_User'],inplace=True,errors='ignore')
    df.drop(columns=['Memory_User'],inplace=True,errors='ignore')
    # Insert Field and Organization columns, which we can figure out from the project name via Topology
    df['Field'] = df['Project'].apply(field_of_science_from_project)
    df['Organization'] = df['Project'].apply(organization_from_project)

    # Finally, serialize the data into a string in jsonp format as expected by the HTCondorView javascript

    mycsv = df.to_csv(index=False,quoting=csv.QUOTE_ALL).splitlines()
    index = 0
    response = "["
    while index < len(mycsv) - 1:
        response += "[" + mycsv[index] + "],\n"
        index += 1
    response += "[" + mycsv[index] + "]\n]"
    
    return response

#######################
# Flask routes
#######################

overview_bp = Blueprint('overview', __name__)

overview_linkmap = {
    'Overview': 'overview.html',
    'Help': {
        'Query Syntax': 'query_syntax.html',
        'Customization': 'customization.html'
    }
}

@overview_bp.route('/overview.html')
def overview():
    # host = request.args.get('host','chtc-spark-ce1.svc.opensciencegrid.org')
    # host = 'Univ of Wisconsin'
    from blueprints.landing import get_ce_facility_site_descrip
    facility, site, descrip = get_ce_facility_site_descrip(request.args.get('host','chtc-spark-ce1.svc.opensciencegrid.org'))
    return render_template('overview.html', page_title=facility, ce_facility_name=facility, ce_site_name=site,
                           ce_description=descrip, linkmap=overview_linkmap)

@overview_bp.route('/data/ce_overview')
def ce_overview_data():
    response_body, cached_response = get_data_from_ganglia()
    return make_data_response(response_body,cached_response)

@overview_bp.route('/error-no-data.html')
def error_no_data():
    from blueprints.landing import get_ce_facility_site_descrip
    facility, site, descrip = get_ce_facility_site_descrip(request.args.get('host','chtc-spark-ce1.svc.opensciencegrid.org'))
    msg = request.args.get('msg','No data available')
    return render_template('error_no_data.html', page_title=facility, ce_name=site, errMsg=msg, linkmap={})
