# Pelican File Transfer Implementation

## Project Overview

This document tracks the implementation of Pelican-based file transfer for HTCondor input and output sandboxes. The goal is to replace traditional CEDAR-based POSIX file transfers with a more efficient Pelican-based approach when appropriate.

## Architecture

### Components

1. **Detection Logic** (`ShouldUsePelicanTransfer()`)
   - Determines if Pelican should be used for a given job
   - Checks configuration knobs
   - Validates job characteristics (no complex directory transfers, etc.)
   - Can be restricted to specific users

2. **Token Service Interface** (`PelicanRequestToken()`)
   - Uses libcurl over domain socket to communicate with Pelican token service
   - POSTs job ad in JSON format
   - Receives token and expiration time
   - RESTful API endpoint: `http://localhost/api/v1/sandbox/register`

3. **Input Sandbox Transfer** (`DoPelicanInputTransfer()`)
   - Called by shadow when uploading input files to starter
   - Obtains token from Pelican service
   - Sends Pelican URL instead of actual files
   - URL format: `pelican://HOSTNAME/sandboxes/CLUSTER.PROC/input`
   - Includes token, expiration, and optional CA contents

4. **Output Sandbox Transfer** (`DoPelicanOutputTransfer()`, `PelicanRequestOutputToken()`)
   - Called by starter when uploading output files to shadow
   - Starter requests token from shadow
   - Shadow obtains token from Pelican service
   - Returns token to starter
   - Starter invokes Pelican plugin (stub for now)

## Configuration Knobs

The following configuration parameters control Pelican file transfer:

- `ENABLE_PELICAN_FILE_TRANSFER` (bool, default: false)
  - Master switch to enable/disable Pelican transfers
  
- `PELICAN_HOSTNAME` (string, default: "localhost")
  - Hostname for Pelican URLs
  
- `PELICAN_TOKEN_SOCKET` (string, default: "/var/run/pelican/token.sock")
  - Unix domain socket path for token service communication
  
- `PELICAN_CA_FILE` (string, optional)
  - Path to CA certificate file for TLS verification
  
- `PELICAN_ALLOWED_USERS` (string, optional)
  - Comma-separated list of users allowed to use Pelican transfers
  - If empty, all users are allowed (when enabled)

## Token Service API (Guessed)

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
  "expires_at": 1735603200
}
```

**Note:** This API is preliminary and may change as the server implementation is developed.

## Implementation Status

### ✅ Completed

1. **Detection Function**
   - `ShouldUsePelicanTransfer()` implemented
   - Checks for ENABLE_PELICAN_FILE_TRANSFER config
   - Basic validation (excludes directory transfers)
   - User whitelist support

2. **Token Request Infrastructure**
   - `PelicanRequestToken()` implemented
   - libcurl-based communication over Unix domain socket
   - JSON serialization/deserialization
   - Error handling and logging

3. **Input Sandbox Transfer**
   - `DoPelicanInputTransfer()` implemented
   - Integrated into `DoNormalUpload()`
   - Sends Pelican URL, token, expiration, and CA contents
   - Basic protocol for shadow-to-starter communication

4. **Output Sandbox Transfer Stubs**
   - `PelicanRequestOutputToken()` stub implemented
   - `DoPelicanOutputTransfer()` stub implemented
   - Framework for starter-to-shadow token requests

### 🚧 In Progress

None currently.

### ⏳ TODO

1. **Protocol Refinement**
   - Define exact wire protocol for Pelican transfers
   - Add versioning for forward/backward compatibility
   - Handle protocol negotiation between shadow and starter

2. **Starter-Side Receiver**
   - Implement receiver code in starter to handle Pelican URLs
   - Parse and validate token/expiration/CA
   - Invoke Pelican file transfer plugin for actual download

3. **Output Transfer Completion**
   - Implement actual file upload in `DoPelicanOutputTransfer()`
   - Shadow-side handling of output token requests
   - Pelican plugin invocation from starter

4. **Error Handling**
   - Better error messages and recovery
   - Fallback to traditional CEDAR transfer on Pelican failure
   - Token expiration handling and renewal

5. **Testing**
   - Unit tests for detection logic
   - Integration tests with mock Pelican service
   - End-to-end tests with real Pelican deployment
   - Performance benchmarks vs CEDAR transfers

6. **Token Service Implementation**
   - Finalize token service API
   - Implement server side
   - Authentication and authorization
   - Token lifecycle management

7. **Security**
   - Token security best practices
   - Secure token storage (avoid logging)
   - TLS certificate validation
   - Permissions and access control

8. **Monitoring & Metrics**
   - Add statistics tracking for Pelican transfers
   - Success/failure rates
   - Transfer sizes and durations
   - Integration with HTCondor stats framework

9. **Documentation**
   - User-facing documentation
   - Admin guide for Pelican setup
   - Configuration examples
   - Troubleshooting guide

10. **Advanced Features**
    - Support for directory transfers
    - Compression options
    - Parallel transfers
    - Resume capability for interrupted transfers
    - Checkpoint file support

## Known Limitations

1. **Directory Transfers**: Currently disabled for Pelican transfers due to complexity
2. **Complex File Lists**: Some advanced transfer_input_files patterns may not be supported
3. **Checkpoint Files**: Not yet implemented for Pelican
4. **Encryption**: Relies on Pelican's transport security (TLS)
5. **Fallback**: No automatic fallback to CEDAR on failure (planned)

## Integration Points

### In File Transfer Object

- `DoNormalUpload()`: Entry point for upload, checks for Pelican
- `DoDownload()`: Will need similar integration for downloads (not yet implemented)
- `HandleCommands()`: Shadow-side command handler, already supports Pelican path

### Dependencies

- libcurl: For HTTP communication over Unix socket
- jsoncpp: For JSON parsing and generation
- Pelican file transfer plugin: External component for actual transfers

## Design Decisions

1. **Why Unix Domain Sockets?**
   - More secure than network sockets for local communication
   - Better performance for local IPC
   - Standard pattern for local service communication

2. **Why Replace vs Augment CEDAR?**
   - Pelican provides better scalability for large transfers
   - Can offload transfer work from HTCondor daemons
   - Enables caching and deduplication at Pelican layer

3. **Why Job ClassAd in Token Request?**
   - Allows flexible authorization decisions
   - Supports user/group-based policies
   - Enables audit logging at Pelican layer

4. **Detection at Runtime vs Submit Time?**
   - Runtime detection allows more flexibility
   - Configuration changes don't require job resubmission
   - Can adapt to resource availability dynamically

## Future Considerations

- **Multi-site Transfers**: Support for federated Pelican namespaces
- **Caching**: Leverage Pelican's caching capabilities for frequently-used inputs
- **Data Locality**: Use Pelican's data location awareness for optimization
- **Quota Management**: Integration with Pelican quota systems
- **Monitoring Integration**: Export metrics to Pelican's monitoring infrastructure

## References

- HTCondor File Transfer: `src/condor_utils/file_transfer.cpp`
- FileTransfer Class: `src/condor_utils/file_transfer.h`
- Pelican Project: (URL to be added)

## Change Log

- 2024-12-30: Initial implementation of Pelican file transfer support
  - Added detection logic, token request infrastructure
  - Implemented input sandbox transfer
  - Created output transfer stubs
  - Created this documentation
