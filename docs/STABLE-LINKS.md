# External Links to Documentation

When including a link to some part of the documentation in the
outside world that needs to be stable, use the **auto-redirect.html**
page. The auto-redirect parses a stable URL scheme to map the stable
URL to the appropriate documentation page. This exists so links for
the outside world (example submit descriptions, critical error messages,
etc.) can exists with less fear of breaking in the future due to
documentation changes/reorganization.

## URL Scheme

All URLs will follow have the same base and provided additional
information that javascript will parse:

```
https://htcondor.readthedocs.io/en/latest/auto-redirect.html?<additional information>
```

Additional Information
----------------------

The information post **?** should be **&** delimited **key=value** pairs
that will be processed for determining where to redirect the page. The
redirect page expects the following key names in order to determine the
correct redirect:

- **category**
- **tag**

```
Note:
    All other **key=value** pairs of information are just ignored.
```

The associated key values are used as the keys for internal map objects.
The category key's value becomes a map key to determine the correct sub-map
to access which then uses the tag key's value to find the correct redirect
URL.

**Example:**

```
?category=example&tag=example
```

## Adding/Updating Options

To add stable URLs update the maps in the **docs/auto-redirect.html** file.

```
Warning:
    Think carefully when naming new **categories** or **tags** since they
    should **NEVER** change. Only the data a **tag** references should be
    changed.
```

## Redirect Failure

If the URL to the redirect page has bad or no additional information then
the webpage will be updated with a error message and a message asking the
user to email the HTCSS dev team about the issue.

If all the additional information is valid but the created redirect link
does not exist then the user will unfortunately get a 404 not found error.

## Valid Categories

|  Categories  |
|:-------------|
| example      |
