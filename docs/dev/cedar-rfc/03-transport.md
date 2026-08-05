# Chapter 3. Transport over TCP: Addresses, Shared Port, and CCB

A CEDAR endpoint is named by a textual address (the *sinful string*).
Reaching the daemon behind that address may involve up to three
mechanisms, composable with one another:

1. a **direct TCP connection** to the given host and port;
2. the **Shared Port** protocol, when many daemons on one host share a
   single listening port (`sock=` parameter);
3. the **Condor Connection Broker (CCB)**, when the daemon cannot accept
   inbound connections at all and instead connects *out* to a broker
   (`ccbid=` parameter).

Implementations: `src/condor_utils/condor_sinful.cpp`,
`src/condor_io/shared_port_{client,server}.cpp`, `src/ccb/` (C++);
`golang-cedar/addresses/`, `golang-cedar/client/sharedport/`,
`golang-cedar/ccb/`, `golang-ccb/` (Go).

## 3.1 Sinful Strings

```
sinful  := "<" address [ ":" port ] [ "?" params ] ">"
address := IPv4-literal | hostname | "[" IPv6-literal "]"
params  := param *( ("&" | ";") param )
param   := key "=" value          ; value is percent-encoded
```

Examples:

```
<128.104.100.22:9618>
<[2607:f388::1]:9618?alias=cm.example.edu>
<128.104.100.22:9618?sock=schedd_1234&PrivNet=cluster>
<192.0.2.1:9618?ccbid=203.0.113.5:9618%23742&sock=startd>
```

Percent-encoding covers all octets except alphanumerics and
`. _ - : # [ ] +`; `%XX` is a two-hex-digit escape and `+` is a literal
plus (path-style, not form-style, decoding).
(`condor_sinful.cpp:194-248`, `addresses/sinful.go:143-163`)

Recognized parameters:

| Key | Meaning |
|---|---|
| `sock` | shared-port identifier of the daemon behind a shared port (§3.3) |
| `ccbid` | space-separated list of CCB contacts, each `broker-address#ccbid` (§3.4); presence means "connect via CCB" |
| `addrs` | additional literal addresses: `ip-port` pairs joined by `+`, e.g. `addrs=192.0.2.1-9618+[2001:db8::1]-9618`; used for dual-stack IPv4/IPv6 |
| `alias` | hostname alias, used for certificate/host verification |
| `PrivAddr` | alternative (percent-encoded) sinful string usable within the named private network |
| `PrivNet` | private-network name qualifying `PrivAddr` |
| `noUDP` | flag: the daemon does not accept UDP |

### 3.1.1 Address Selection

A client resolves a sinful string as follows
(`golang-cedar/client/client.go:99-168`, `src/condor_io/sock.cpp`
`special_connect`):

1. If `ccbid` is present (and the client is not on the daemon's private
   network), connect via CCB (§3.4).
2. Otherwise, if `sock` is present, connect to the host/port and run the
   Shared Port protocol (§3.3).
3. Otherwise, connect directly. When `addrs` lists multiple address
   families, the C++ client tries candidates in configured preference
   order; the Go client races IPv6 and IPv4 ("happy eyeballs") with a
   150 ms fallback delay.

A daemon on the same host as a shared-port daemon may combine cases:
CCB brokers and their registered daemons are themselves frequently
behind shared ports, in which case the `ccbid` broker address or the
reverse connection target may in turn contain `sock=`.

## 3.2 Direct TCP Connections

There is no preamble, banner, or version exchange at the transport
level. The first bytes sent by the client are a CEDAR frame whose
payload begins with a command integer (Chapter 4). Version and feature
negotiation happen inside the command protocol.

## 3.3 The Shared Port Protocol

One daemon per host (`condor_shared_port`) owns the well-known TCP port
and hands accepted connections to the intended local daemon over a UNIX
domain socket. Each local daemon registers under a **shared port ID**
(the `sock=` value; matching `[A-Za-z0-9._-]+`).

### 3.3.1 Client → Shared Port Daemon

Immediately after connecting, the client sends one CEDAR message
(`shared_port_client.cpp:100-176`,
`client/sharedport/sharedport.go:113-150`):

```
int64   SHARED_PORT_CONNECT (= 75)
string  shared_port_id            ; e.g. "schedd_1234"
string  client_name               ; free-form, for logging only
int64   deadline                  ; seconds the client will wait; -1 = none
int64   more_args                 ; 0 (reserved for extensions)
<end of message>
```

There is **no reply**. On success, the next bytes the client sends and
receives travel to and from the target daemon; on failure the shared
port daemon simply closes the connection. If `more_args` is nonzero, a
receiver MUST read and discard that many additional values (currently
always 0).

