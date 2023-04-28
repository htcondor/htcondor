#!/usr/bin/python3
# HTCondor V9 to V10 check for incompatibilites script
#   This is intended to check a current system before upgrade
#   for known 'gotcha' breaking changes:
#        1. IDToken TRUST_DOMAIN default value change
#        2. Upgrade to PCRE2 breaking map file regex secuences
#        3. The way to request GPU resources for a job
#   This script can still be ran post upgrade as a post mortem
#   Note: Requires sudo and HTCONDOR python bindings
import os
import sys
import platform
import subprocess

try:
    import htcondor
except ImportError:
    msg = """Failed to find HTCondor Python bindings.
Please check your current Python environment or install the bindings if needed:
https://htcondor.readthedocs.io/en/latest/apis/python-bindings/install.html"""
    print(msg, file=sys.stderr)
    sys.exit(1)

# Base special posix characters for PCRE2 regex sequences
PCRE2_POSIX_CHARS = [
    "[:alnum:]",
    "[:alpha:]",
    "[:ascii:]",
    "[:blank:]",
    "[:cntrl:]",
    "[:digit:]",
    "[:graph:]",
    "[:lower:]",
    "[:print:]",
    "[:punct:]",
    "[:space:]",
    "[:upper:]",
    "[:word:]",
    "[:xdigit:]",
]
PCRE2_TEST_FILE = "HTCondor-PCRE2-Test.txt"


def format_print(msg, offset=0, newline=False):
    """Custom print function to help with output spacing"""
    linestart = ""
    if newline:
        linestart = "\n"
    print("{}{}".format(linestart.ljust(offset, " "), msg))


def get_token_configuration(param, is_ce, checking_remote_collector):
    """Get IDToken information from the given system configuration"""
    # Base return info
    ret_info = {
        "trust_domain": {"is_default": False, "unexpanded": None, "value": None},
        "version": 9,
        "has_signing_key": False,
        "using_token_auth": False,
    }
    ret_info["version"] = int(param["CONDOR_VERSION"].split(".")[0])
    # Check for token authentication being used (unlikely not set)
    for key in param.keys():
        if ret_info["using_token_auth"]:
            break
        if "SEC_" in key and key[-22:] == "AUTHENTICATION_METHODS":
            delim_char = "," if "," in param[key] else " "
            for method in param[key].split(delim_char):
                method = method.strip(", ")
                if method.upper() == "TOKEN":
                    ret_info["using_token_auth"] = True
                    break
    if not ret_info["using_token_auth"]:
        return ret_info
    # Unexpanded default values for TRUST_DOMAIN
    default_trust_domain = "$(COLLECTOR_HOST)"
    if ret_info["version"] >= 10:
        default_trust_domain = "$(UID_DOMAIN)"
    # Create command line to run to get unexpanded TRUST_DOMAIN value from config
    cmd = []
    if is_ce:
        cmd.append("condor_ce_config_val")
    else:
        cmd.append("condor_config_val")
    if checking_remote_collector:
        cmd.append("-collector")
    cmd.extend(["-dump", "TRUST_DOMAIN"])
    p = subprocess.run(cmd, stdout=subprocess.PIPE)
    cmd_output = p.stdout.rstrip().decode()
    # Find actual TRUST_DOMAIN line in file dump and check if default value
    for line in cmd_output.split("\n"):
        if line[0:14] == "TRUST_DOMAIN =":
            unexpanded_val = line.split("=")[1].strip()
            temp = {
                "is_default": False,
                "unexpanded": unexpanded_val,
                "value": param.get("TRUST_DOMAIN"),
            }
            if unexpanded_val == default_trust_domain:
                temp["is_default"] = True
            ret_info["trust_domain"] = temp
            break
    # If this is not checking a remote hosts config then check for signing key
    if not checking_remote_collector:
        # If custom signing key file specified then likely that tokens were issued from this host
        if param["SEC_TOKEN_POOL_SIGNING_KEY_FILE"] != "/etc/condor/passwords.d/POOL":
            ret_info["has_signing_key"] = True
        else:
            # Check password directory for files
            # Note this could return a false positive because of a pool password file
            password_dir = param.get("SEC_PASSWORD_DIRECTORY")
            if password_dir != None:
                secure_files = []
                try:
                    secure_files = os.listdir(password_dir)
                except FileNotFoundError:
                    pass
                if len(secure_files) > 0:
                    ret_info["has_signing_key"] = True
    return ret_info


