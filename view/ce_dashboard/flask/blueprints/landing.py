from flask import Blueprint, render_template,redirect, url_for, request, current_app
import csv
from io import StringIO
import typing as t
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
import classad2 as classad
import htcondor2 as htcondor
from dataclasses import dataclass, fields, astuple
from math import floor
import time
import threading
from utils import cache_response_to_disk, make_data_response, getOrganizationFromInstitutionID
# from . import overview  # Import the overview module

##########################################
# Functions to generate the data by querying Topology and Collectors
##########################################

def get_resources_tree(params: str) -> ET.Element:
    """
    Query the Topology resource group XML endpoint with the given parameters.
    Returns an ElementTree Element.

    May raise urllib.error.HTTPError.
    """
    handle = urllib.request.urlopen(
        "https://topology.opensciencegrid.org/rgsummary/xml?%s" % params
    )
    result_bytes = handle.read()

    return ET.fromstring(
        result_bytes.decode(encoding="utf-8", errors="surrogateescape")
    )


def elem_text(elem: ET.Element, path: t.Union[None, str] = None) -> str:
    """
    Get the text from an element (or subelement if `path` is provided), stripped of
    leading and trailing whitespace.
    Return the empty string if the element doesn't exist.
    """
    if path:
        elem = elem.find(path)
    try:
        return elem.text.strip()
    except AttributeError:  # we got a None
        return ""


def is_hosted_fqdn(fqdn):
    """
    Return True if the FQDN is that of a hosted CE
    """
    return (
        fqdn.startswith("hosted-ce")
        and (
            fqdn.endswith(".grid.uchicago.edu") or fqdn.endswith(".opensciencegrid.org")
        )
    ) or fqdn.endswith(".svc.opensciencegrid.org")

@dataclass
class ResourceInfo:
    fqdn: str
    active: bool
    registered: bool = True
    facility_name: str = "Unknown" # a.k.a. institution
    site_name: str = "Unknown"
    description: str = "Unknown"
    name: str = "Unknown" 
    isCCStar: bool = False  # True if this is a CCstar site
    hosted: bool = False
    scheduler: str = "Unknown"
    health: str = "Poor"
    healthInfo: str = "Not reporting; likely not running"
    startTime: str = "Unknown"
    version: str = "Unknown"
    allocationsPastWeek: bool = False
    allocationsPastMonth: bool = False
    glideinsRunning: int = 0
    glideinsIdle: int = 0
    glideinsHeld: int = 0
    def __post_init__(self):
        self.hosted = is_hosted_fqdn(self.fqdn)


@dataclass
class HeaderLink:
    """ Util class containing the text and URL for a header link """
    title: str
    tooltip: str
    url: str

def returnOrAddUnregisteredInfo(resource_info_by_fqdn, fqdn):
    """
    Add an unregistered CE to the resource info dictionary.
    """
    if fqdn not in resource_info_by_fqdn:
        # The CE is not registered in Topology
        resource_info = ResourceInfo(
            fqdn=fqdn,
            active=True,
            registered=False
        )
        resource_info_by_fqdn[fqdn] = resource_info
    return resource_info_by_fqdn[fqdn]

def ce_info_from_ganglia(resource_info_by_fqdn):
    """
    Given a dictionary of CE info from Topology, augment this with information
    gleaned from the time series database.
    """
    import pandas as pd
    host = current_app.config['CE_DASHBOARD_DEFAULT_CE_DOMAIN']
    r = 'month'
    df=pd.read_csv('https://display.ospool.osg-htc.org/ganglia/graph.php?r=' + r + '&hreg[]=' + host + '&mreg[]=%5E' + 'CpusInUse' + '&aggregate=1&csv=1',skipfooter=1,engine='python')
    
    # Rename 'Timestamp' column to 'Date' for clarity and set it as the index
    df.rename({'Timestamp':'Date'}, axis='columns', inplace=True)
    df.set_index('Date', inplace=True)
    # Convert the index to datetime
    df.index = pd.to_datetime(df.index, utc=True)
    df.index = df.index.tz_localize(None)
    
    # The columns are the FQDNs of the CEs.  The values are the number of CPUs in use at that time.
    # We want a list of the FQDNs that did not use any CPUs at all in the last week, and 
    # a second list of the FQDNs that did not use any CPUs at all in the last month.
    from datetime import datetime, timedelta

    # Define the time ranges for the last week and last month
    one_week_ago = datetime.now() - timedelta(weeks=1)
    one_month_ago = datetime.now() - timedelta(weeks=4)

    # Filter FQDNs that did not use any CPUs in the last week
    fqdn_some_cpus_past_week = [
        fqdn.strip() for fqdn in df.columns
        if df.loc[df.index >= one_week_ago, fqdn].sum() > 0
    ]
    # Set the allocationsPastWeek flag for these FQDNs
    for fqdn in fqdn_some_cpus_past_week:
        info = returnOrAddUnregisteredInfo(resource_info_by_fqdn, fqdn)
        info.allocationsPastWeek = True

    # Filter FQDNs that did not use any CPUs in the last month
    fqdn_some_cpus_past_month = [
        fqdn.strip() for fqdn in df.columns
        if df.loc[df.index >= one_month_ago, fqdn].sum() > 0
    ]
    # Set the allocationsPastMonth flag for these FQDNs
    for fqdn in fqdn_some_cpus_past_month:
        info = returnOrAddUnregisteredInfo(resource_info_by_fqdn, fqdn)
        info.allocationsPastMonth = True