The special ID `"self"` addresses the shared port daemon itself.

Because the target daemon never sees the `SHARED_PORT_CONNECT` message,
a client that maintains the pre-encryption transcript digest of §1.4.2
MUST reset the digest after sending this message (unless connecting to
`"self"`), so that both ends of the eventual daemon-to-client channel
digest the same bytes (`shared_port_client.cpp:168-172`).

### 3.3.2 Shared Port Daemon → Target Daemon (Host-Local)

The handoff is host-local and OS-specific rather than a network
protocol; it is described for completeness
(`shared_port_endpoint.cpp`, `client/sharedport/endpoint_protocol.go`):
the shared port daemon connects to the target's UNIX domain socket,
sends the one-value CEDAR message

```
int64   SHARED_PORT_PASS_SOCK (= 76)
<end of message>
```

and then transfers the connected TCP file descriptor with
`sendmsg(2)`/`SCM_RIGHTS` (one dummy payload byte). No acknowledgment
is sent.

## 3.4 The Condor Connection Broker (CCB)

CCB inverts connection direction for daemons that can make outbound
connections but not accept inbound ones. Three parties participate:

- the **target daemon** (e.g. a startd behind NAT), which registers with
  a broker and keeps a persistent TCP connection to it;
- the **broker** (CCB server, usually co-located with the collector);
- the **client** (e.g. a schedd) that wants to reach the target.

All CCB payloads are ClassAds (Chapter 2), sent over ordinary CEDAR
command connections with the usual security handshake (Chapter 4).
Command numbers: `CCB_REGISTER` = 67, `CCB_REQUEST` = 68,
`CCB_REVERSE_CONNECT` = 69 (`condor_commands.h:885-889`,
`golang-cedar/commands/commands.go:222-224`).

### 3.4.1 Registration

The target daemon sends `CCB_REGISTER` with:

| Attribute | Type | Meaning |
|---|---|---|
| `Name` | string | daemon name (logging only) |
| `CCBID` | string | (reconnect only) previously assigned contact |
| `ClaimId` | string | (reconnect only) reconnect cookie from prior registration |

The broker replies on the same connection with:

| Attribute | Type | Meaning |
|---|---|---|
| `CCBID` | string | contact of the form `broker-address#id`, e.g. `203.0.113.5:9618#742` — note the broker address carries **no** angle brackets; `id` is a decimal unsigned integer |
| `ClaimId` | string | reconnect cookie: 40 hex characters (20 random bytes) |
| `Command` | int | echo of `CCB_REGISTER` |

The daemon advertises the returned contact(s) in the `ccbid=` parameter
of its sinful string. The TCP connection is left open; the broker sends
periodic keep-alive messages (ClassAds containing `Command = 441`
(`ALIVE`)) and uses this connection to deliver connect requests. If the
connection breaks, the daemon re-registers presenting its `CCBID` and
`ClaimId` to retain the same contact string.
(`ccb_server.cpp:449-527`, `golang-ccb/server.go:112-250`)

### 3.4.2 Connection Request and Reverse Connect

```
 Client                        Broker                     Target daemon
   |                             |     (persistent conn)      |
   |--- CCB_REQUEST ad --------->|                            |
   |                             |--- request ad ------------>|
   |                             |                            |
   |<========================== TCP connect (reverse) ========|
   |<-- CCB_REVERSE_CONNECT ad --|---(sent by target)         |
   |                             |<-- result ad --------------|
   |<-- reply ad ----------------|                            |
   |                             |                            |
   |=== CEDAR command protocol proceeds on reversed socket ===|
```

**Step 1.** The client, listening on its own address, sends
`CCB_REQUEST` to the broker:

| Attribute | Type | Meaning |
|---|---|---|
| `CCBID` | string | target's contact, `broker-address#id` |
| `MyAddress` | string | client's sinful string, to which the target should connect back |
| `ClaimId` | string | **connect id**: 40 fresh random hex characters |
| `Name` | string | client name (logging) |

**Step 2.** The broker forwards a request ad to the target over the
registration connection, adding `Command = CCB_REQUEST` and a broker-
assigned decimal `RequestID` (string).

**Step 3.** The target connects to `MyAddress` (a normal CEDAR
connection, possibly itself via shared port) and sends, as its first
command, `CCB_REVERSE_CONNECT` (69) followed immediately by a ClassAd —
no security handshake precedes this command:

| Attribute | Type | Meaning |
|---|---|---|
| `ClaimId` | string | the connect id from the request — proves this connection answers the client's request |
| `RequestID` | string | broker request id |
| `MyAddress` | string | target's own address |

