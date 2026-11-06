#!/usr/bin/env python3

import os
import sys
import time
from pathlib import Path
import ast  # parsing dictionary from command line argument
import json
from authlib.integrations.requests_client import OAuth2Session
from authlib.oauth2.rfc7523 import PrivateKeyJWT
import urllib
import requests
import logging

# Default settings
token_url = "https://oidc.nersc.gov/c2id/token"
key_path = Path.home() / ".condor" / "irikey.pem"

# IRI service
iri_host = "api.iri.nersc.gov"
iri_storage_rsc = "scratch"
iri_compute_rsc = "compute"
iri_storage_id = None
iri_compute_id = None
iri_service = f"{iri_host},{iri_compute_rsc},{iri_storage_rsc}"

# Set the env variable before invoking the python helper
# Format: logging_level;log_secure;log_file
# Set log_secure to True/true to log also credentials
IRI_LOGGING = os.getenv("BLAH_HELPER_LOGGING", "INFO;False;/dev/null")
log_level, LOG_SECURE, log_file = IRI_LOGGING.split(";", 2)
log_level = log_level.upper()
LOG_SECURE = LOG_SECURE.lower() == "true"
logging.basicConfig(filename=log_file, level=log_level)

# Aux functions
def log_secure(msg: str)  -> str:
    if LOG_SECURE:
        return msg
    return ""


class IriApiHelperError(Exception):
    """Raised for errors encountered in iri_helpers operations."""
    pass


def deep_merge(dict1, dict2):
    """
    Recursively merges dict2 into a copy of dict1.
    """
    # Create a copy so we don't mutate the original dictionary
    result = dict1.copy()
    
    for key, value in dict2.items():
        # If both values are dictionaries, merge them recursively
        if key in result and isinstance(result[key], dict) and isinstance(value, dict):
            result[key] = deep_merge(result[key], value)
        else:
            # Otherwise, overwrite or add the value from dict2
            result[key] = value
            
    return result

# This is specific to NERSC
def get_access_token():
    if hasattr(get_access_token, "access_token"):
        # TODO: check also for validity. Add token caching in a file
        logging.debug(f"returning available access token{log_secure(f' = {get_access_token.access_token}')}")
        return get_access_token.access_token
    logging.debug(f"Requesting new access token")
    logging.debug(f"token_url = {token_url}")
    logging.debug(f"key_path = {key_path}")
    # TODO: handle failures (missing file, no reply, ...)
    with Path(key_path).open() as secret:
        priv_key = secret.read()
        client_id = priv_key.split("\n")[0]
        logging.debug(f"read private key for {client_id}")
        if LOG_SECURE:
            logging.debug(priv_key)
    session = OAuth2Session(client_id, priv_key, PrivateKeyJWT(token_url), grant_type="client_credentials", token_endpoint=token_url)
    token = session.fetch_token()
    logging.debug(f"retrieved token{log_secure(f' = {token}')}")
    get_access_token.access_token = token["access_token"]
    logging.debug(f"new access token{log_secure(f' = {get_access_token.access_token}')}")
    return get_access_token.access_token

def do_request(method, url_path, req_body=None, req_filename=None):
    logging.debug(f"Doing {method} {url_path}")
    access_token = get_access_token()
    url = f"https://{iri_host}{url_path}"
    headers = {"accept": "application/json", "Authorization": f"Bearer {access_token}"}
    payload_files = None
    if req_filename is not None:
        payload_files = {"file": open(req_filename, "rb")}
    resp=requests.request(method, url, headers=headers, data=req_body, files=payload_files)
    #logging.info(f"{resp.status_code} {resp.reason}")
    resp_body = resp.text
    #logging.info(resp_body)
    if resp.status_code < 200 or resp.status_code > 299:
        logging.warning(f"Request {method} {url_path} failed: {resp.status_code} {resp.reason}")
        return None

    body_json = ""
    if resp_body:
        body_json = json.loads(resp_body)

    if isinstance(body_json, dict) and "task_id" in body_json:
        #logging.info(body_json["task_id"])
        task_id = body_json["task_id"]
        task_status = ""
        task_resp = None
        while task_status != "completed":
            task_resp = requests.request("GET", f"https://{iri_host}/api/v1/task/{task_id}", headers=headers)
            task_resp_body = task_resp.text
            #logging.info(f"{task_resp.status_code} {task_resp.reason}")
            #logging.info(task_resp_body)
            if task_resp.status_code < 200 or task_resp.status_code > 299:
                logging.warning("Task {method} {url_path} failed: {task_resp.status_code} {task_resp.reason}")
                task_status = "completed"
                return None
            body_json = json.loads(task_resp_body)
            task_status = body_json["status"]
            if task_status == "completed":
                return body_json["result"]["output"]
            if task_status == "failed":
                text = body_json["result"]["error"]
                logging.warning(f"Task {method} {url_path} failed: {text}")
                return None
            time.sleep(1)
    logging.debug("Done")
    return body_json

