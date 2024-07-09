import sys
from pathlib import Path
import datetime
from collections import defaultdict

import htcondor2 as htcondor
import classad2 as classad

from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb


def compactify_job_list(jobs):
    if jobs is None:
        return ""

    list = ""
    for clusterID in sorted(jobs.keys()):
        ordered = sorted(jobs[clusterID])

        # This whole thing needs some help.
        if len(ordered) == 1:
            list += f"{clusterID}.{ordered[0]}; "
        elif len(ordered) == 2:
            list += f"{clusterID}.{ordered[0]},{ordered[1]}; "
        else:
            list += f"{clusterID}."

            last = None
            first = None
            index = 0

            while index < len(ordered):
                if index + 1 == len(ordered):
                    last = ordered[index]
                    next = None
                else:
                    next = ordered[index + 1]

                if ordered[index] + 1 == next:
                    if first is None:
                        first = ordered[index]
                    last = ordered[index + 1]
                else:
                    if first is None:
                        list += f"{ordered[index]},"
                    else:
                        list += f"[{first}-{last}],"
                        first = None
                        last = None
                index += 1

            list = list[:-1]
            list += "; "

    return list[:-2]


class ListAll(Verb):
    def __init__(self, logger, **options):
        # SEC_PASSWORD_DIRECTORY is probably not of interest.
        # We'll save SEC_CREDENTIAL_DIRECTORY_KRB for later.

        # SEC_CREDENTIAL_DIRECTORY_OAUTH
        credentials_dir_path = htcondor.param.get("SEC_CREDENTIAL_DIRECTORY_OAUTH")
        if credentials_dir_path is None:
            return
        credentials_dir = Path(credentials_dir_path)

        credentials_by_user = defaultdict(dict)
        for entry in credentials_dir.iterdir():
            if not entry.is_dir():
                continue

            for file in entry.iterdir():
                if file.suffix != ".top":
                    continue
                credentials_by_user[entry.name][file.stem] = {
                    'refresh':  int(file.stat().st_mtime),
                    'jobs':     defaultdict(list),
                }

        # We're assuming that the unqualified user names on disk are
        # the owner names in the queue, but that's not necessarily
        # true; at some point, we'll have to update the credd/credmon.
        schedd = htcondor.Schedd()
        results = schedd.query(
            constraint=f'OAuthServicesNeeded =!= undefined',
            projection=['Owner', 'OAuthServicesNeeded', 'ClusterID', 'ProcID']
        )
        for result in results:
            clusterID = result['ClusterID']
            procID = result['ProcID']
            owner = result['Owner']
            service_names = result['OAuthServicesNeeded']
            for service_name in service_names.split(','):
                service_name = service_name.replace('*', '_')
                # In the wild, not all values in OAuthServicesNeeded are correctly
                # constructed (e.g., are space-separated), and the lookup may fail.
                credentials_by_service = credentials_by_user[owner].get(service_name)
                if credentials_by_service is not None:
                    credentials_by_service['jobs'][clusterID].append(procID)

        if len(credentials_by_user.keys()) == 0:
            print(f">> no OAuth credentials found")
            return

        longest_user = 4
        for user in credentials_by_user.keys():
            if len(user) > longest_user:
                longest_user = len(user)

        longest_file = 4
        for entry in credentials_by_user.values():
            for file in entry.keys():
                if len(file) + 4 > longest_file:
                    longest_file = len(file) + 4

        print(f">> Found the following OAuth2 credentials:")
        print(f"   {'User':<{longest_user}}  {'Service':<18}  {'Handle':<18}  {'Last Refreshed':>19}  {'File':<{longest_file}}  Jobs")

        for user in sorted(credentials_by_user.keys()):
            for name in credentials_by_user[user].keys():
                entry = credentials_by_user[user][name]
                (service, _, handle) = name.partition('_')
                refresh = str(datetime.datetime.fromtimestamp(entry['refresh']))
                jobs = compactify_job_list(entry.get('jobs'))

                file = f"{name}.use"

                print(f"   {user:<{longest_user}}  {service:<18}  {handle:<18}  {refresh:>19}  {file:<{longest_file}}  {jobs}")