The client MUST verify that `ClaimId` equals the connect id it generated
in step 1 and MUST otherwise discard the connection.

**Step 4.** The target reports the outcome to the broker (ad with
`RequestID`, `ClaimId`, `Result` (bool), and `ErrorString` on failure);
the broker relays `Result`/`ErrorString` to the waiting client.

**Step 5.** The client now treats the reversed socket as if it had
dialed it: it initiates the ordinary command protocol of Chapter 4
(including authentication) with the roles it originally intended —
the *client* remains the security-handshake client even though the TCP
connection was accepted, not dialed.

Multiple `ccbid` contacts (space-separated) may be listed in a sinful
string when a daemon registered with several brokers; clients try them
in order.

### 3.4.3 Authorization

Registration requires the broker's `DAEMON`-level authorization;
connection requests require `READ`. The connect id in `ClaimId` is the
only credential linking the reverse connection to the request, so it
MUST be generated from a cryptographically secure random source and
MUST be sent only over channels whose confidentiality matches the
deployment's threat model (the broker necessarily learns it).

### 3.4.4 Worked Example

A startd on `worker-01` (private address `10.0.0.17`, behind NAT)
registers with a broker at `203.0.113.5:9618`; later a schedd at
`198.51.100.7:9618` connects to it. The ads below are shown in ClassAd
text; on the wire each is serialized per Chapter 2.

**Registration.** Startd → broker, command `CCB_REGISTER` (67):

```
[ Name = "startd worker-01.example.net" ]
```

Broker → startd, on the same (now persistent) connection:

```
[ CCBID   = "203.0.113.5:9618#742";
  ClaimId = "8f3a5b9c0d1e2f3a4b5c6d7e8f9a0b1c2d3e4f5a";
  Command = 67 ]
```

The startd then advertises itself to the collector with a sinful string
carrying the contact (`#` percent-encoded as `%23`):

```
<10.0.0.17:9618?ccbid=203.0.113.5:9618%23742&addrs=10.0.0.17-9618>
```

If the registration connection later breaks, the startd re-registers
sending all three of `Name`, `CCBID`, and `ClaimId` to keep contact
`742`.

**Connection request.** The schedd, wanting to reach the startd,
listens on its own address, generates a fresh connect id, and sends
`CCB_REQUEST` (68) to the broker:

```
[ CCBID     = "203.0.113.5:9618#742";
  MyAddress = "<198.51.100.7:9618?addrs=198.51.100.7-9618>";
  ClaimId   = "a1b2c3d4e5f60718293a4b5c6d7e8f9012345678";
  Name      = "schedd submit.example.net" ]
```

The broker forwards over the registration connection:

```
[ Command   = 68;
  MyAddress = "<198.51.100.7:9618?addrs=198.51.100.7-9618>";
  ClaimId   = "a1b2c3d4e5f60718293a4b5c6d7e8f9012345678";
  RequestID = "3151";
  Name      = "schedd submit.example.net" ]
```

**Reverse connect.** The startd dials `198.51.100.7:9618` and sends, as
the first command on that socket, `CCB_REVERSE_CONNECT` (69) followed by:

```
[ ClaimId   = "a1b2c3d4e5f60718293a4b5c6d7e8f9012345678";
  RequestID = "3151";
  MyAddress = "<10.0.0.17:9618?ccbid=203.0.113.5:9618%23742&addrs=10.0.0.17-9618>" ]
```

The schedd checks `ClaimId` against the connect id it generated — a
match proves this inbound connection answers its request. The startd
reports back to the broker:

```
[ Result    = true;
  RequestID = "3151";
  ClaimId   = "a1b2c3d4e5f60718293a4b5c6d7e8f9012345678" ]
```

and the broker relays the outcome to the schedd's waiting `CCB_REQUEST`:

```
[ Result = true ]
```

(on failure: `[ Result = false; ErrorString = "..." ]`). The schedd now
runs the normal command protocol (Chapter 4) over the reversed socket,
acting as the security-handshake client.

## 3.5 Implementation Differences

| Aspect | C++ | Go |
|---|---|---|
| Shared port daemon (server side) | yes | no (client-side protocol only) |
| CCB broker | yes | yes (`golang-ccb`), adds an experimental proxy/streaming mode (`CCBStreamingRequired`/`ProxyMode` attributes) not present in C++ |
| Broker reconnect-cookie persistence | on disk | pluggable `ReconnectStore` interface |
| Dual-stack dialing | ordered candidate list | happy-eyeballs race, 150 ms fallback |
| UDP addressing (`noUDP` relevance) | yes | UDP unsupported |
