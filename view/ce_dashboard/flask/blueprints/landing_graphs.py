from flask import Blueprint, render_template, request, current_app
import csv
import urllib.request
import functools
import xml.etree.ElementTree as ET
from utils import cache_response_to_disk, make_data_response, getOrganizationFromInstitutionID
import time
from blueprints.landing import ce_info_from_topology, ce_info_from_collectors, ce_info_from_ganglia, returnOrAddUnregisteredInfo
import classad2 as classad
import htcondor2 as htcondor
from datetime import datetime
import json
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

def ce_last_heard_from(resource_info_by_fqdn):
    """
    Augments a resource_info_by_fqdn dict with the lastHeardFrom field taken
    from each hosted, active CE's grid ads in its own CE collector.
    """
    coll = htcondor.Collector("collector.opensciencegrid.org:9619")
    try:
        ads = coll.query(htcondor.AdTypes.Schedd, projection=["Name", "CollectorHost"])
    except Exception as e:
        print(f"ce_last_heard_from: unable to reach collector.opensciencegrid.org:9619: {e}")
        return

    for ad in ads:
        fqdn = ad["Name"]
        if fqdn not in resource_info_by_fqdn:
            continue
        info = resource_info_by_fqdn[fqdn]
        if not (info.hosted and info.active):
            continue
        if "CollectorHost" not in ad:
            continue
        try:
            ce_coll = htcondor.Collector(ad["CollectorHost"])
            grid_ads = ce_coll.query(htcondor.AdTypes.Grid, projection=["LastHeardFrom"])
        except Exception as e:
            print(f"ce_last_heard_from: unable to reach CE collector for {fqdn}: {e}")
            continue
        for grid_ad in grid_ads:
            if "LastHeardFrom" in grid_ad:
                info.lastHeardFrom = datetime.fromtimestamp(grid_ad["LastHeardFrom"]).strftime("%Y-%m-%d %H:%M:%S")

@cache_response_to_disk()
def get_site_names_with_update_time():
    resource_info_by_fqdn = ce_info_from_topology()
    ce_last_heard_from(resource_info_by_fqdn)
    return json.dumps({
        fqdn: info.lastHeardFrom
        for fqdn, info in resource_info_by_fqdn.items()
        if info.hosted and info.active
    })
    
#######################
# Flask routes
#######################

landing_graphs_bp = Blueprint('landing_graphs', __name__)

landing_graphs_linkmap = {
    'Overview': 'overview.html',
    'Contributed': 'contributed.html',
}

@landing_graphs_bp.route('/data/ce_update_times')
def ce_update_times():
    response_body, cached_response = get_site_names_with_update_time()
    return make_data_response(response_body, cached_response)

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
