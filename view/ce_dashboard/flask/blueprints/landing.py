from flask import Blueprint, render_template,redirect, url_for, request
import csv
from io import StringIO
import typing as t
import urllib
import urllib.request
import xml.etree.ElementTree as ET
import classad2 as classad
import htcondor2 as htcondor
from dataclasses import dataclass, fields, astuple
from math import floor
import time
import threading
from utils import cache_response_to_disk, make_data_response
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
    def __post_init__(self):
        self.hosted = is_hosted_fqdn(self.fqdn)


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
    except:
        for fqdn in resource_info_by_fqdn:
            resource_info_by_fqdn[fqdn].health="Unknown"
            resource_info_by_fqdn[fqdn].healthInfo="Server collector.opensciencegrid.org unreachable"
        return
    htcondor.param["QUERY_TIMEOUT"] = "5"
    for ad in ads:
        fqdn = ad["Name"]
        #if fqdn in resource_info_by_fqdn and resource_info_by_fqdn[fqdn].hosted:
        if fqdn not in resource_info_by_fqdn:
            # The CE is not registered in Topology
            resource_info = ResourceInfo(
                fqdn=fqdn,
                active=True,
                registered=False
            )
            resource_info_by_fqdn[fqdn] = resource_info            
        info = resource_info_by_fqdn[fqdn]
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
                if "GridResourceUnavailableTime" in gridAd and "GriqdResourceUnavailableTimeReason" in gridAd:
                    hinfo = classad.ExprTree('strcat("Since ",formatTime(GridResourceUnavailableTime),": ",GridResourceUnavailableTimeReason)').eval(gridAd)
                    info.health="Poor"
                    info.healthInfo=hinfo
                    continue
        # Check for load problems (RecentDaemonCoreDutyCycle and File Transfer load are reflected in Status attr)
        status = "Unknown"
        if "Status" in ad:
            status = classad.ExprTree('Status').eval(ad)
        if status == "CRITICAL":
            info.health="Poor"
            info.healthInfo = "Server load level = " + status
            continue
        if status != "OK":
            info.health = "Fair"
            info.healthInfo = "Server load level = " + status
            continue
        # Check for too many held jobs, or idle jobs without anything running, etc.
        totalHeld = ad["TotalHeldJobs"]
        totalRunning  = ad["TotalRunningJobs"]
        totalIdle = ad["TotalIdleJobs"]
        if info.hosted or info.scheduler.casefold() != "Condor".casefold():
            # All CEs that are (a) hosted or (b) in front of any system other than
            # HTCondor will have two running jobs per running glidein: the vanilla
            # universe provisioning request, and the routed grid universe.  So
            # for these systems, divide totalRunning by two.
            totalRunning = floor( totalRunning / 2)
        if totalHeld > totalRunning and totalRunning > 0:
            info.health="Fair"       
            info.healthInfo="More glideins held than running: running=" + str(totalRunning) + " held=" + str(totalHeld) + " idle=" + str(totalIdle)
            continue
        if totalRunning == 0:
            if totalHeld > 0:
                info.health="Poor"       
                info.healthInfo="Found held glideins and none running: running=" + str(totalRunning) + " held=" + str(totalHeld) + " idle=" + str(totalIdle)
                continue
            if totalIdle > 0:
                info.health="Fair"       
                info.healthInfo="No glideins are running: running=" + str(totalRunning) + " held=" + str(totalHeld) + " idle=" + str(totalIdle)
                continue
            if totalIdle == 0:
                info.health="Fair"       
                info.healthInfo="No glideins at all at this CE; maybe factory misconfigured?"
                continue
        # If we made it here, things look good!
        info.health="Good"
        info.healthInfo="Glideins running=" + str(totalRunning) + " held=" + str(totalHeld) + " idle=" + str(totalIdle)


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
        # resourcegroup_name = elem_text(resourcegroup, "./GroupName")
        facility_name = elem_text(resourcegroup, "./Facility/Name")
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
                name=name if name else fqdn.split(',')[0],  # use first part of fqdn if no name is given
                active=(active == "true"),  # convert string to bool
                isCCStar=(isCCStar == "true"),  # convert string to bool
            )
            resource_info_by_fqdn[fqdn] = resource_info
    return resource_info_by_fqdn

@cache_response_to_disk(file_name="info.csv")
def get_landing_response():
    # Gather up CE info from Topology and Collectors
    resource_info_by_fqdn = ce_info_from_topology()
    ce_info_from_collectors(resource_info_by_fqdn)
    # Output this info as a CSV that htcondorview can use
    output = StringIO()
    writer = csv.writer(output,quoting=csv.QUOTE_ALL,lineterminator=']')
    output.write('[[')
    writer.writerow([field.name for field in fields(ResourceInfo)])
    for ri in resource_info_by_fqdn.values():
        output.write(',\n[')
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
        return ce_info.facility_name, ce_info.site_name, ce_info.description
    return "Unknown", "Unknown", "Unknown"

##########################################
# Flask routes
##########################################

landing_bp = Blueprint('landing', __name__)
lock = threading.Lock()
ce_info_dict = {}
ce_info_dict_last_update_time = 0

landing_linkmap = {
    'Reload': 'url_for("landing.ce_admin_landing_page"),',
    'Help': {
        'Query Syntax': 'query_syntax.html',
        'Customization': 'customization.html'
    }
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
    if stored_page_url:
        return redirect(url_for('overview.overview') + '?' + urllib.parse.urlencode({'host': stored_page_url}))
    else:
        # Redirect to the default user landing page
        return redirect(url_for('landing.ce_user_landing_page'))
