from flask import Blueprint, render_template, request, current_app
import csv
import urllib.request
import functools
import xml.etree.ElementTree as ET
from utils import cache_response_to_disk, make_data_response, getOrganizationFromInstitutionID
import time
from blueprints.landing import ce_info_from_topology, ce_info_from_collectors, ce_info_from_ganglia
import pandas as pd

@cache_response_to_disk()
def get_landing_graph_ganglia_data():
    r = request.args.get('r','day')

    metricsd_url = current_app.config['CE_DASHBOARD_METRICSD_URL']

    # Retrieve any metrics from Ganglia that start with any of the following phrases
    metric_exprs = ['Cpus.%2AInUse', 'EPsRunning', 'GlideinsRunning']
    metric_args = '&'.join([f'mreg[]=%5E{expr}' for expr in metric_exprs])

    df=pd.read_csv(f'{metricsd_url}/ganglia/graph.php?r={r}&hreg[]=svc.opensciencegrid.org&hreg[]=svc.osg-htc.org&{metric_args}&aggregate=1&csv=1',skipfooter=1,engine='python')

    # Transpose the data received from ganglia into the format we need for the CE Dashboard frontend

    # Rename 'Timestamp' column to 'Date' for clarity and set it as the index
    df.rename({'Timestamp':'Date'}, axis='columns', inplace=True)
    df.set_index('Date', inplace=True)
    # Split column names into multiple levels based on specific delimiters
    df.columns = df.columns.str.split(' ', n=1, expand=True)
    # Define new names for the column levels
    new_names = ['Site', 'Resource']
    df.columns.names = new_names
    # Reshape the DataFrame from wide to long format
    df = df.stack(new_names).rename('value').reset_index()
    # Strip whitespace from 'Resource' column and handle empty project names
    df['Resource'] = df['Resource'].map(str.strip)
    # Simplify 'Resource' column by removing specific suffixes
    # df['Resource'] = df['Resource'].str.replace('(In|NotIn)$', '', regex=True)
    # Pivot the DataFrame to create a schema with 'Date' and 'Project' as indices
    df = df.pivot(index=['Date', 'Site'], columns='Resource', values='value')
    # Fill missing values with zeros to handle incomplete data
    df.fillna(0, inplace=True)
    # Remove axis names and reset the index for a flat structure
    df = df.rename_axis(None, axis=1).reset_index()
    # Group by 'Date' and 'Project' to aggregate data
    df = df.groupby(['Date', 'Site'], as_index=False).sum()

    # Finally, serialize the data into a string in jsonp format as expected by the HTCondorView javascript
    mycsv = df.to_csv(index=False,quoting=csv.QUOTE_ALL).splitlines()
    index = 0
    response = "["
    while index < len(mycsv) - 1:
        response += "[" + mycsv[index] + "],\n"
        index += 1
    response += "[" + mycsv[index] + "]\n]"
    
    return response

@cache_response_to_disk()
def get_site_names_with_data():
    resource_info_by_fqdn = ce_info_from_topology()
    ce_info_from_ganglia(resource_info_by_fqdn)
    # Now gather up CE info from the collectors - currently must be done after all other sources
    ce_info_from_collectors(resource_info_by_fqdn)
    pass
    
#######################
# Flask routes
#######################

landing_graphs_bp = Blueprint('landing_graphs', __name__)

landing_graphs_linkmap = {
    'Overview': 'overview.html',
    'Contributed': 'contributed.html',
}

@landing_graphs_bp.route('/data/landing_graphs')
def ce_overview_data():
    response_body, cached_response = get_landing_graph_ganglia_data()
    return make_data_response(response_body,cached_response)


@landing_graphs_bp.route('/landing3.html')
def ce_admin_landing_page():
    sites = [ce for ce in ce_info_from_topology().values() if ce.hosted and ce.active]
    return render_template(
        'landing3.html',
        linkmap={},
        sites=sites,
        page_title="Hosted CE Contributions vs Allocations")