def get_token_issuers():
    """Read this hosts stored IDTokens and parse out the issuer field"""
    # Return dict = {issuer:Number tokens issued}
    issuers = {}
    # Run condor_token_list
    p = subprocess.run(["condor_token_list"], stdout=subprocess.PIPE)
    stored_tokens = p.stdout.rstrip().decode()
    # Parse each token line for issuer field
    for token in stored_tokens.split("\n"):
        token = token.strip()
        if token == "":
            continue
        iss_pos = token.find('"iss":')
        end_pos = token.find(",", iss_pos)
        issuer = token[iss_pos + 5 : end_pos].strip('":')
        if issuer in issuers:
            issuers[issuer] += 1
        else:
            issuers.update({issuer: 1})
    if len(issuers) > 0:
        return issuers
    return None


def check_idtokens(is_ce):
    """Check IDToken information for possible breakages due to V9 to V10 changes"""
    format_print("Checking IDTokens:")
    # The original default value for TRUST_DOMAIN
    v9_default_value = htcondor.param.get("COLLECTOR_HOST")
    # Daemon list to determin if this host is running collector
    daemon_list = htcondor.param["daemon_list"]
    # Get this hosts IDToken configuration infor
    my_config_info = get_token_configuration(htcondor.param, is_ce, False)
    # No token auth means extremely unlikely system will have problems wiht upgrade
    if not my_config_info["using_token_auth"]:
        format_print(
            "*** Not using token authentication. Should be unaffected by upgrade ***", 8
        )
        return
    # Get this hosts stored IDToken issuers
    my_token_issuers = get_token_issuers()
    remote_config_info = None
    # If local collector is not on this host then remote query config
    if "collector" not in daemon_list.lower():
        location_ad = htcondor.Collector().locate(htcondor.DaemonTypes.Collector)
        remote_config_info = get_token_configuration(
            htcondor.RemoteParam(location_ad), is_ce, True
        )
    # Digest collected information
    format_print("Diagnosis:", 4)
    # Look at local host TRUST_DOMAIN information
    trust_domain = my_config_info["trust_domain"]
    td_line = "Has non-default TRUST_DOMAIN ({}) set.".format(trust_domain["value"])
    if trust_domain["is_default"]:
        td_line = "Using default TRUST_DOMAIN '{}'.".format(trust_domain["unexpanded"])
    version_line = (
        "HTCONDOR-CE" if is_ce else "HTCONDOR V{}".format(my_config_info["version"])
    )
    format_print("This Host: {} | {}".format(version_line, td_line), 8)
    # Determine if this host has issued IDTokens by checking if signing key exists
    if my_config_info["has_signing_key"]:
        format_print(
            "Found possible IDToken signing key. Likely that tokens have been issued.",
            19,
        )
    # Does this host have any stored issued IDTokens
    if my_token_issuers == None:
        format_print("Did not find any stored IDTokens on host.", 19)
    else:
        format_print(
            "Found {} IDTokens with the following issuers:".format(
                len(my_token_issuers)
            ),
            19,
        )
        for key, value in my_token_issuers.items():
            format_print("->'{}' used for {} tokens.".format(key, value), 21)
    # Analyze local collector host information
    if remote_config_info != None and remote_config_info["using_token_auth"]:
        trust_domain = remote_config_info["trust_domain"]
        td_line = "Has non-default TRUST_DOMAIN ({}) set.".format(trust_domain["value"])
        if trust_domain["is_default"]:
            td_line = "Using default TRUST_DOMAIN '{}'.".format(
                trust_domain["unexpanded"]
            )
        format_print(
            "Local Collector: HTCONDOR V{} | {}".format(
                remote_config_info["version"], td_line
            ),
            8,
        )

    # Do our best to output findings
    if not my_config_info["has_signing_key"] and my_token_issuers == None:
        format_print(
            "*** HTCondor system should be unaffected by V9 to V10 upgrade IDToken default changes ***",
            8,
            True,
        )
        return
    elif not my_config_info["trust_domain"]["is_default"]:
        if (
            remote_config_info != None
            and remote_config_info["trust_domain"]["is_default"]
        ):
            pass
        else:
            format_print(
                "*** HTCondor system should be unaffected by V9 to V10 upgrade IDToken default changes ***",
                8,
                True,
            )
            return
    format_print(
        "*** HTCondor system possibly affected by V9 to V10 upgrade IDToken default changes ***",
        8,
        True,
    )
    format_print(
        "IDToken authentication may fail if in use. If tokens have been created and", 8
    )
    format_print("distributed consider doing the following. Possible Solutions:", 8)
    set_trust_domain = (
        "TRUST_DOMAIN to local collectors value in{}tokens issuers field".format(
            "\n".ljust(13, " ")
        )
    )
    if my_token_issuers != None and v9_default_value in my_token_issuers:
        set_trust_domain = "'TRUST_DOMAIN = {}'".format(v9_default_value)
    format_print(
        "1. To keep using existing distributed tokens set {} on all hosts in the pool and reconfig.".format(
            set_trust_domain
        ),
        12,
    )
    format_print(
        "2. Or re-issue all tokens after hosts have been upgraded or TRUST_DOMAIN is explicilty set in configuration.",
        12,
    )
    format_print(
        "Note: Any IDTokens issued from other collectors used for flocking will likely need to be re-issued",
        12,
    )
    format_print("      once those hosts HTCondor system is upgraded to V10.", 12)


