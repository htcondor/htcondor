import sys
from pathlib import Path
import datetime
from collections import defaultdict

import htcondor2 as htcondor
import classad2 as classad

from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb


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
        jobs_by_name = defaultdict(list)
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
                    jobs_by_name[service_name].append(f"{clusterID}.{procID}")


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

            jobs = ""
            job_list = jobs_by_name.get(name)
            if job_list is not None:
                jobs = ", ".join(job_list)

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


    @classmethod
    def verbs(cls):
        return [cls.list, cls.add, cls.remove]
