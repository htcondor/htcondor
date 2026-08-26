# Chapter 4. The Command Protocol and Security Negotiation

Every CEDAR interaction is a **command**: the client sends an integer
identifying the requested operation, then a command-specific dialogue
follows. Modern clients do not send application commands directly;
they wrap them in the `DC_AUTHENTICATE` handshake, which negotiates
security policy, authenticates the peers, establishes an AES-256-GCM
channel key, and creates a resumable **security session** — and only
then dispatches the real command.

Implementations: `src/condor_daemon_core.V6/daemon_command.cpp` and
`src/condor_io/condor_secman.cpp` (C++);
`golang-cedar/security/auth.go` and
`golang-cedar/security/session_cache.go` (Go).

## 4.1 Command Integers

A command is a CEDAR integer (8 bytes on the wire, §1.3.1). Numbers are
assigned from a central registry (`condor_commands.h`,
`golang-cedar/commands/commands.go`). Notable values and ranges:

| Value / range | Purpose |
|---|---|
| 0–99 | collector: `UPDATE_STARTD_AD` = 0, `QUERY_STARTD_ADS` = 5, `QUERY_SCHEDD_ADS` = 6, … |
| 67–69 | CCB: register / request / reverse-connect (§3.4) |
| 75, 76 | shared port: connect / pass-socket (§3.3) |
| 400–499 | schedd/negotiator (`SCHED_VERS` base) |
| 441 | `ALIVE` keep-alive |
| 1111, 1112 | schedd queue management (`QMGMT_READ_CMD`, `QMGMT_WRITE_CMD`) |
| 60000–60099 | DaemonCore: `DC_AUTHENTICATE` = **60010**, `DC_NOP` = 60011, `DC_NOP_READ` = 60020, `DC_NOP_WRITE` = 60021, `DC_SEC_QUERY` = 60040, … |
| 61000+ | file transfer |
| 75000+ | credd |

Each registered command has an associated **authorization level**
(READ, WRITE, DAEMON, ADMINISTRATOR, NEGOTIATOR, …) that the server
enforces after authentication.

## 4.2 Connection Outline

```
Client                                                     Server
  | ---- int64 DC_AUTHENTICATE (60010) ------------------->  |
  | ---- ClassAd: client security policy ---- <EOM> ------>  |   cleartext
  | <--- ClassAd: server security policy ---- <EOM> ------   |   cleartext
  |                                                          |
  | <=== authentication method loop (Chapter 5) ==========>  |   cleartext*
  |                                                          |
  |      [both sides derive/obtain 32-byte session key]      |
  |      [AES-256-GCM enabled; transcript digests bound]     |
  |                                                          |
  | <--- ClassAd: post-auth response --------- <EOM> ------  |   encrypted
  |                                                          |
  | ---- real command payload ---------------------------->  |   encrypted
```

\* individual mechanisms may tunnel their own cryptography (e.g. TLS)
inside this phase.

A *legacy* client may still send a bare application command integer with
no `DC_AUTHENTICATE` wrapper; the connection then proceeds without
authentication or encryption, subject to server policy. New
implementations MUST use the wrapped form.

## 4.3 The Client Security Policy Ad

The ad following `DC_AUTHENTICATE` is a "POD" ClassAd (§2.6) carrying
(`auth.go:759-830`, attribute names from
`condor_attributes.h:1038-1089`):

| Attribute | Type | Meaning |
|---|---|---|
| `Command` | int | the real command the client intends to execute |
| `AuthCommand` | int | for `DC_SEC_QUERY`: the command whose authorization is being probed |
| `AuthMethods` | string | comma-separated authentication methods offered, in preference order, e.g. `"IDTOKENS,SSL,FS"` |
| `CryptoMethods` | string | crypto methods offered, e.g. `"AES"` (legacy names may also appear; out of scope) |
| `Authentication` | string | `REQUIRED` \| `PREFERRED` \| `OPTIONAL` \| `NEVER` |
| `Encryption` | string | same four-valued scale |
| `Integrity` | string | same four-valued scale (with AES-GCM, integrity is inherent in encryption) |
| `Enact` | string | `NO` during negotiation |
| `NewSession` | string | `YES` — full handshake requested |
| `SessionDuration` | int | requested session lifetime, seconds |
| `SessionLease` | int | requested lease, seconds |
| `ECDHPublicKey` | string | base64 of the client's ephemeral ECDH P-256 public key (65-byte uncompressed point, leading `0x04`) |
| `NegotiatedSession` | bool | `true` (marks the modern protocol) |
| `RemoteVersion` | string | client's version string, `"$CondorVersion: …"` |
| `TrustDomain` | string | client's token trust domain |
| `Subsystem`, `ServerPid`, `Sid`, … | | informational |

## 4.4 The Server Response Ad

The server reconciles the client's ad with its own policy and replies
(cleartext) with (`auth.go:930-997`):

