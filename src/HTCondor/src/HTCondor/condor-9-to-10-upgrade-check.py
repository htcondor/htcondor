#!/usr/bin/python3
# HTCondor V9 to V10 check for incompatibilites script
#   This is intended to check a current system before upgrade
#   for known 'gotcha' breaking changes:
#        1. IDToken TRUST_DOMAIN default value change
#        2. Upgrade to PCRE2 breaking map file regex secuences
#        3. The way to request GPU resources for a job
#   This script can still be ran post upgrade as a post mortem
#   Note: Requires root and HTCONDOR python bindings
import os
import sys
import json
import platform
import subprocess
import shlex
from tempfile import NamedTemporaryFile
from pathlib import Path

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


def format_print(msg, offset=0, newline=False, err=False):
    """Custom print function to help with output spacing"""
    f = sys.stderr if err else sys.stdout
    if newline:
        print(file=f)

    offset += len(msg)
    print(f"{msg:>{offset}s}", file=f)


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
        payload_start = token.find("Payload:") + 8
        payload_end = token.rfind("File:")
        payload = json.loads(token[payload_start : payload_end].strip())
        issuer = payload["iss"]
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
            "*** Not using token authentication. Should be unaffected by upgrade ***", offset=8
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
    format_print("Diagnosis:", offset=4)
    # Look at local host TRUST_DOMAIN information
    trust_domain = my_config_info["trust_domain"]
    td_line = f"Has non-default TRUST_DOMAIN ({trust_domain['value']}) set."
    if trust_domain["is_default"]:
        td_line = "Using default TRUST_DOMAIN '{trust_domain['unexpanded']}'."
    version_line = "HTCONDOR-CE" if is_ce else f"HTCONDOR V{my_config_info['version']}"
    format_print(f"This Host: {version_line} | {td_line}", offset=8)
    # Determine if this host has issued IDTokens by checking if signing key exists
    if my_config_info["has_signing_key"]:
        format_print(
            "Found possible IDToken signing key. Likely that tokens have been issued.",
            offset=19,
        )
    # Does this host have any stored issued IDTokens
    if my_token_issuers == None:
        format_print("Did not find any stored IDTokens on host.", offset=19)
    else:
        format_print(
            f"Found {len(my_token_issuers)} IDTokens with the following issuers:",
            offset=19,
        )
        for key, value in my_token_issuers.items():
            format_print(f"->'{key}' used for {value} tokens.", offset=21)
    # Analyze local collector host information
    if remote_config_info != None and remote_config_info["using_token_auth"]:
        trust_domain = remote_config_info["trust_domain"]
        td_line = f"Has non-default TRUST_DOMAIN ({trust_domain['value']}) set."
        if trust_domain["is_default"]:
            td_line = f"Using default TRUST_DOMAIN '{trust_domain['unexpanded']}'."
        format_print(
            f"Local Collector: HTCONDOR V{remote_config_info['version']} | {td_line}",
            offset=8,
        )

    # Do our best to output findings
    if not my_config_info["has_signing_key"] and my_token_issuers == None:
        format_print(
            "*** HTCondor system should be unaffected by V9 to V10 upgrade IDToken default changes ***",
            offset=8,
            newline=True,
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
                offset=8,
                newline=True,
            )
            return
    format_print(
        "*** HTCondor system possibly affected by V9 to V10 upgrade IDToken default changes ***",
        offset=8,
        newline=True,
    )
    format_print(
        "IDToken authentication may fail if in use. If tokens have been created and", offset=8
    )
    format_print("distributed consider doing the following. Possible Solutions:", offset=8)
    set_trust_domain = """TRUST_DOMAIN to local collectors value in
             tokens issuers field"""
    if my_token_issuers != None and v9_default_value in my_token_issuers:
        set_trust_domain = f"'TRUST_DOMAIN = {v9_default_value}'"
    format_print(
        f"1. To keep using existing distributed tokens set {set_trust_domain} on all hosts in the pool and reconfig.",
        offset=12,
    )
    format_print(
        "2. Or re-issue all tokens after hosts have been upgraded or TRUST_DOMAIN is explicilty set in configuration.",
        offset=12,
    )
    format_print(
        "Note: Any IDTokens issued from other collectors used for flocking will likely need to be re-issued",
        offset=12,
    )
    format_print("      once those hosts HTCondor system is upgraded to V10.", offset=12)


