# Pelican File Transfer for HTCondor

## Project Overview

By default, HTCondor uses CEDAR for sandbox movement.  We would like to shift
the load of transfers & transfer management from CEDAR to the Pelican protocol.

To do so, we add a new Pelican origin component to the AP.  The origin will
be responsible for upload and download of sandboxes, including authorization.
When the `condor_shadow` would like to move an input sandbox to a job, it will
first contact the origin service via a Unix domain socket in a well-known location
(the domain socket provides authentication for the server), providing the job
ad and getting a token and a URL in response.  The shadow then orders the starter
to download the URL with the given token instead of doing the transfers via CEDAR.
On the starter side, this uses the standard plugin mechanism, which is augmented
to pass along arbitrary additional attributes in the input ClassAd.

## Implementation Notes

### Components

1. **New File Transfer commands** (`DownloadUrlWithAd`, `RequestUrlMetadata`, `CheckUrlSchemes`)
   - `DownloadUrlWithAd` sends a ClassAd with a URL to the downloader.  Additional attributes in
     the ad are copied into the transfer ad for the file transfer plugin.
   - `RequestUrlMetadata` requests metadata from the downloader for a given plugin.  This is used
     by the starter to request a destination and credentials from the shadow when doing stageout.
   - `CheckUrlSchemes` requests information from the downloader to see if it supports a given
     scheme.  This allows the shadow to detect if it's safe to swap out CEDAR for `pelican://`;
     this was previously guaranteed by matchmaking.

2. **Detection Logic** (`ShouldInputUsePelicanTransfer()` / `ShouldOutputUsePelicanTransfer()`)
   - Determines if Pelican should be used for a given job based on current configuration
   - Validates job characteristics (doesn't use unsupported features like directory transfer)
   - Can be restricted to specific users

3. **Registration Service Interface** (`PelicanRequestToken()`)
   - Uses libcurl over domain socket to communicate with Pelican registration service
   - POSTs job ad in JSON format
   - Receives token, expiration time, and transfer URLs
   - RESTful API endpoint: `http://localhost/api/v1/sandbox/register`

4. **Sandbox Manipulation** (`PreparePelicanInputTransferList()` / `PreparePelicanOutputTransferList()`)
   - Manipulate the list of file transfers, replacing CEDAR-based transfers with
     Pelican URLs.
   - (Input-only) Checks if the starter has the `pelican://` plugin
   - (Output-only) Requests a token & destination URL from the shadow.

## Configuration Knobs

The following configuration parameters control Pelican file transfer:

- `ENABLE_PELICAN_FILE_TRANSFER` (bool, default: false)
  - Master switch to enable/disable Pelican transfers

- `PELICAN_REGISTRATION_SOCKET` (string, default: "/var/run/pelican/registration.sock")
  - Unix domain socket path for registration service communication

- `PELICAN_CA_FILE` (string, optional)
  - Path to CA certificate file for TLS verification

- `PELICAN_ALLOWED_USERS` (string, optional)
  - Comma-separated list of users allowed to use Pelican transfers
  - If empty, all users are allowed (when enabled)

## Registration Service API

### Request: POST /api/v1/sandbox/register

**Request Body:** Job ClassAd in JSON format

Example:
```json
{
  "ClusterId": 12345,
  "ProcId": 0,
  "Owner": "user@example.com",
  "Cmd": "/path/to/executable",
  ...
}
```

**Response Body:** JSON

Example:
```json
{
  "token": "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...",
  "expires_at": 1735603200,
  "input_urls": ["pelican://foo.example.com/sandboxes/1.0/input"],
  "output_urls": ["pelican://foo.example.com/sandboxes/1.0/output"]
}
```

## Implementation Status

### ✅ Completed

1. **Input sandbox**
2. **Output sandbox**
3. **Integration Test**

### ⏳ TODO

1. **Security**
   - Token security best practices
   - Secure token storage (avoid logging)
   - TLS certificate validation

2. **Monitoring & Metrics**
   - Add statistics tracking for Pelican transformation

## Known Limitations

1. **Directory Transfers**: If a directory transfer is requested, we skip
   it and make CEDAR do it.
2. **Checkpoint Files**: Not yet implemented for Pelican

## Design Decisions

1. **Why HTTP over Unix Domain Sockets?**
   - Server side can determine the UID/GID of the client and use
     that for access control (e.g., opening/closing files as the
     client).
   - Existing dependency on libcurl; well-known client.

2. **Why Replace vs Augment CEDAR?**
   - Pelican provides better scalability for large transfers
   - Can offload transfer work from HTCondor daemons
   - Enables caching and deduplication at Pelican layer

3. **Why Transfer Sandboxes and not Files?**
   - We can give sandboxes a unique name and (potentially) pretend
     they are immutable.
   - Same access control as the shadow: you can't request an arbitrary
     file from the user's home directory, it must be listed in the ad.
   - The job ad is needed to calculate all the input/output locations.

## Future Considerations

- **Named sandboxes**: Have jobs be able to refer to a named sandbox
  so we can increase reuse
- **OSDF integration**: Use a Pelican origin also in the OSDF; not just
  a standalone one at the AP.
- **Caching**: Leverage reuse and immutability of sandboxes to scale out
  delivery.
