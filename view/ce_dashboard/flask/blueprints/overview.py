from flask import Blueprint, render_template, request, current_app
import csv
import urllib.request
import functools
import xml.etree.ElementTree as ET
from utils import cache_response_to_disk, make_data_response, getOrganizationFromInstitutionID
import time

#######################
# Functions to query Topology and cache responses
#######################

# Query the Topology database for project info, storing the result in a local file for 1 hour
# We store it in a local file so that we can share the response between multiple wsgi server
# processes, and to avoid hitting the topology for each process.
@cache_response_to_disk(file_name="topology_projects.bin", seconds_to_cache=60*60)
def _get_topology_projects_xml():
    # may raise urllib.error.HTTPError

    handle = urllib.request.urlopen("https://topology.opensciencegrid.org/miscproject/xml")
    result_bytes = handle.read()
    return result_bytes

# Parse the XML response from the Topology database and cache the result
# for 1 hour. How does the caching for one hour work? The function is
# decorated with @functools.lru_cache(maxsize=1), which caches the result
# of the function call. The cache is invalidated after one hour by
# using the hour_counter parameter. The function is called with the current
# hour as the parameter, and the result is cached for that hour. When the
# hour changes, the cache is invalidated and a new result is generated.
@functools.lru_cache(maxsize=1)
def _get_projects_tree(hour_counter: int) -> ET.Element:
    hour_counter += 0  # just to avoid warning about unused variable
    result_bytes, _ = _get_topology_projects_xml()
    # Decode the bytes to a string using surrogateescape error handling
    result_string = result_bytes.decode(
        encoding="utf-8", errors="surrogateescape"
    )
    # Parse the XML response and return the root element
    return ET.fromstring(result_string)

# Cache the hash for one hours using the same hour_counter trick as above.
# Cache is maxsize 3, so that we can cache for field = "FieldOfScience"
# and field = "Organization" and field = "InstitutionID".
# The cache is invalidated after one hour by using the hour_counter parameter.
# The function is called with the current hour as the parameter, and the result
# is cached for that hour. When the hour changes, the cache is invalidated and a new result is generated.
@functools.lru_cache(maxsize=3)
def _get_project_fos_hash(hour_counter: int, field: str):
    def safe_elem_text(elem: ET.Element) -> str:
        try:
            return elem.text.strip()
        except AttributeError:  # we got a None
            return ""

    project_hash = {}

    projects_tree = _get_projects_tree(hour_counter)
    for project in projects_tree.findall("./Project"):
        name = safe_elem_text(project.find("./Name"))
        if not name:
            continue  # malformed project?
        field_of_science = safe_elem_text(project.find("./" + field))
        project_hash[name] = field_of_science

    return project_hash


def organization_from_project(project_name: str):
    hour_counter = int(time.time() / 3600)
    organizationName =  _get_project_fos_hash(hour_counter,"Organization").get(project_name, "Unknown").replace(',',' -')
    institutionId =  _get_project_fos_hash(hour_counter,"InstitutionID").get(project_name, "Unknown").replace(',',' -')
    # We want to get the organization name from the institution ID, as this is the 'new' way to do it.
    # However, if there is not a valid institution ID, we will fallback to the organization name.
    return getOrganizationFromInstitutionID(institutionId,organizationName)


def field_of_science_from_project(project_name: str):
    hour_counter = int(time.time() / 3600)
    return _get_project_fos_hash(hour_counter,"FieldOfScience").get(project_name, "Unknown").replace(',',' -')


#######################
# Functions to query Ganglia Web and format response for HTCondorView
#######################