def read_map_file(filename, is_el7, has_cmd):
    """Digest a map file and attempt to digest regex sequences in file"""
    # Try to read map file
    try:
        fd = open(filename, "r")
        line_num = 0
        for line in fd:
            line_num += 1
            line = line.strip()
            # Skip empty and comment lines
            if line == "" or line[0] == "#":
                continue
            # Digest @include line to other map files
            elif line[0:8] == "@include":
                path = line.split()[1].strip()
                # If include file then read it
                if os.path.isfile(path):
                    read_map_file(path, is_el7, has_cmd)
                # If directory then read all files in directory
                elif os.path.isdir(path):
                    for map_file in os.listdir(path):
                        full_path = os.path.join(path, map_file)
                        read_map_file(full_path, is_el7, has_cmd)
            elif has_cmd:
                # If we found 'pcre2grep' cmd then find regex sequence and try to use it
                start = line.find(" ") + 1
                length = line.rfind(" ")
                sequence = '"{}"'.format(line[start:length])
                p = subprocess.run(
                    ["pcre2grep", sequence, PCRE2_TEST_FILE], stderr=subprocess.PIPE
                )
                error = p.stderr.rstrip().decode()
                if len(error) > 0:
                    format_print(
                        "Invalid: {0}|line-{1}: '{2}'".format(filename, line_num, line),
                        12,
                    )
                    format_print("Error: {}".format(error), 14)
            elif is_el7:
                # No 'pcre2grep' cmd found so if EL7 & manually check for known incompatibility
                for char in PCRE2_POSIX_CHARS:
                    if char in line:
                        pos = line.find(char)
                        if line[pos + len(char)] == "-":
                            invalid = True
                            format_print(
                                "Invalid: {0}|line-{1}: '{2}'".format(
                                    filename, line_num, line
                                ),
                                12,
                            )
                            break
        fd.close()
    except IOError:
        format_print("Failed to open {}".format(filename), 12)


def check_pcre2():
    """Check map files for known PCRE2 breakages"""
    is_el7 = False
    has_pcre2_cmd = False
    # Check if platform is EL7
    if platform.system() == "Linux" and "el7" in platform.release():
        is_el7 = True
    format_print("Checking for incompatibilities with PCRE2.", 0, True)
    format_print("Known PCRE2 incompatibilities:", 8)
    format_print("1. Hyphens in a regex character sequence must occur at the end.", 16)
    if is_el7:
        format_print(
            "2. On EL7 posix name sets ([:space:]) followed by hyphen are invalid.", 16
        )
    # Check for 'pcre2grep' cmd
    try:
        p = subprocess.run(["pcre2grep"], stderr=subprocess.PIPE)
        has_pcre2_cmd = True
    except FileNotFoundError:
        if is_el7:
            format_print(
                "Failed to find 'pcre2grep' command. Only checking for known issue #2.",
                8,
            )
        else:
            format_print(
                "Failed to find 'pcre2grep' command. Unable to check Map files for incompatibilties.",
                8,
            )
            return
    # Temporary file to test pcre2grep against
    if has_pcre2_cmd:
        with open(PCRE2_TEST_FILE, "w") as f:
            f.write("HTCondor is cool")
    # Get regular user map files and digest them
    format_print("Reading CLASSAD_USER_MAPFILE_* files...", 8)
    for key in htcondor.param.keys():
        if key[0:21] == "CLASSAD_USER_MAPFILE_":
            filename = htcondor.param[key]
            read_map_file(filename, is_el7, has_pcre2_cmd)
    # Get certificate map file and digest it (plus any @include files)
    map_cert_file = htcondor.param.get("CERTIFICATE_MAPFILE")
    if map_cert_file != None:
        format_print("Reading {}...".format(map_cert_file), 8)
        read_map_file(map_cert_file, is_el7, has_pcre2_cmd)
    if has_pcre2_cmd and os.path.exists(PCRE2_TEST_FILE):
        os.remove(PCRE2_TEST_FILE)


