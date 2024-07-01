from pathlib import Path
import datetime

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
        except OSError:
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
        except OSError:
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
        names = {}
        for key in ad.keys():
            name = None
            if key.endswith(".top"):
                name = key[:-4]
            elif key.endswith(".use"):
                name = key[:-4]

            if name is not None:
                names[name] = ad[key]

        print(f">> Found OAuth2 credentials:")
        print(f"   {'Service':<19} {'Handle':<19} {'Last Refreshed':>19} {'File':<17}")
        for name in names:
            (service, _, handle) = name.partition('_')
            date = datetime.datetime.fromtimestamp(names[name])
            # If the filename is too long, don't truncate it.
            print(f"   {service:<19} {handle:<19} {str(date):>19} {name}.use")


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