def read_map_file(filename, is_el7, has_cmd):
    """Digest a map file and attempt to digest regex sequences in file"""
    # Try to read map file
    try:
        with open(filename, "r") as fd:
            line_num = 0
            for line in fd:
                line_num += 1
                line = line.strip()
                # Skip empty and comment lines
                if (not line) or line.startswith("#"):
                    continue
                # Digest @include line to other map files
                elif line[0:8] == "@include":
                    include_path = Path(shlex.split(line)[1])
                    # If include file then read it
                    if include_path.is_file():
                        read_map_file(str(include_path), is_el7, has_cmd)
                    # If directory then read all files in directory
                    elif include_path.is_dir():
                        for sub_path in include_path.iterdir():
                            if sub_path.is_file():
                                read_map_file(str(sub_path), is_el7, has_cmd)
                # This is a normal map line so try to get the regex sequence
                try:
                    sequence = shlex.split(line)[1]
                except IndexError:
                    format_print(
                        f"WARNING: Did not find a regex on line {line_num} of {filename}:\n{line}",
                        err=True,
                    )
                    continue
                if has_cmd:
                    # If we found 'pcre2grep' cmd then try regex sequence with cmd
                    with NamedTemporaryFile("w") as tf:
                        tf.write("HTCondor is cool")
                        tf.flush()
                        os.fsync(tf.fileno())
                        p = subprocess.run(
                            ["pcre2grep", sequence, tf.name], stderr=subprocess.PIPE
                        )
                    error = p.stderr.rstrip().decode()
                    if len(error) > 0:
                        format_print(
                            f"Invalid: {filename}|line-{line_num}: '{line}'",
                            offset=12,
                        )
                        format_print(f"Error: {error}", offset=14)
                else:
                    # No 'pcre2grep' cmd so do a best effort check
                    bracket_cnt = 0
                    prev_char = None
                    pos = 0
                    # Manually check sequence line for hyphens in non common cases
                    for char in sequence:
                        if char == "[":
                            bracket_cnt += 1
                        elif char == "]":
                            bracket_cnt -= 1
                        elif char == "-" and bracket_cnt > 0:
                            # Ignore correct/common cases
                            if sequence[pos + 1] == "]":
                                # Hyphen at end of pcre statement
                                pass
                            elif prev_char == "A" and sequence[pos + 1] == "Z":
                                # A-Z
                                pass
                            elif prev_char == "a" and sequence[pos + 1] == "z":
                                # a-z
                                pass
                            elif prev_char == "0" and sequence[pos + 1] == "9":
                                # 0-9
                                pass
                            else:
                                # Not a normal case so output
                                format_print(
                                    f"Possibly Invalid: {filename}|line-{line_num}: '{line}'",
                                    offset=12,
                                )
                                break
                        prev_char = char
                        pos += 1
                if not has_cmd and is_el7:
                    # No 'pcre2grep' cmd and EL7 so manually check for known incompatibility
                    for char in PCRE2_POSIX_CHARS:
                        if char in line:
                            pos = line.find(char)
                            if line[pos + len(char)] == "-":
                                invalid = True
                                format_print(
                                    f"Invalid: {filename}|line-{line_num}: '{line}'",
                                    offset=12,
                                )
                                break
    except IOError:
        format_print(f"ERROR: Failed to open {filename}", offset=12, err=True)