def ce_info_from_collectors(resource_info_by_fqdn):
    """
    Given a dictionary of CE info from Topology, augment this with information
    gleaned from the top-level CE collector, and from querying CE collectors
    for hosted/active CEs.
    """
    # First grab all ads from top level collector
    coll = htcondor.Collector("collector.opensciencegrid.org:9619")
    try:
        ads = coll.query(htcondor.AdTypes.Schedd,projection=["Name","CollectorHost","OSG_BatchSystems","Status","DaemonStartTime","HTCondorCEVersion","TotalRunningJobs","TotalHeldJobs","TotalIdleJobs"])
    except Exception as e:
        print(f"Unable to reach collector.opensciencegrid.org:9619: {e}")
        for fqdn in resource_info_by_fqdn:
            resource_info_by_fqdn[fqdn].health="Unknown"
            resource_info_by_fqdn[fqdn].healthInfo="Server collector.opensciencegrid.org unreachable"
        return
    htcondor.param["QUERY_TIMEOUT"] = "5"
    for ad in ads:
        fqdn = ad["Name"]
        info = returnOrAddUnregisteredInfo(resource_info_by_fqdn, fqdn)
        info.scheduler = ad.get("OSG_BatchSystems","Unknown")
        info.scheduler = info.scheduler[0].upper() + info.scheduler[1:]  # ensure first letter capitalized
        if info.scheduler == "Condor":
            info.scheduler = "HTCondor"
        info.version = ad.get("HTCondorCEVersion","Unknown")
        if "DaemonStartTime" in ad:
            info.startTime = classad.ExprTree('formatTime(DaemonStartTime,"%Y-%m-%d")').eval(ad)
        # If CE is hosted and active, try to query the CE collector for grid ads
        if info.hosted and info.active:
            canReachCeCollector = False
            if "CollectorHost" in ad:
                ceColl = htcondor.Collector(ad["CollectorHost"])
                try:
                    gridAds = ceColl.query(htcondor.AdTypes.Grid,projection=["GridResourceUnavailableTime","GridResourceUnavailableTimeReason"])
                    canReachCeCollector = True
                except:
                    canReachCeCollector = False
            if not canReachCeCollector:
                info.health="Poor"
                info.healthInfo="CE Collector unreachable"
                continue
            for gridAd in gridAds:
                if "GridResourceUnavailableTime" in gridAd and "GridResourceUnavailableTimeReason" in gridAd:
                    hinfo = classad.ExprTree('strcat("Since ",formatTime(GridResourceUnavailableTime),": ",GridResourceUnavailableTimeReason)').eval(gridAd)
                    info.health="Poor"
                    info.healthInfo=hinfo
                    continue
        
        # Check for too many held jobs, or idle jobs without anything running, etc.
        totalHeld = ad["TotalHeldJobs"]
        totalIdle = ad["TotalIdleJobs"]
        totalDaysNoStartedJobs = 0
        if info.allocationsPastWeek == False:
            totalDaysNoStartedJobs = 7
        if info.allocationsPastMonth == False:
            totalDaysNoStartedJobs = 30
        totalRunning  = ad["TotalRunningJobs"]
        if info.hosted or info.scheduler.casefold() != "Condor".casefold():
            # All CEs that are (a) hosted or (b) in front of any system other than
            # HTCondor will have two running jobs per running glidein: the vanilla
            # universe provisioning request, and the routed grid universe.  So
            # for these systems, divide totalRunning by two.
            totalRunning = floor( totalRunning / 2)
        info.glideinsRunning = totalRunning
        info.glideinsIdle = totalIdle
        info.glideinsHeld = totalHeld

        status = "Unknown"
        if "Status" in ad:
            status = str( classad.ExprTree('Status').eval(ad) )

        # Checks here should be done in the order of severity, from most to least 
        if totalRunning == 0 and totalIdle == 0 and totalHeld == 0:
            info.health="Poor"       
            info.healthInfo="No glideins at all at this CE; maybe factory misconfigured?"
            continue
        if totalDaysNoStartedJobs > 29:
            info.health="Poor"       
            info.healthInfo="No allocated Cpus in the past month"
            continue
        if totalDaysNoStartedJobs > 6:
            info.health="Fair"       
            info.healthInfo="No allocated Cpus in the past week"
            continue
        if totalHeld > totalRunning + totalIdle and totalRunning > 0:
            info.health="Fair"       
            info.healthInfo="Many glideins being held: running=" + str(totalRunning) + " held=" + str(totalHeld) + " idle=" + str(totalIdle)
            continue
        # Check for load problems (RecentDaemonCoreDutyCycle and File Transfer load are reflected in Status attr)
        if status == "CRITICAL":
            info.health="Fair"
            info.healthInfo = "Server load level = " + status
            continue
        if status == "WARNING":
            info.health = "Fair"
            info.healthInfo = "Server load level = " + status
            continue
        
        # If we made it here, things look good!
        info.health="Good"
        #info.healthInfo="Glideins running=" + str(totalRunning) + " held=" + str(totalHeld) + " idle=" + str(totalIdle)
        info.healthInfo=""