def get_resource_id(name):
    url_path = f"/api/v1/status/resources"
    if name != "":
        url_path += f"?name={name}"
    resp = do_request("GET", url_path)
    return resp[0]["id"]

def get_storage_id():
    global iri_storage_id
    if iri_storage_id is None:
        iri_storage_id = get_resource_id(iri_storage_rsc)
    return iri_storage_id

def get_compute_id():
    global iri_compute_id
    if iri_compute_id is None:
        iri_compute_id = get_resource_id(iri_compute_rsc)
    return iri_compute_id

def cmd_resource_status(name):
    # logging.debug(f"IRI name to cmd_resource_status = {name}")
    quoted_name = urllib.parse.quote(name, safe="")
    url_path = f"/api/v1/status/resources?name={quoted_name}"
    resp = do_request("GET", url_path)
    logging.debug(f"IRI resource status ({resp[0]['current_status']}) = {resp}")
    print(resp[0]["current_status"])

def cmd_download(remote_path, local_path):
    quoted_path = urllib.parse.quote(remote_path, safe="")
    resource_id = get_storage_id()
    url_path = f"/api/v1/filesystem/download/{resource_id}?path={quoted_path}"
    resp = do_request("GET", url_path)
    with open(local_path, "w") as file:
        file.write(resp)

def cmd_upload(local_path, remote_path):
    logging.info(f"cmd_upload({local_path, remote_path})")
    quoted_path = urllib.parse.quote(remote_path, safe="")
    # with open(local_path, "rb") as file:
    #     req_body = file.read()
    resource_id = get_storage_id()
    url_path = f"/api/v1/filesystem/upload/{resource_id}?path={quoted_path}"
    resp = do_request("POST", url_path, req_filename=local_path)
    logging.info(resp)

def cmd_chmod(new_mode, remote_path):
    logging.info(f"cmd_chmod({new_mode}, {remote_path})")
    req_body = json.dumps({"mode": new_mode, "path": remote_path})
    resource_id = get_storage_id()
    url_path = f"/api/v1/filesystem/chmod/{resource_id}"
    resp = do_request("PUT", url_path, req_body)
    logging.info(resp)

def cmd_mkdir(remote_path):
    logging.info(f"cmd_mkdir({remote_path})")
    resource_id = get_storage_id()
    url_path = f"/api/v1/filesystem/mkdir/{resource_id}"
    req_body = json.dumps({"parent": "true", "path": remote_path})
    resp = do_request("POST", url_path, req_body)
    logging.info(resp)

def cmd_rmdir(remote_path):
    quoted_path = urllib.parse.quote(remote_path, safe="")
    resource_id = get_storage_id()
    url_path = f"/api/v1/filesystem/rm/{resource_id}?path={quoted_path}"
    resp = do_request("DELETE", url_path)
    logging.info(resp)

def cmd_submit(job_name, base_dir, inputs, script, xtra_args):
    logging.debug(f"IRI cmd_submit method: {job_name}, {base_dir}, {inputs}, {script}, {xtra_args}")
    job_dir = f"{base_dir}/{job_name}"
    logging.debug(f"job_dir = {job_dir}")
    # Arguments come from command line arguments and are all strings
    if xtra_args:
        try:
            # JSON would be safer, but the syntax is more restricted (double quotes)
            xtra_args = ast.literal_eval(xtra_args)
            if not isinstance(xtra_args, dict):
                raise TypeError("xtra_args must be a dict")
        except:
            logging.warning(f"IRI cmd_submit method: xtra_args is not a valid dictionary: {xtra_args}")
            xtra_args = {}

    storage_id = get_storage_id()
    logging.debug(f"storage_id = {storage_id}")
    cmd_mkdir(job_dir)

    input_files = [f.strip() for f in inputs.split(",") if f.strip()]
    input_files.append(script)
    logging.debug(f"input_files = {input_files}")
    for file in input_files:
        # logging.debug(f"file = {file}")
        basename = file.split("/")[-1]
        cmd_upload(file, f"{job_dir}/{basename}")

    basename = script.split("/")[-1]
    remote_cmd = f"{job_dir}/{basename}"
    remote_stdout = f"{job_dir}/iri-job.out"
    remote_stderr = f"{job_dir}/iri-job.err"

    cmd_chmod("755", remote_cmd)

    compute_id = get_compute_id()
    req_body = {
        "name": job_name,
        "executable": remote_cmd,
        "directory": job_dir,
        "inherit_environment": True,
        "stdin_path": "/dev/null",
        "stdout_path": remote_stdout,
        "stderr_path": remote_stderr,
        "resources": {
            "node_count": 1
        },
        "attributes": {
            "duration": 30
        },
    }
    #req_body["attributes"]["custom_attributes"] = { "constraint": "cpu" }
    if xtra_args:
        req_body = deep_merge(req_body, xtra_args)
    req_json = json.dumps(req_body)
    logging.info(f"request json: {req_json}")
    url_path = f"/api/v1/compute/job/{compute_id}"
    resp = do_request("POST", url_path, req_json)
    logging.info(resp)
    job_id = resp["id"]
    logging.debug(f"IRI submit: IRI_RESULT:{job_id}:{job_dir}:{remote_stdout}:{remote_stderr}")
    print(f"IRI_RESULT:{job_id}:{job_dir}:{remote_stdout}:{remote_stderr}")