| Attribute | Type | Meaning |
|---|---|---|
| `AuthMethodsList` | string | full list of methods the server can accept, in server preference order |
| `AuthMethods` | string | single selected method (kept for pre-list peers) |
| `CryptoMethodsList` / `CryptoMethods` | string | ditto for crypto |
| `Authentication` | string | `YES` \| `NO` — will authentication occur |
| `Encryption` | string | `YES` \| `NO` |
| `Integrity` | string | `YES` \| `NO` |
| `Enact` | string | `YES` — parameters are final |
| `ECDHPublicKey` | string | server's ephemeral P-256 public key |
| `SessionDuration`, `SessionLease` | int | server's decision |
| `RemoteVersion`, `TrustDomain` | string | server's values |

Clients MUST prefer `AuthMethodsList`/`CryptoMethodsList` when present
and fall back to the singular attributes for older peers
(`auth.go:832-879`).

**Policy reconciliation.** Each feature (authentication, encryption,
integrity) is decided from the two four-valued preferences: if either
side says `NEVER` while the other says `REQUIRED`, negotiation fails;
`NEVER` beats `OPTIONAL`/`PREFERRED`; the feature is enabled iff at
least one side requires it or both prefer it. The method actually used
is the first entry of the *server's* preference list also offered by the
client (`auth.go:1293-1399`, `condor_secman.cpp`
`ReconcileSecurityPolicyAds`).

## 4.5 Authentication Phase

If authentication was enabled, the negotiation of Chapter 5 now runs:
an exchange of method bitmasks and mechanism-specific sub-protocols that
yields (a) a mutually authenticated identity for the client and (b),
for most mechanisms, a mechanism-derived key. This phase is still
carried in cleartext CEDAR frames unless the mechanism itself provides
cryptography (TLS-based methods).

## 4.6 Key Agreement and Channel Activation

When both sides advertised `ECDHPublicKey` and negotiated the `AES`
crypto method, the channel key is derived by ephemeral Elliptic-Curve
Diffie-Hellman over P-256 followed by HKDF-SHA256
(`auth.go:1534-1615`):

```
shared_secret = ECDH(own_private, peer_public)          // P-256
key           = HKDF-SHA256(ikm  = shared_secret,
                            salt = "htcondor",
                            info = "keygen",
                            len  = 32)
```

The 32-byte key becomes the AES-256-GCM key for the frame protection of
§1.4. Public keys are the raw 65-byte uncompressed points, base64
encoded, exchanged inside the policy ads of §§4.3–4.4; the Go
implementation additionally accepts DER-encoded points for
compatibility.

Because the ECDH exchange itself is unauthenticated, its result is
trustworthy only in combination with (a) the authentication phase and
(b) the transcript binding of §1.4.2, which folds the byte-exact
handshake (including both public keys and the negotiated parameters)
into the AAD of the first encrypted frame in each direction. A
downgrade or key-substitution attack therefore causes the first
encrypted frame to fail authentication.

For legacy peers without ECDH support, the mechanism-derived key from
the authentication phase is transported by the mechanism's own key
exchange (§5.7) and used directly; details of the legacy ciphers keyed
this way are out of scope.

After key establishment both sides enable encryption; **everything from
the post-auth ad onward is inside AES-GCM protected frames**, including
the real command's entire dialogue.

## 4.7 The Post-Auth Response Ad

The server sends one final (encrypted) ad (`auth.go:1000-1063`):

| Attribute | Type | Meaning |
|---|---|---|
| `ReturnCode` | string | `"AUTHORIZED"` on success |
| `User` | string | the client's canonical identity as mapped by the server, `user@domain` |
| `Sid` | string | the new session's identifier |
| `ValidCommands` | string | comma-separated command integers this session may execute without re-authenticating |
| `SessionDuration` | int | granted lifetime, seconds (typical default 3600) |
| `SessionLease` | int | granted lease, seconds (typical default 1800) |

The client then proceeds with the payload of the command named in its
policy ad's `Command` attribute, exactly as if it had sent that command
integer on a raw connection.

## 4.8 Security Sessions and Resumption

Both sides cache the negotiated state as a **session**
(`session_cache.go:19-31`): the session id, the peer address, the key
(`KeyInfo`: 32 key bytes + protocol name, e.g. `"AES"`), a policy
snapshot (negotiated method, the authenticated `User`, peer version),
an expiration (`SessionDuration`) and a lease. Clients index sessions
by `(peer address, command)` using `ValidCommands`. A session whose
lease keeps being renewed by use remains valid until its duration
expires.

**Resumption.** To reuse a session on a new TCP connection, the client
sends `DC_AUTHENTICATE` with a short ad:

```
Command        = <real command>
UseSession     = "YES"
Sid            = "<cached session id>"
ResumeResponse = true
RemoteVersion  = "..."
```

If the server still holds the session it replies
`ReturnCode = "AUTHORIZED"` (with the `Sid`), both sides install the
cached key, enable AES-GCM immediately, and the command proceeds — no
authentication loop, no ECDH. If not, it replies
`ReturnCode = "SID_NOT_FOUND"`; the client MUST discard its cached
session and restart with a full `NewSession` handshake
(`auth.go:1184-1215, 604-618`).