def ce_info_from_topology() -> t.Dict[str, ResourceInfo]:
    """
    Get the resource info of production CEs from Topology
    """
    params = [
        # "gridtype=on&gridtype_1=on",  # production, not itb
        "service=on&service_1=on",  # resources with CEs
    ]
    tree = get_resources_tree("&".join(params))

    resource_info_by_fqdn = {}

    for resourcegroup in tree.findall("./ResourceGroup"):
        # We want to use the institution ID to get the organization name, as this is the new way to do it.
        # If the institution ID is not found, we will fallback to the old facility name.
        old_facility_name = elem_text(resourcegroup, "./Facility/Name")
        institution_id = elem_text(resourcegroup, "./Facility/InstitutionID")
        facility_name = getOrganizationFromInstitutionID(institution_id, old_facility_name)
        site_name = elem_text(resourcegroup, "./Site/Name")
        for resource in resourcegroup.findall("./Resources/Resource"):
            fqdn = elem_text(resource, "./FQDN")
            if not fqdn:
                continue
            name = elem_text(resource, "./Name")
            description = elem_text(resource, "./Description")
            description = " ".join(description.splitlines())    # Description may have newlines; get rid of em
            description = description.replace('\\','')          # Get rid of escaping backslashes
            description = description.replace('"','')           # Get rid of doublequotes
            description = description.replace(',',';')          # Get rid of commas, as we use commas to delimit fields in the CSV output
            active = elem_text(resource, "./Active").lower()
            isCCStar = elem_text(resource, "./IsCCStar").lower()
            resource_info = ResourceInfo(
                fqdn=fqdn,
                facility_name=facility_name,
                site_name=site_name,
                description=description,
                name=name,
                active=(active == "true"),  # convert string to bool
                isCCStar=(isCCStar == "true"),  # convert string to bool
            )
            resource_info_by_fqdn[fqdn] = resource_info
    return resource_info_by_fqdn

@cache_response_to_disk(file_name="ce_info.csv")
def get_landing_response():
    # Gather up CE info from Topology
    resource_info_by_fqdn = ce_info_from_topology()
    # Now gather up CE info from the ganglia time series database
    ce_info_from_ganglia(resource_info_by_fqdn)
    # Now gather up CE info from the collectors - currently must be done after all other sources
    ce_info_from_collectors(resource_info_by_fqdn)
    # Output this info as a CSV that htcondorview can use
    default_domain = current_app.config['CE_DASHBOARD_DEFAULT_CE_DOMAIN']
    output = StringIO()
    writer = csv.writer(output,quoting=csv.QUOTE_ALL,lineterminator=']')
    output.write('[[')
    writer.writerow([field.name for field in fields(ResourceInfo)])
    for ri in resource_info_by_fqdn.values():
        output.write(',\n[')
        # If the Name is unknown, use the first part of the FQDN
        if ri.name=="Unknown":
            ri.name =  ri.fqdn.split('.')[0]
        # If the FQDN ends with the default domain, remove it
        if ri.fqdn.endswith('.' + default_domain):
            ri.fqdn = ri.fqdn[:-len(default_domain)-1]
        # Add a URL with the name to point to the Overview page if not in Poor health
        if ri.health != "Poor":
            if ri.allocationsPastWeek:
                ri.name=f"/overview.html?host={ri.fqdn}|{ri.name}"
            else:
                # No allocations in the past week, so use the month view
                ri.name=f"/overview.html?host={ri.fqdn}&r=month|{ri.name}"
        writer.writerow(astuple(ri))
    output.write(']')
    result = output.getvalue()
    output.close()
    result = result.replace('"True"','true')
    result = result.replace('"False"','false')
    return result

