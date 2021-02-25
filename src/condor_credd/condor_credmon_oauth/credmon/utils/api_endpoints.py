import re

DOMAINS = {
    'box':               re.compile(r'https://.*\.box\.com'),
    'gdrive':            re.compile(r'https://.*\.googleapis\.com'),
    'onedrive':          re.compile(r'https://.*\.microsoftonline\.com'),
    'dropbox':           re.compile(r'https://.*\.dropboxapi?\.com'),
}

def get_token_name(token_url):
    '''Returns the token name by matching it to the list of token domains'''
    for token_name, domain in DOMAINS.items():
        if domain.match(token_url):
            return token_name
    return None

def user(token_url):
    '''Returns the URL and fields to query for user info'''

    url = {
        'box':          'https://api.box.com/2.0/users/me',
        'gdrive':       'https://www.googleapis.com/drive/v3/about&fields=user',
        'onedrive':     'https://graph.microsoft.com/v1.0/me',
        'dropbox':      'https://api.dropboxapi.com/2/users/get_account',
    }

    field = {
        'box':          ['login'],
        'gdrive':       ['user', 'emailAddress'],
        'onedrive':     ['userPrincipalName'],
        'dropbox':      ['email'],
    }

    token = get_token_name(token_url)
    if (token is None) or not ((token in url) and (token in field)):
        return (None, None)

    return (url[token], field[token])

def token_lifetime_fraction(token_url):
    '''Returns the fraction of an access token's lifetime at
    which point it should be refreshed. Default is 0.5 (the
    token's "halflife").'''

    lifetime = {
        'box': 0.95,
    }

    token = get_token_name(token_url)
    if (token is None) or not (token in lifetime):
        return 0.5

    return lifetime[token]
