import htcondor2 as htcondor
import classad2 as classad

from htcondor_cli.noun import Noun
from htcondor_cli.verb import Verb


class List(Verb):
    def __init__(self, logger, **options):
        # htcondor.enable_debug()

        user = ''
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
            print(f">> Windows password timestamp {windows_password_time}")
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
            print(f">> Kerberos timestamp {kerberos_time}")
        else:
            print(f">> no Kerberos credential found")

        # Check for OAuth2 credentias.
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
        for name in names:
            print(f"   {name} @{names[name]}")


class Add(Verb):
    def __init__(self, logger, **options):
        pass


class Remove(Verb):
    def __init__(self, logger, **options):
        pass


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