def update_ce_info():
    """
    Update the in-memory dictionary of CE info
    """
    # check if we updated recently
    global ce_info_dict_last_update_time
    with lock:
        if (time.time() - ce_info_dict_last_update_time) < 60 * 10: # only update if last update was more than 10 minutes ago
            return
        ce_info_dict_last_update_time = time.time()
    # get the response and parse it
    response, _ = get_landing_response()
    global ce_info_dict
    with lock:
        ce_info_dict.clear()
        for line in response.splitlines():
            line = line.rstrip(',')
            if line.startswith('[') and line.endswith(']'):
                values = line.strip('[]').replace('"','').split(',')
                if len(values) == len(fields(ResourceInfo)):
                    ri = ResourceInfo(*values)
                    ce_info_dict[ri.fqdn] = ri

def get_ce_info(fqdn: str) -> t.Optional[ResourceInfo]:
    """
    Get the CE info for a given FQDN from the in-memory dictionary.
    If the FQDN is not found, try updating the dictionary and return None if not found.
    """
    if fqdn in ce_info_dict:
        return ce_info_dict[fqdn]
    # If not found, update the dictionary and try again     
    update_ce_info()
    return ce_info_dict.get(fqdn)

def get_ce_facility_site_descrip(fqdn: str):
    """
    Get the facility/site description for a given FQDN.
    If the FQDN is not found, try updating the dictionary and return None if not found.
    """
    ce_info = get_ce_info(fqdn)
    if ce_info:
        # Re-extract the original site name from the URL if the health isn't Poor
        name = ce_info.name if ce_info.health == "Poor" else ce_info.name.split('|',1)[1]
        return ce_info.facility_name, ce_info.site_name, name, ce_info.description, ce_info.health
    return "Unknown", "Unknown", "Unknown", "Unknown", "Poor"

def get_next_prev_sites(fqdn: str) -> t.Tuple[t.Optional[HeaderLink], t.Optional[HeaderLink]]:
    """
    Given a site FQDN, return the next and previous healthy sites in alphabetical order.
    """
    # Get all CE Facility names in sorted order, for the facilities that have links from the homepage
    healthy_facilities_by_name = sorted(
        (ce for ce in ce_info_dict.values() if 'overview.html' in ce.name and ce.health != "Poor"), 
        key=lambda x: x.facility_name)
    current_index = next((i for i, ri in enumerate(healthy_facilities_by_name) if ri.fqdn == fqdn), 0)

    prev_index = current_index - 1 if current_index > 0  else len(healthy_facilities_by_name) - 1
    next_index = current_index + 1 if current_index < len(healthy_facilities_by_name) - 1 else 0

    prev_facility = healthy_facilities_by_name[prev_index]
    next_facility = healthy_facilities_by_name[next_index]

    # TODO re-extracting the original name and link from the parsed facility csv here is a bit hacky
    prev_link, prev_name = prev_facility.name.split('|',1)
    next_link, next_name = next_facility.name.split('|',1)

    return (
        HeaderLink(title='Next\nCE', tooltip=f"{next_facility.facility_name} - {next_name}", url=next_link),
        HeaderLink(title='Prev\nCE', tooltip=f"{prev_facility.facility_name} - {prev_name}", url=prev_link)
    )

##########################################
# Flask routes
##########################################

landing_bp = Blueprint('landing', __name__)
lock = threading.Lock()
ce_info_dict : dict[str, ResourceInfo] = {}
ce_info_dict_last_update_time = 0

landing_linkmap = {
}

@landing_bp.route('/data/ce_landing')
def ce_landing_data():
    response_body, cached_response = get_landing_response()
    return make_data_response(response_body,cached_response)


@landing_bp.route('/landing.html')
def ce_admin_landing_page():
    return render_template('landing.html',linkmap=landing_linkmap,page_title="Hosted CE Dashboards")

@landing_bp.route('/home.html')
@landing_bp.route('/select.html')
@landing_bp.route('/index.html')
def ce_user_landing_page():
    return render_template('home.html',linkmap={},page_title="Available CE Dashboards")

@landing_bp.route('/')
def ce_goto_default_or_user_landing_page():
    """
    Examine the cookie named ceDashStoredHost to see if the user has a preferred page.
    If so, redirect to that page.
    If not, redirect to the default landing page.
    If the cookie is not found, redirect to the default landing page.       
    """
    stored_page_url = request.cookies.get('ceDashStoredHost')
    if stored_page_url and stored_page_url != "none":
        return redirect(url_for('overview.overview') + '?' + urllib.parse.urlencode({'host': stored_page_url}))
    else:
        # Redirect to the default user landing page
        return redirect(url_for('landing.ce_user_landing_page'))