Sessions may also be *injected* out of band: a parent daemon passes
sessions to children via the `CONDOR_PRIVATE_INHERIT` environment (both
implementations), and higher-level protocols (e.g. the negotiator
handing a schedd a claim) embed session keys in ClassAd secrets. Such
sessions behave exactly like negotiated ones on the wire.

There is no explicit invalidation command in the current protocol
(`DC_INVALIDATE_KEY` exists in the historical registry but is unused);
stale sessions are handled by the `SID_NOT_FOUND` fallback.

## 4.9 UDP Commands (C++ Only)

Over UDP, there is no room for a handshake: a datagram carries a
cleartext session id in its extension header (§1.6), and the payload is
protected with the keys of that previously established (over TCP)
session. A datagram naming an unknown session is dropped. The Go
implementation omits UDP entirely.

## 4.10 `DC_SEC_QUERY`

`DC_SEC_QUERY` (60040) is a diagnostic command: the client runs the
normal handshake with `AuthCommand` naming a target command, and the
server answers whether the authenticated identity would be authorized
for it, without executing anything.

## 4.11 Worked Example

A `condor_status`-style client queries a collector for slot ads
(`QUERY_STARTD_ADS` = 5). Ads are shown as ClassAd text; on the wire
each is serialized per Chapter 2 inside the frames of Chapter 1.

**1. Client → server** (cleartext): the integer `60010`
(`DC_AUTHENTICATE`), then the policy ad:

```
[ Command            = 5;
  AuthMethods        = "IDTOKENS,FS";
  CryptoMethods      = "AES";
  Authentication     = "REQUIRED";
  Encryption         = "PREFERRED";
  Integrity          = "PREFERRED";
  Enact              = "NO";
  NewSession         = "YES";
  NegotiatedSession  = true;
  SessionDuration    = 3600;
  SessionLease       = 1800;
  ECDHPublicKey      = "BGb2…kA==";        # base64, 65-byte P-256 point
  RemoteVersion      = "$CondorVersion: 25.4.0 2026-06-01 $";
  TrustDomain        = "example.net";
  Subsystem          = "TOOL" ]
```

**2. Server → client** (cleartext), after reconciling with its own
policy (server prefers SSL but accepts IDTOKENS; requires
authentication and encryption):

```
[ AuthMethodsList    = "SSL,IDTOKENS,FS";
  AuthMethods        = "SSL";
  CryptoMethodsList  = "AES";
  CryptoMethods      = "AES";
  Authentication     = "YES";
  Encryption         = "YES";
  Integrity          = "YES";
  Enact              = "YES";
  SessionDuration    = 3600;
  SessionLease       = 1800;
  ECDHPublicKey      = "BJ9x…Qw==";
  RemoteVersion      = "$CondorVersion: 25.4.0 2026-06-01 $";
  TrustDomain        = "example.net" ]
```

**3. Authentication loop** (cleartext): the client offers
IDTOKENS + FS, the server selects IDTOKENS, and the AKEP2 exchange
authenticates both parties — see the worked example in §5.9.

**4. Key agreement:** both sides compute
`HKDF-SHA256(ECDH(priv, peer_pub), "htcondor", "keygen")` → 32-byte
AES-256-GCM key, and enable frame encryption. The first encrypted frame
in each direction carries the transcript-digest AAD of §1.4.2, sealing
steps 1–3 against tampering.

**5. Server → client** (encrypted), the post-auth ad:

```
[ ReturnCode      = "AUTHORIZED";
  User            = "bbockelm@example.net";
  Sid             = "worker:3145:1782115200:42";
  ValidCommands   = "5,6,50,51";
  SessionDuration = 3600;
  SessionLease    = 1800 ]
```

**6. The real command proceeds** (encrypted): the client does **not**
resend the command integer — the server already dispatched on
`Command = 5` — it sends the query's payload directly (for this command,
a constraint ClassAd), and the collector streams back matching machine
ads.

**Resumption.** For its next query the client opens a new TCP
connection and sends `60010` plus only:

```
[ Command        = 5;
  UseSession     = "YES";
  Sid            = "worker:3145:1782115200:42";
  ResumeResponse = true;
  RemoteVersion  = "$CondorVersion: 25.4.0 2026-06-01 $" ]
```

The server finds the session, replies
`[ ReturnCode = "AUTHORIZED"; Sid = "worker:3145:1782115200:42" ]`,
both sides install the cached AES key and enable encryption at once,
and step 6 repeats with no authentication or ECDH. Had the server
restarted, the reply would be `[ ReturnCode = "SID_NOT_FOUND" ]` and
the client would fall back to the full handshake above.

## 4.12 Implementation Differences

| Aspect | C++ | Go |
|---|---|---|
| UDP command protocol | yes | no |
| Legacy MAC/integrity modes | yes | integrity only via AES-GCM |
| Session persistence | in-memory + daemon inheritance | in-memory + inheritance (`SetInherited`) |
| Policy configuration | extensive `SEC_*` knobs per access level | programmatic `SecurityConfig` |
| Post-auth authorization mapping | unified map file + per-command auth levels | pluggable `PostAuthPolicy` callback |