def process_ad(ad):
    """Digest job ad for known keywords"""
    owner = "UNKNOWN"
    if "Owner" in ad:
        for var in ad:
            if var[0:4].lower() == "cuda" or var[0:11].lower() == "target.cuda":
                owner = ad["Owner"]
                break
    return owner


def check_gpu_requirements():
    """Check job ads for known pre V10 GPU requesting"""
    return
    # No schedd Daemon then no check needed
    if "SCHEDD" not in htcondor.param["daemon_list"]:
        return
    format_print("Checking GPU resource matching:", 0, True)
    # Try to locate schedd
    try:
        ppl_using_gpu = []
        schedd = htcondor.Schedd()
        # Check jobs currently running in queue
        format_print("Checking current schedd queue job ads.", 8)
        queue = schedd.xquery()
        for ad in queue:
            ad_owner = process_ad(ad)
            if ad_owner != "UNKNOWN" and ad_owner not in ppl_using_gpu:
                ppl_using_gpu.append(ad_owner)
        # Check a bit of the history too
        format_print("Checking last 5000 job ads in history.", 8)
        history = schedd.history(None, [], 5000)
        for ad in history:
            ad_owner = process_ad(ad)
            if ad_owner != "UNKNOWN" and ad_owner not in ppl_using_gpu:
                ppl_using_gpu.append(ad_owner)
        # If we found jobs with known keywords inform
        if len(ppl_using_gpu) > 0:
            format_print(
                "There are currently {} user(s) using V9 GPU requirements specification.".format(
                    len(ppl_using_gpu)
                ),
                8,
            )
            format_print(
                "The method to request GPUs changes in V10 to use the new submit description keywords:",
                8,
            )
            format_print("- request_gpus", 16)
            format_print("- require_gpus", 16)
            format_print(
                "The following user(s) were found during the query for the old style of requesting GPUs:",
                8,
                True,
            )
            for user in ppl_using_gpu:
                format_print("{}".format(user), 16)
    except htcondor.HTCondorLocateError:
        format_print("Failed to locate local schedd.", 8)


def main():
    # Make sure script is running as root because we need to run 'condor_token_list'
    if os.geteuid() != 0:
        format_print("Error: Script not ran under 'sudo'")
        sys.exit(1)
    # Systems with config to check: Base condor and condor-ce
    ecosystems = ["HTCondor"]
    # Check if passed -ce flag to indicate this host is a CE
    for arg in sys.argv:
        arg = arg.lower().strip()
        if arg == "-ce" and "HTCondor-CE" not in ecosystems:
            ecosystems.append("HTCondor-CE")
    # Check for incompatibilities
    for system in ecosystems:
        format_print(
            "----- Checking V9 to V10 incompatibilities for {} -----".format(system),
            16,
            True,
        )
        # If CE then digest CE configuration
        if system == "HTCondor-CE":
            os.environ["CONDOR_CONFIG"] = "/etc/condor-ce/condor_config"
            htcondor.reload_config()
        check_idtokens(system == "HTCondor-CE")
        check_pcre2()
        # Only worry about GPU incompatibility if not CE
        if system != "HTCondor-CE":
            check_gpu_requirements()
    # Final information to direct people to help
    format_print(
        "For more information reagrding incompatibilities visit: https://htcondor.readthedocs.io/en/latest/version-history/upgrading-from-9-0-to-10-0-versions.html#upgrading-from-an-9-0-lts-version-to-an-10-0-lts-version-of-htcondor",
        0,
        True,
    )
    format_print(
        "To ask any questions regarding incompatibilities email: htcondor-users@cs.wisc.edu",
        0,
        True,
    )


if __name__ == "__main__":
    main()