def check_pcre2():
    """Check map files for known PCRE2 breakages"""
    is_el7 = False
    has_pcre2_cmd = False
    # Check if platform is EL7
    if platform.system() == "Linux" and "el7" in platform.release():
        is_el7 = True
    format_print("Checking for incompatibilities with PCRE2.", newline=True)
    format_print("Known PCRE2 incompatibilities:", offset=8)
    format_print("1. Hyphens in a regex character sequence must occur at the end.", offset=16)
    if is_el7:
        format_print(
            "2. On EL7 posix name sets ([:space:]) followed by hyphen are invalid.", offset=16
        )
    # Check for 'pcre2grep' cmd
    try:
        p = subprocess.run(["pcre2grep"], stderr=subprocess.PIPE)
        has_pcre2_cmd = True
    except FileNotFoundError:
        format_print(
            """        Failed to find 'pcre2grep' command. Will do a simple check for incompatibilties.
        Recommended to install 'pcre2grep' command for better check of map files.""",
            err=True,
        )
    # Get regular user map files and digest them
    format_print("Reading CLASSAD_USER_MAPFILE_* files...", offset=8)
    for key in htcondor.param.keys():
        if key[0:21] == "CLASSAD_USER_MAPFILE_":
            filename = htcondor.param[key]
            read_map_file(filename, is_el7, has_pcre2_cmd)
    # Get certificate map file and digest it (plus any @include files)
    map_cert_file = htcondor.param.get("CERTIFICATE_MAPFILE")
    if map_cert_file != None:
        format_print(f"Reading {map_cert_file}...", offset=8)
        read_map_file(map_cert_file, is_el7, has_pcre2_cmd)


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
    # No schedd Daemon then no check needed
    if "SCHEDD" not in htcondor.param["daemon_list"]:
        return
    format_print("Checking GPU resource matching:", newline=True)
    # Try to locate schedd
    try:
        ppl_using_gpu = []
        schedd = htcondor.Schedd()
        # Check jobs currently running in queue
        format_print("Checking current schedd queue job ads.", offset=8)
        queue = schedd.xquery()
        for ad in queue:
            ad_owner = process_ad(ad)
            if ad_owner != "UNKNOWN" and ad_owner not in ppl_using_gpu:
                ppl_using_gpu.append(ad_owner)
        # Check a bit of the history too
        format_print("Checking last 5000 job ads in history.", offset=8)
        history = schedd.history(None, [], 5000)
        for ad in history:
            ad_owner = process_ad(ad)
            if ad_owner != "UNKNOWN" and ad_owner not in ppl_using_gpu:
                ppl_using_gpu.append(ad_owner)
        # If we found jobs with known keywords inform
        if len(ppl_using_gpu) > 0:
            format_print(
                f"There are currently {len(ppl_using_gpu)} user(s) using V9 GPU requirements specification.",
                offset=8,
            )
            format_print(
                "The method to request GPUs changes in V10 to use the new submit description keywords:",
                offset=8,
            )
            format_print("- request_gpus", offset=16)
            format_print("- require_gpus", offset=16)
            format_print(
                "The following user(s) were found during the query for the old style of requesting GPUs:",
                offset=8,
                newline=True,
            )
            for user in ppl_using_gpu:
                format_print(user, offset=16)
    except htcondor.HTCondorLocateError:
        format_print("Failed to locate local schedd.", offset=8, err=True)


def usage():
    """Print Script Usage"""
    format_print("HTCondor Upgrade Incompatabilities Script", newline=True)
    format_print(
        """  This Script is to help assist catching possible issues that
  may occur while upgrading an HTCondor system from
  version 9 to version 10.""")
    format_print("Options:", newline=True, offset=8)
    format_print("-h/-help         Display Usage", offset=12)
    format_print("-ce              This host is an HTCondor-CE", offset=12)
    format_print("-ignore-gpu      Skip checking GPU requirements incompatability", offset=12)
    sys.exit(0)


def main():
    # Systems with config to check: Base condor and condor-ce
    ecosystems = ["HTCondor"]
    check_gpu_reqs = True
    # Check if passed -ce flag to indicate this host is a CE
    for arg in sys.argv[1:]:
        arg = arg.lower().strip()
        if arg == "-ce" and "HTCondor-CE" not in ecosystems:
            ecosystems.append("HTCondor-CE")
        elif arg == "-ignore-gpu":
            check_gpu_reqs = False
        elif arg == "-h" or arg == "-help":
            usage()
    # Make sure script is running as root because we need to run 'condor_token_list'
    if os.geteuid() != 0:
        format_print("Error: Script not ran as root", err=True)
        sys.exit(1)
    # Check for incompatibilities
    for system in ecosystems:
        format_print(
            f"----- Checking V9 to V10 incompatibilities for {system} -----",
            offset=16,
            newline=True,
        )
        # If CE then digest CE configuration
        if system == "HTCondor-CE":
            os.environ["CONDOR_CONFIG"] = "/etc/condor-ce/condor_config"
            htcondor.reload_config()
        check_idtokens(system == "HTCondor-CE")
        check_pcre2()
        # Only worry about GPU incompatibility if not CE
        if system != "HTCondor-CE" and check_gpu_reqs:
            check_gpu_requirements()
    # Final information to direct people to help
    format_print(
        """For more information reagrding incompatibilities visit:
  https://htcondor.readthedocs.io/en/v10_0/version-history/upgrading-from-9-0-to-10-0-versions.html#upgrading-from-an-9-0-lts-version-to-an-10-0-lts-version-of-htcondor""",
        newline=True,
    )
    format_print(
        """To ask any questions regarding incompatibilities email:
  htcondor-users@cs.wisc.edu""",
        newline=True,
    )


if __name__ == "__main__":
    main()