class List(Verb):
    # For now, this only works for the authenticated identity and the
    # various different local daemons (depending on the command).
    def __init__(self, logger, **options):
        user = None
        credd = htcondor.Credd()

        # Check for a password credential.
        has_windows_password = False
        windows_password_time = None
        try:
            # This ignores the specified daemon and asks the schedd directly;
            # the schedd doesn't even register the corresponding command except
            # on Windows, so we get an exception instead of an answer.  Sigh.
            has_windows_password = credd.query_password(user)
            windows_password_time = credd.query_user_cred(htcondor.CredTypes.Password, user)
        except htcondor.HTCondorException:
            pass
        if windows_password_time is not None:
            date = datetime.datetime.fromtimestamp(windows_password_time)
            print(f">> Windows password timestamp {date}")
        else:
            print(f">> no Windows password found");

        # Check for Kerberos credential.
        kerberos_time = None
        try:
            # This throws an exception if the Kerberos credential directory
            # is misconfigured, which is super not-helpful, because if it's
            # configured but not active, the schedd blocks for ten minutes
            # on start-up.
            kerberos_time = credd.query_user_cred(htcondor.CredTypes.Kerberos, user)
        except htcondor.HTCondorException:
            pass
        if kerberos_time is not None:
            date = datetime.datetime.fromtimestamp(kerberos_time)
            print(f">> Kerberos timestamp {date}")
        else:
            print(f">> no Kerberos credential found")

        # Check for OAuth2 credentials.
        oauth_classad_string = credd.query_user_service_cred(htcondor.CredTypes.OAuth, None, None, user)

        if oauth_classad_string is None:
            print(f">> no OAuth credentials found")
            return

        ad = classad.parseOne(oauth_classad_string)
        # If we're talking to an older server, this could be None; we
        # handle that in the formatting step, below.
        fully_qualified_user = ad.get("fully_qualified_user")

        names = {}
        for key in ad.keys():
            name = None
            if key.endswith(".top"):
                name = key[:-4]
            elif key.endswith(".use"):
                name = key[:-4]

            if name is not None:
                names[name] = ad[key]

        fqu_string = ""
        if fully_qualified_user is not None:
            fqu_string = f" for '{fully_qualified_user}'"
        if len(names.keys()) == 0:
            print(f">> no OAuth credentials found{fqu_string}")
            return
        fqu_string += ':'

        #
        # Let's ask the queue for jobs that use these credentials.
        #
        jobs_by_name = defaultdict(lambda: defaultdict(list))
        if fully_qualified_user is not None:
            schedd = htcondor.Schedd()
            results = schedd.query(
                constraint=f'user == "{fully_qualified_user}" && OAuthServicesNeeded =!= undefined',
                projection=['OAuthServicesNeeded', 'ClusterID', 'ProcID']
            )
            for result in results:
                clusterID = result['ClusterID']
                procID = result['ProcID']
                service_names = result['OAuthServicesNeeded']
                for service_name in service_names.split(','):
                    service_name = service_name.replace('*', '_')
                    jobs_by_name[service_name][clusterID].append(procID)

        # Convert 'File' to a fixed-width column.
        longest_name = 4
        for name in names:
            if len(name) + 4 > longest_name:
                longest_name = len(name) + 4

        print(f">> Found OAuth2 credentials{fqu_string}")
        print(f"   {'Service':<18}  {'Handle':<18}  {'Last Refreshed':>19}  {'File':<{longest_name}}  Jobs")
        for name in names:
            (service, _, handle) = name.partition('_')
            date = datetime.datetime.fromtimestamp(names[name])
            jobs = compactify_job_list(jobs_by_name.get(name))
            name = f"{name}.use"
            print(f"   {service:<18}  {handle:<18}  {str(date):>19}  {name:<{longest_name}}  {jobs}")



class Add(Verb):
    def _handle_add_password(*, credential_file, ** options):
        user = None
        credd = htcondor.Credd()
        contents = Path(credential_file).read_text()

        # This can't work except for on Windows...
        credd.add_user_cred(htcondor.CredTypes.Password, contents, user)


    def _handle_add_kerberos(*, credential_file, ** options):
        user = None
        credd = htcondor.Credd()
        contents = Path(credential_file).read_text()

        credd.add_user_cred(htcondor.CredTypes.Kerberos, contents, user)


    def _handle_add_oauth2(*, credential_file, service, handle, ** options):
        user = None
        credd = htcondor.Credd()
        contents = Path(credential_file).read_text()

        if service is None and handle is None:
            (service, _, handle) = Path(credential_file).name.partition('_')

        credd.add_user_service_cred(htcondor.CredTypes.OAuth, contents, service, handle, user)


    choices = {
        "password":     _handle_add_password,
        "kerberos":     _handle_add_kerberos,
        "oauth2":       _handle_add_oauth2,
    }


    options = {
        "type": {
            "args":         ("type",),
            "metavar":      "type",
            "choices":      choices.keys(),
            "help":         "The credential type: password, kerberos, or oauth2",
        },
        "credential-file": {
            "args":         ("credential_file",),
            "metavar":      "credential-file",
            "help":         "Path to a file storing the credential",
        },
        "service": {
            "args":         ("--service",),
            "metavar":      "service",
            "help":         "(OAuth2)  Service name, if not from the file",
        },
        "handle": {
            "args":         ("--handle",),
            "metavar":      "handle",
            "help":         "(OAuth2)  Handle name, if not from the file",
        },
    }


    def __init__(self, logger, **options):
        self.choices[options['type']](** options)


class Remove(Verb):
    def _handle_delete_password(** options):
        user = None
        credd = htcondor.Credd()

        # This can't work except for on Windows...
        credd.delete_user_cred(htcondor.CredTypes.Password,user)


    def _handle_delete_kerberos(** options):
        user = None
        credd = htcondor.Credd()

        credd.delete_user_cred(htcondor.CredTypes.Kerberos, user)


    def _handle_delete_oauth2(*, service, handle, ** options):
        user = None
        credd = htcondor.Credd()

        credd.delete_user_service_cred(htcondor.CredTypes.OAuth, service, handle, user)


    choices = {
        "password":     _handle_delete_password,
        "kerberos":     _handle_delete_kerberos,
        "oauth2":       _handle_delete_oauth2,
    }


    options = {
        "type": {
            "args":         ("type",),
            "metavar":      "type",
            "choices":      choices.keys(),
            "help":         "The credential type: password, kerberos, or oauth2",
        },
        "service": {
            "args":         ("--service",),
            "metavar":      "service",
            "help":         "(OAuth2)  Service name, if not from the file",
        },
        "handle": {
            "args":         ("--handle",),
            "metavar":      "handle",
            "help":         "(OAuth2)  Handle name, if not from the file",
        },
    }


    def __init__(self, logger, **options):
        self.choices[options['type']](** options)


class Credential(Noun):
    '''
    Operations on credentials.
    '''

    class list(List):
        pass


    class add(Add):
        pass


    class remove(Remove):
        pass


    class listall(ListAll):
        pass


    @classmethod
    def verbs(cls):
        return [cls.list, cls.add, cls.remove, cls.listall]