@cache_response_to_disk()
def get_data_from_ganglia():
    # Grab parameters host (CE name), r (range: day, week, or month)
    host = request.args.get('host','chtc-spark-ce1.svc.opensciencegrid.org')
    r = request.args.get('r','day')

    # Grab data as a raw csv from ganglia, which was stashed there by the condor_gangliad
    import pandas as pd
    # If host is not fully qualified, add the default domain
    if not host.endswith('.' + current_app.config['CE_DASHBOARD_DEFAULT_CE_DOMAIN']):
        host = host + '.' + current_app.config['CE_DASHBOARD_DEFAULT_CE_DOMAIN']
    
    metricsd_url = current_app.config['CE_DASHBOARD_METRICSD_URL']

    # Retrieve any metrics from Ganglia that start with any of the following phrases
    metric_exprs = ['Cpus', 'Gpus','Memory','Disk','Bcus','Glideins']
    metric_args = '&'.join([f'mreg[]=%5E{expr}' for expr in metric_exprs])

    df=pd.read_csv(f'{metricsd_url}/ganglia/graph.php?r={r}&hreg[]={host}&{metric_args}&aggregate=1&csv=1',skipfooter=1,engine='python')

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
    for oldname, newname in (['CpusIn', 'CpusAllocated'],
                              ['CpusNotIn', 'CpusUnallocated'],
                              ['GpusIn', 'GpusAllocated'],
                              ['GpusNotIn', 'GpusUnallocated'],
                              ['MemoryIn', 'MemoryAllocated'],
                              ['MemoryNotIn', 'MemoryUnallocated'],
                              ['DiskIn', 'DiskAllocated'],
                              ['DiskNotIn', 'DiskUnallocated'],
                              ['BcusIn', 'BcusAllocated'],
                              ['BcusNotIn', 'BcusUnallocated'],
                              ['GlideinsCpuLimited', 'GlideinsCpuLimited'],
                              ['GlideinsMemoryLimited', 'GlideinsMemoryLimited'],
                              ['GlideinsDiskLimited', 'GlideinsDiskLimited']):
        df['Project'] = df['Project'].str.replace(f'____meta_{oldname}', newname)
    # Get rid of columns that are not needed; specifically, we don't want info per user, just per project   
    df.drop(columns=['Cpus_User'],inplace=True,errors='ignore')
    df.drop(columns=['Memory_User'],inplace=True,errors='ignore')
    # Remove projects where all rows with Type="P" have Cpus=0.0
    projects_to_remove = df[(df['Type'] == 'P') & (df['Cpus'] == 0.0)]['Project'].unique()
    projects_with_nonzero_cpus = df[(df['Type'] == 'P') & (df['Cpus'] > 0.0)]['Project'].unique()
    projects_to_remove = set(projects_to_remove) - set(projects_with_nonzero_cpus)
    df = df[~df['Project'].isin(projects_to_remove)]
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
    'Contributed': 'contributed.html',
}

@overview_bp.route('/allocated_graph_<resource>.html')
def handle_allocated_graph(resource):
    host = request.args.get('host','chtc-spark-ce1.svc.opensciencegrid.org')
    from blueprints.landing import get_ce_facility_site_descrip
    facility, site, descrip, health = get_ce_facility_site_descrip(host)
    title = facility if facility!='Unknown' else host
    return render_template('allocated_graph.html', resource=resource,
                           linkmap=overview_linkmap,
                           page_title=title, ce_facility_name=facility, ce_site_name=site)

@overview_bp.route('/contributed.html')
def handle_contributed():
    time_range = request.args.get('r','week')
    if time_range not in ['hour','day','week','month','year']:
        time_range = 'week'
    host = request.args.get('host','chtc-spark-ce1.svc.opensciencegrid.org')
    from blueprints.landing import get_ce_facility_site_descrip
    facility, site, descrip, health = get_ce_facility_site_descrip(host)
    title = facility if facility!='Unknown' else host
    return render_template('contributed.html', host=host, 
                           linkmap=overview_linkmap, time_range=time_range,
                           page_title=title, ce_facility_name=facility, ce_site_name=site)

@overview_bp.route('/overview.html')
def overview():
    host = request.args.get('host','chtc-spark-ce1.svc.opensciencegrid.org')
    from blueprints.landing import get_ce_facility_site_descrip
    facility, site, descrip, health = get_ce_facility_site_descrip(host)
    title = facility if facility!='Unknown' else host
    return render_template('overview.html', page_title=title, ce_facility_name=facility, ce_site_name=site,
                           ce_description=descrip, ce_health=health, linkmap=overview_linkmap)

@overview_bp.route('/data/ce_overview')
def ce_overview_data():
    response_body, cached_response = get_data_from_ganglia()
    return make_data_response(response_body,cached_response)

@overview_bp.route('/error-no-data.html')
def error_no_data():
    from blueprints.landing import get_ce_facility_site_descrip
    facility, site, descrip, health = get_ce_facility_site_descrip(request.args.get('host','chtc-spark-ce1.svc.opensciencegrid.org'))
    msg = request.args.get('msg','No data available')
    return render_template('error_no_data.html', page_title=facility, ce_name=site, errMsg=msg, linkmap={})
