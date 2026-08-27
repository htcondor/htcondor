# Chapter 5. Authentication Protocols

This chapter specifies the authentication phase of the command protocol
(§4.5): the method negotiation loop and the individual mechanisms.
Focus is on the mechanisms implemented in **both** the C++ and Go
codebases: **TOKEN/IDTOKENS** and **PASSWORD** (a shared AKEP2 core),
**SSL**, **SCITOKENS**, **FS**/**FS_REMOTE**, and **CLAIMTOBE**.

Implementations: `src/condor_io/authentication.cpp` and
`src/condor_io/condor_auth_*.cpp` (C++);
`golang-cedar/security/{auth,token_auth,ssl_auth,fs_auth}.go` (Go).

## 5.1 Method Identifiers

Methods are named by strings in the policy ads (§4.3) and by bit values
in the negotiation loop (`condor_auth.h`, `auth.go:61-78`):

| Name | Bit | In C++ | In Go |
|---|---:|---|---|
| `CLAIMTOBE` | 2 | yes | yes |
| `FS` | 4 | yes | yes |
| `FS_REMOTE` | 8 | yes | yes |
| `NTSSPI` | 16 | Windows only | no |
| `KERBEROS` | 64 | yes | no |
| `ANONYMOUS` | 128 | yes | no |
| `SSL` | 256 | yes | yes |
| `PASSWORD` | 512 | yes | yes |
| `MUNGE` | 1024 | yes | no |
| `TOKEN` / `IDTOKENS` | 2048 | yes | yes |
| `SCITOKENS` | 4096 | yes | yes |

## 5.2 The Method Negotiation Loop

After the policy ads are exchanged, the client and server agree on a
mechanism by exchanging integers
(`authentication.cpp:949-1098`, `auth.go:1811-2038`):

```
repeat:
  Client -> Server : int64 bitmask of methods the client can still try   <EOM>
  Server -> Client : int64 single selected method bit, or 0              <EOM>
```

- The client's first bitmask is the OR of the methods it offered that
  also appear in the server's list (and whose local prerequisites —
  credentials, key files — are satisfied).
- The server picks the **first method in its own preference order**
  whose bit is set, and echoes exactly that bit. `0` means "nothing
  acceptable" and aborts authentication.
- The selected mechanism's sub-protocol (§§5.3–5.6) then runs. On
  failure, the client clears that method's bit and loops; sending a
  bitmask of `0` abandons authentication. On success the loop ends.

The result of a successful mechanism is the client's **authenticated
name** (mechanism-specific, e.g. a certificate subject or token
subject). The server maps it to a canonical `user@domain` via its map
file/policy; only the mapped result is wire-visible, in the post-auth
ad's `User` attribute (§4.7).

## 5.3 TOKEN / IDTOKENS and PASSWORD (AKEP2)

`PASSWORD` (protocol version 1) and `TOKEN` (version 2) share one
sub-protocol: AKEP2 (Bellare–Rogaway Authenticated Key Exchange
Protocol 2), instantiated with HMAC-SHA256
(`condor_auth_passwd.cpp:57-98`, `token_auth.go`). They differ only in
where the shared secret comes from:

- **PASSWORD:** both sides hold the *pool password*; the secret is the
  password concatenated with itself.
- **TOKEN:** the client holds an **IDTOKEN** — a JWT signed with
  HMAC-SHA-256/384/512 — and the server holds the *pool signing key*
  from which the JWT's signature can be recomputed. The JWT's claims:
  `sub` (client identity), `iss` (trust domain), `iat`, optional `exp`,
  `jti`, optional `scope` (authorization limits), and header `kid`
  naming the signing key. The signing key for `kid` is stretched as
  `jwt_key = HKDF-SHA256(master_key, salt="htcondor", info="master jwt")`
  and the JWT signature is `HMAC(header.payload, jwt_key)`.

### 5.3.1 Key Derivation

Let `S` be the shared secret (doubled pool password, or the JWT
signature). Two 256-byte protocol constants `seed_ka`, `seed_kb` are
fixed by the implementations (`condor_auth_passwd.cpp:1077-1140`,
`token_auth.go:513-540`). For TOKEN, let `T` be the transmitted token
content `base64url(header) "." base64url(payload)` (no signature).

```
version 1 (PASSWORD):  K  = HMAC-SHA256(seed_ka, S)
                       K' = HMAC-SHA256(seed_kb, S)
version 2 (TOKEN):     K  = HKDF-SHA256(ikm=S, salt=seed_ka || T, info="master ka", len=32)
                       K' = HKDF-SHA256(ikm=S, salt=seed_kb || T, info="master kb", len=32)
```

`K` keys the AKEP2 MACs; `K'` keys the session-key derivation.

### 5.3.2 Message Exchange

Let `A` be the client identity string (e.g. `user@trust.domain`), `B`
the server identity (`condor@trust.domain`), and `rA`, `rB` 256-byte
random nonces. Every variable-length field is sent as an integer length
followed by the bytes. Three CEDAR messages
(`condor_auth_passwd.cpp:2932-3128`, `token_auth.go:544-799`):

```
Msg 1  Client -> Server:
    int64  status            ; 0 = AUTH_PW_A_OK, -1 = error
    int64  len(A),  bytes A
    string T                  ; version 2 only: JWT header.payload
    int64  len(rA), bytes rA
    <EOM>

Msg 2  Server -> Client:
    int64  status
    int64  len(A),  bytes A       ; echo
    int64  len(B),  bytes B
    int64  len(rA), bytes rA      ; echo
    int64  len(rB), bytes rB
    int64  len(mac1), bytes mac1  ; mac1 = HMAC-SHA256(K, B || A || rA || rB)
    <EOM>

Msg 3  Client -> Server:
    int64  status
    int64  len(A),  bytes A       ; echo
    int64  len(rB), bytes rB      ; echo
    int64  len(mac2), bytes mac2  ; mac2 = HMAC-SHA256(K, A || rB)
    <EOM>
```

The client verifies `mac1` (proving the server knows `K`, hence the
password / signing key) before sending Msg 3; the server verifies
`mac2` (proving the client knows `K`, hence for TOKEN a validly signed
JWT — the server recomputed the signature from `T` and its `kid`).
Either side aborts by sending a negative `status`. The server
additionally validates the JWT claims: `iss` must match its trust
domain, `exp` must not have passed, and `sub` becomes the authenticated
name (subject to `scope` restrictions).

### 5.3.3 Session Key

Both sides derive the mechanism session key `W` from `rB`:

```
version 1:  W = HMAC-SHA256(K', rB)
version 2:  W = HKDF-SHA256(ikm=rB, salt=K', info="session key", len=32)
```

(`condor_auth_passwd.cpp:3218-3226`, `token_auth.go:796-850`.) `W`
seeds the channel key when ECDH (§4.6) is not in use; with ECDH, the
ECDH-derived key takes precedence for the channel while `W` still
authenticates the mechanism's key-exchange step (§5.7).

## 5.4 SSL and SCITOKENS

`SSL` authenticates with X.509 certificates over TLS; `SCITOKENS`
reuses the same TLS machinery but authenticates the *client* with a
bearer JWT (a SciToken) instead of a client certificate.

The mechanism runs in up to four phases, each a sequence of strictly
alternating CEDAR messages: (1) an initial status share, (2) the TLS
handshake, (3) transport of a 256-byte session key, and (4) — SCITOKENS
only — transfer of the token. Certificate verification happens locally
between phases 2 and 3.

### 5.4.1 Message Form and Status Codes

TLS bytes are **not** written raw to the socket; they are ferried
inside CEDAR messages so that the framing layer (and its transcript
digest, §1.4.2) stays consistent. Apart from the one-integer messages
of phase 1, every message during SSL authentication has the form
(`condor_auth_ssl.cpp:1527-1560`; the `net.Conn` adapter
`CEDARTLSConnection`, `ssl_auth.go:361`):

```
int64  status         ; sender's current state, see below
int64  len            ; may be 0
bytes  raw TLS record data (len bytes)
<EOM>
```

Status codes (`condor_auth_ssl.h:31-38`, `ssl_auth.go:33-44`):

| Value | Name | Meaning |
|---:|---|---|
| −1 | `ERROR` | local failure; a receiver treats it like `QUITTING` |
| 0 | `A_OK` | setup succeeded (phase 1 only) |
| 1 | `SENDING` | this side's TLS engine has more to write for the current phase |
| 2 | `RECEIVING` | this side's TLS engine is waiting for peer data |
| 3 | `QUITTING` | fatal: abandon the mechanism (the method loop of §5.2 may try another) |
| 4 | `HOLDING` | this side has locally completed the current phase and is standing by |

Within each phase, messages alternate in a fixed direction order (who
starts is fixed per phase, below). Every message carries the sender's
current status, so each side always knows the peer's state; **a phase
ends when both sides are in `HOLDING`**, and either side saying
`QUITTING` (or `ERROR`) aborts the mechanism. Receivers MUST bound
buffered TLS data (both implementations cap it at 1 MiB,
`AUTH_SSL_BUF_SIZE`).

### 5.4.2 Phase 1: Status Share

Each side reports whether its local setup (TLS library, contexts,
credential loading) succeeded, as a bare one-integer message — the
server first:

```
Server -> Client : int64 status   <EOM>
Client -> Server : int64 status   <EOM>
```

Both MUST be `A_OK` (0) to continue (`condor_auth_ssl.cpp:1486-1524`,
`ssl_auth.go:198-243`).

### 5.4.3 Phase 2: TLS Handshake

The **client sends first**. Each side drives its TLS engine (client
handshake / server handshake) against memory buffers, then sends
whatever records the engine produced, with status `RECEIVING`/`SENDING`
while the engine reports the handshake incomplete, or `HOLDING` once
the engine reports success; received bytes are fed back into the
engine. `len = 0` messages are legal and common — e.g. the server's
final message when TLS itself finished with the client's `Finished`
record but the server must still announce `HOLDING`. A fatal TLS
error (bad certificate, protocol mismatch) becomes `QUITTING`. Both
implementations require TLS 1.2 or newer.

**Certificate verification.** After the handshake phase completes, each
side checks the peer certificate. The client MUST verify the server's
chain against its CA bundle and match the hostname (CN or
subjectAltName, wildcards allowed) against the connected address or the
`alias=` from the sinful string (`condor_auth_ssl.cpp:71-136`). In SSL
mode the server requests (and deployment policy may require) a client
certificate; in SCITOKENS mode the client typically presents none. A
side whose verification fails does not abort abruptly: it stays in the
message rhythm and reports `QUITTING` in its next message (the client,
whose next phase-3 step is a receive, first reads and discards the
server's key message, then sends `QUITTING`;
`condor_auth_ssl.cpp:570-587`).

### 5.4.4 Phase 3: Session-Key Transport

The **server sends first**. The server generates 256 random bytes
(`AUTH_SSL_SESSION_KEY_LEN`) and writes them through the TLS engine;
the TLS records appear in the usual `[status, len, bytes]` messages.
The client reads until its engine yields the 256 bytes, then goes
`HOLDING` (typically one round trip: `S->C [HOLDING, key record]`,
`C->S [HOLDING, 0]`). This value is the **mechanism key**: it wraps
the key-exchange step of §5.7, and for legacy peers seeds the channel
key; when ECDH (§4.6) is in use the channel key comes from ECDH
instead, but this phase still runs.

### 5.4.5 Phase 4 (SCITOKENS): Token Transfer

The **client sends first**. Inside the TLS channel the client writes

```
uint32  token length (big-endian, 4 bytes — not a CEDAR int)
bytes   the SciToken (a serialized JWT)
```

(`condor_auth_ssl.cpp:658-675`). The server peeks the length, rejects
zero, reads exactly `4 + length` bytes, and validates the JWT: the
signature against the issuer's published keys, `iss`, `exp`, audience,
and scopes — and immediately attempts to *map* the token identity
(`iss`,`sub`) to a canonical user. Validation or mapping failure makes
the server answer `QUITTING` — which is how the client learns its token
was rejected; success is `HOLDING` and the usual both-`HOLDING` rule
ends the phase (`condor_auth_ssl.cpp:996-1120`).

### 5.4.6 Identity

- **SSL mode:** the authenticated name is the client certificate's
  subject DN (or an anonymous placeholder if policy allowed no client
  certificate), mapped by the server's map file.
- **SCITOKENS mode:** the authenticated name derives from the token's
  `iss` and `sub`; the TLS-level client identity is ignored.

After phase 3/4 the TLS engines are discarded; the mechanism key and
the §5.7 exchange feed the CEDAR channel protection of §4.6.

### 5.4.7 Worked Example

A SCITOKENS authentication, one line per CEDAR message
(`[status, len]`; TLS byte counts illustrative):

```
Phase 1 — status share
  S -> C : [0]                       server setup OK
  C -> S : [0]                       client setup OK

Phase 2 — TLS handshake (client first)
  C -> S : [2, 289]   ClientHello                       client: RECEIVING
  S -> C : [2, 1620]  ServerHello…ServerHelloDone       server: RECEIVING
  C -> S : [4, 133]   …Finished; client engine done  -> client: HOLDING
  S -> C : [4, 51]    …Finished; server engine done  -> server: HOLDING
                      both HOLDING -> phase over
  (client verifies server chain + hostname; server notes no client cert)

Phase 3 — session key (server first)
  S -> C : [4, 285]   TLS record: 256-byte random key   server: HOLDING
  C -> S : [4, 0]     key read                       -> client: HOLDING

Phase 4 — SciToken (client first)
  C -> S : [4, N]     TLS record: uint32 len ‖ JWT      client: HOLDING
  S -> C : [4, 0]     token valid, identity mapped   -> server: HOLDING
```

The server then knows the client as e.g.
`https://cms-auth.web.cern.ch,bbockelm` → mapped user
`cmsuser@example.net`, the §5.7 exchange runs (wrapped with the
256-byte key), and the post-auth ad of §4.7 reports the mapped user.

Had the token been expired, the last message would instead be
`S -> C : [3, 0]` (`QUITTING`); the client would clear the SCITOKENS
bit and re-enter the method loop of §5.2.

## 5.5 FS and FS_REMOTE

Filesystem authentication proves that client and server run as the same
identity on a shared filesystem: only that user (or root) can create a
directory the server can verify ownership of. It provides no key
material and no protection against a network man-in-the-middle; it is
meant for same-host connections (`FS`) or hosts sharing a trusted
network filesystem (`FS_REMOTE`).

```
Msg 1  Server -> Client:  string path        <EOM>
Msg 2  Client -> Server:  int64  status      <EOM>   ; 0 = mkdir succeeded
Msg 3  Server -> Client:  int64  result      <EOM>   ; 0 = ownership verified
```

(`condor_auth_fs.cpp:39-240`, `fs_auth.go:84-280`.)

- The server chooses a unique path under `/tmp` (FS; form
  `FS_<random>`) or under the configured shared directory (FS_REMOTE;
  form `FS_REMOTE_<host>_<pid>_<random>`); an empty string signals
  server-side failure.
- The client creates the directory with mode 0700 and reports status.
- The server `lstat`s the path, verifies it is a directory owned by the
  claimed uid (and not a symlink), reports the result, and both sides
  remove it. The authenticated name is the client's local username.

The Go implementation hardens the client against a malicious server by
validating the proposed path: it must be a direct child of the expected
base directory and its leaf must match `^FS_[A-Za-z0-9]{1,16}$` (FS) or
`^FS_REMOTE_[A-Za-z0-9._\-]+_[0-9]+_[A-Za-z0-9]{1,16}$` (FS_REMOTE)
(`fs_auth.go:71-82`). Clients SHOULD apply such validation; without it
a hostile server can probe or create directories at arbitrary
client-writable locations.

## 5.6 CLAIMTOBE

The client simply asserts an identity; nothing is verified and no key
is produced. Both implementations support it for testing and for
deliberately unauthenticated read-only setups. It MUST NOT be enabled
where authentication has security value.

## 5.7 Mechanism Key Exchange

After a mechanism succeeds, and before the channel switches to the
negotiated key, the legacy key-exchange step runs
(`authentication.cpp:852-941`, mirrored in Go):

```
Server -> Client : int64 hasKey (0|1)   <EOM>
if hasKey == 1:
  Server -> Client :
      int64  keyLength      ; plaintext key bytes
      int64  protocol       ; crypto protocol enum
      int64  duration
      int64  wrappedLen
      bytes  wrapped key (wrappedLen bytes)
      <EOM>
```

The key is "wrapped" (encrypted) with a cipher keyed by the mechanism's
own derived key: `W` for the AKEP2 mechanisms (§5.3.3), or the 256-byte
session key transported through TLS for SSL/SCITOKENS (§5.4.4). When
ECDH key agreement (§4.6) is in effect, this step is either skipped
(`hasKey = 0`) or its result is superseded by the ECDH-derived
AES-256-GCM key; the message flow is retained for compatibility.

## 5.8 Security Considerations

- **Downgrade resistance.** The negotiation loop and policy ads are
  cleartext; their integrity rests entirely on the transcript binding
  of §1.4.2 (when encryption is enacted) and on the mechanisms' mutual
  authentication. If policy permits `Encryption = NEVER` *and* a
  keyless mechanism (FS, CLAIMTOBE), an active attacker can strip
  stronger options; deployments requiring security MUST set
  authentication and encryption to `REQUIRED`.
- **IDTOKEN theft.** IDTOKENs are bearer-like: possession of the JWT
  (whose signature is the AKEP2 secret) suffices to authenticate.
  Tokens SHOULD carry `exp` and narrow `scope`/`sub` claims.
- **FS path trust.** See §5.5; validate server-supplied paths.
- **CCB interaction.** On a CCB-reversed connection (§3.4), the
  security handshake runs with the original client as the handshake
  client; the `CCB_REVERSE_CONNECT` hello itself is unauthenticated and
  is trusted only via the random connect id.
- **Randomness.** Nonces (`rA`, `rB`), connect ids, session ids, and
  ECDH keys MUST come from a cryptographically secure RNG.

## 5.9 Worked Example: Negotiation, IDTOKENS, Post-Auth

Continuing the example of §4.11: the client offered
`AuthMethods = "IDTOKENS,FS"`, the server's list is `"SSL,IDTOKENS,FS"`.

**Method negotiation.** The client's usable set is the intersection
{IDTOKENS, FS} = bits 2048 | 4:

```
Client -> Server : 2052              ; TOKEN(2048) | FS(4)
Server -> Client : 2048              ; first of "SSL,IDTOKENS,FS" in the mask:
                                     ; SSL(256) absent -> TOKEN selected
```

**IDTOKENS (AKEP2).** The client holds the IDTOKEN

```
header : { "alg": "HS256", "kid": "POOL" }
payload: { "sub": "bbockelm@example.net", "iss": "example.net",
           "iat": 1750000000, "jti": "d3adb33f…" }
```

and sends **Msg 1** with `status = 0`,
`A = "bbockelm@example.net"`,
`T = base64url(header) "." base64url(payload)` (signature omitted), and
a fresh 256-byte nonce `rA`.

The server looks up signing key `POOL`, recomputes
`sig = HMAC-SHA256(T, HKDF(pool_key, "htcondor", "master jwt"))`,
derives `K`/`K'` from `sig` and `T` (§5.3.1), checks the claims
(`iss` matches its trust domain; no `exp` present), and answers
**Msg 2** with `B = "condor@example.net"`, its nonce `rB`, and
`mac1 = HMAC-SHA256(K, B‖A‖rA‖rB)`.

The client, holding the complete JWT, derives the same `K`/`K'`
directly from the token's signature, verifies `mac1` — proof the server
knows the pool key — and sends **Msg 3** with
`mac2 = HMAC-SHA256(K, A‖rB)`. The server verifies `mac2` — proof the
client holds a validly signed token — and the authenticated name is the
token's `sub`, which the server's map file maps to canonical
`bbockelm@example.net`.

Both sides derive `W = HKDF-SHA256(rB, salt=K', "session key")`; the
mechanism key exchange (§5.7) then runs (`hasKey = 0` here, since the
ECDH agreement of §4.6 supplies the channel key), encryption switches
on, and the server issues the post-auth ad shown in §4.11 step 5 with
`User = "bbockelm@example.net"`.

**Failure and fallback.** Had the token been rejected (say, an unknown
`kid`), the server would send a failing `status` inside the AKEP2
exchange; the client clears the TOKEN bit and the loop continues:

```
Client -> Server : 4                 ; only FS remains
Server -> Client : 4                 ; FS selected
     …  FS sub-protocol (§5.5) runs …
```

and if that too failed, `Client -> Server : 0` abandons authentication —
which, with `Authentication = "REQUIRED"` on either side, fails the
whole command.

## 5.10 Implementation Differences

| Aspect | C++ | Go |
|---|---|---|
| Methods beyond the common set | KERBEROS, MUNGE, NTSSPI, ANONYMOUS | not implemented |
| SCITOKENS validation | via libscitokens + mapfile | native JWT validation |
| Post-auth mapping | unified map file (`CERTIFICATE_MAPFILE`) | `PostAuthPolicy` callback |
| FS path validation | trusts server path | regex + base-directory checks |
| PASSWORD | distinct v1 protocol | AKEP2 core shared with TOKEN |