def cmd_status(job_id):
    compute_id = get_compute_id()
    url_path = f"/api/v1/compute/status/{compute_id}/{job_id}"
    resp = do_request("GET", url_path)
    logging.info(resp)
    # TODO print addl info (exit code, resource usage)
    state=resp["status"]["state"]
    exit_code=resp["status"]["exit_code"]
    # logging.debug(f"IRI  status: {state} ExitCode={exit_code};Foo=true")
    print(f"{state} ExitCode={exit_code};Foo=true")

def cmd_cancel(job_id):
    compute_id = get_compute_id()
    url_path = f"/api/v1/compute/cancel/{compute_id}/{job_id}"
    resp = do_request("DELETE", url_path)
    logging.info(resp)
    #TODO delete job dir?

def cmd_download_output(job_id):
    jobstate_file = Path.home() / ".blah" / "iri_jobs" / f"{iri_service}_{job_id}"
    if not jobstate_file.exists():
        raise FileNotFoundError(f"Job state file not found: {jobstate_file}")

    for line in jobstate_file.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#") or '::' not in line:
            continue
        # Split into type tag and the path pair
        type, paths = line.split('::', 1)
        if type == "remote_dir":
            continue

        # Both paths are absolute so splitting on the first ':' is unambiguous
        local_path, remote_path = paths.split(':', 1)
        logging.info(f"Downloading {remote_path} -> {local_path}")
        cmd_download(remote_path, local_path)

def cmd_delete_job_dir(job_id):
    jobstate_file = Path.home() / ".blah" / "iri_jobs" / f"{iri_service}_{job_id}"
    if not jobstate_file.exists():
        raise FileNotFoundError(f"Job state file not found: {jobstate_file}")

    remote_dir = None
    for line in jobstate_file.read_text().splitlines():
        line = line.strip()
        if line.startswith("remote_dir::"):
            _, remote_dir = line.split('::', 1)
            remote_dir = remote_dir.strip()
            break

    if not remote_dir:
        raise IriApiHelperError(f"No remote_dir entry found in jobstate file: {jobstate_file}")

    cmd_rmdir(remote_dir)
    logging.info(f"Deleted remote directory {remote_dir}")

def main():
    if len(sys.argv) < 3:
        print("Usage: iri.py <cmd> <arg> [<other_args>]")
        return 1
    logging.debug(f"Invoked IRI helper: {sys.argv}")

    service = sys.argv[1]
    cmd = sys.argv[2]

    if service:
        global iri_service
        global iri_host
        global iri_compute_rsc
        global iri_storage_rsc
        iri_service = service
        try:
            iri_host, iri_compute_rsc, iri_storage_rsc = service.split(",")
        except ValueError:
            print(f"Wrong service definition (should have 3 comma separated elements): {service}")
            return 1

    if cmd == "resource-id":
        id = get_resource_id(sys.argv[3])
        # logging.debug(f"IRI resource-id: {id}")
        print(id)
    elif cmd == "resource-status":
        cmd_resource_status(sys.argv[3])
    elif cmd == "download":
        cmd_download(sys.argv[3], sys.argv[4])
    elif cmd == "upload":
        cmd_upload(sys.argv[3], sys.argv[4])
    elif cmd == "chmod":
        cmd_chmod(sys.argv[3], sys.argv[4])
    elif cmd == "mkdir":
        cmd_mkdir(sys.argv[3])
    elif cmd == "rmdir":
        cmd_rmdir(sys.argv[3])
    elif cmd == "get_resource_id":
        print(get_resource_id(sys.argv[3]))
        # gr_id = get_resource_id(sys.argv[3])
        # logging.debug(f"IRI get-resource-id: {gr_id}")
        # print(f"{gr_id}")
    elif cmd == "submit":
        cmd_submit(sys.argv[3], sys.argv[4], sys.argv[5], sys.argv[6], sys.argv[7])
    elif cmd == "status":
        cmd_status(sys.argv[3])
    elif cmd == "cancel":
        cmd_cancel(sys.argv[3])
    elif cmd == "download-output":
        cmd_download_output(sys.argv[3])
    elif cmd == "delete-job-dir":
        cmd_delete_job_dir(sys.argv[3])
    else:
        print("Unknown command")
        return 1
    return 0

if __name__ == "__main__":
    sys.exit(main())
