# Chapter 2. Serialization of Complex Objects

The composite data structure of the HTCondor ecosystem is the
**ClassAd**: an unordered set of `name = expression` pairs, where
expressions belong to the ClassAd language (literals, attribute
references, operators, function calls, lists, and nested record
expressions). CEDAR does not define a binary encoding for the ClassAd
expression tree; instead, each attribute is shipped as *text* in the
"old ClassAds" syntax, wrapped in the primitive encodings of Chapter 1.

Implementations: `src/condor_utils/classad_oldnew.cpp` (C++),
`golang-cedar/message/classad.go` with the expression language in
`golang-classads/` (Go).

## 2.1 Wire Format of a ClassAd

A ClassAd is serialized as (`classad_oldnew.cpp:590-901`,
`classad.go:94-186`):

```
classad :=
    int64   numExprs                 ; number of attribute strings that follow
    attr*   numExprs attributes
    string  MyType                   ; usually ""
    string  TargetType               ; usually ""

attr :=
    string  "Name = <expression text>"          ; ordinary attribute
  | string  "ZKM" , secret "Name = <expr text>" ; secret attribute (§2.4)
```

- `numExprs` is a CEDAR integer (8 bytes, big-endian). Receivers MUST
  bound it; the C++ receiver rejects counts outside 0…1,000,000.
- Each attribute is a single CEDAR string of the exact form
  `Name` + `" = "` (space, equals, space) + the unparsed expression.
- The two trailing type strings are always present unless the peer
  agreed to the `NO_TYPES` option (§2.5); in modern usage both are
  empty strings.

Example — the byte sequence for `[ JobId = 1; Priority = 0 ]` on a
plaintext channel:

```
00 00 00 00 00 00 00 02                          numExprs = 2
4A 6F 62 49 64 20 3D 20 31 00                    "JobId = 1\0"
50 72 69 6F 72 69 74 79 20 3D 20 30 00           "Priority = 0\0"
00                                               MyType = ""
00                                               TargetType = ""
```

If the sending ClassAd is *chained* to a parent ad (C++ feature), the
parent's attributes are serialized first and the child's attributes
follow, overriding duplicates by name at the receiver.

## 2.2 Expression Text

The expression portion is produced by the ClassAd unparser and parsed by
the ClassAd parser on receipt. The forms relevant to interoperability:

- **Booleans:** `true` / `false` (case-insensitive on input).
- **Integers:** optional sign followed by decimal digits, e.g. `-30`.
- **Reals:** decimal or scientific notation, e.g. `1.5`, `1.23e10`.
  (Note: as expression *text*; the binary float encoding of §1.3.3 is
  not used inside ClassAds.)
- **Strings:** double-quoted with backslash escapes
  (`\"`, `\\`, `\n`, `\r`, `\t`, …).
- **Lists:** `{ expr, expr, ... }`.
- **Nested ClassAds:** `[ name = expr; name = expr; ... ]` — carried as
  text inside the parent attribute's string; nesting does not introduce
  additional wire structure.
- **Arbitrary expressions:** transmitted verbatim, e.g.
  `Requirements = (Memory >= 1024) && (Cpus > 0)`.

Both implementations include a "fast path" that recognizes simple
literals (booleans, integers, reals, short quoted strings without
escapes) without invoking the full parser
(`classad_oldnew.cpp:378-408`, `classad.go:320-363`); this is an
optimization and MUST NOT change the accepted language.

## 2.3 Special Attributes

- **`ServerTime`** — when the sender requests the `SERVER_TIME` option,
  an attribute `ServerTime = <unix-seconds>` is counted in `numExprs`
  and sent first (`classad_oldnew.cpp:749-752`, `classad.go:136-140`).
  It lets the receiver estimate clock skew relative to the sender.

## 2.4 Secret (Private) Attributes

Some attributes carry capabilities that must not appear in cleartext
even when the channel as a whole is unencrypted (e.g. a startd claim
identifier). Two generations of policy identify them
(`classad_oldnew.cpp:698-816`, `classad.go:62-89`):

- **V1:** a fixed name list — `Capability`, `ClaimId`, `ClaimIds`,
  `ClaimIdList`, `ChildClaimIds`, `TransferKey`.
- **V2:** any attribute whose name begins with `_condor_priv`
  (case-insensitive), plus any names the caller explicitly designates.

When the stream has a usable key and the attribute is private, the
sender emits the marker string `"ZKM"` (a normal CEDAR string,
`5A 4B 4D 00`) in place of the attribute, immediately followed by the
`"Name = value"` string sent as a **secret** — i.e. encrypted with the
channel key even if bulk encryption is off, and therefore
length-prefixed per §1.3.4. A receiver that sees `ZKM` MUST decrypt the
next string and parse it as the real attribute. The `ZKM` marker string
itself counts as the attribute's slot in `numExprs`.

If no key is available, the sender either transmits the attribute in
cleartext or omits private attributes entirely (the `NO_PRIVATE`
option). Senders SHOULD omit V2 private attributes when talking to
peers older than HTCondor 9.9.0, which do not understand them.

The Go implementation currently *detects* private attributes and
supports attribute whitelisting, but does not yet emit the `ZKM`
encrypted form (`classad.go:154`, TODO).

## 2.5 Serialization Options

The following sender options change the wire image and therefore must be
consistent with the receiver's expectations (`classad_oldnew.h:36-80`):

| Option | Effect on the wire |
|---|---|
| `NO_PRIVATE` | private attributes omitted |
| `NO_TYPES` | trailing `MyType`/`TargetType` strings omitted (receiver must use the matching option) |
| `SERVER_TIME` | prepends the `ServerTime` attribute (§2.3) |
| `NON_BLOCKING` | sender-side buffering only; no wire change |
| whitelist | only listed attributes (plus, by default, attributes referenced by their expressions) are serialized |

Receiver-side options (`GET_CLASSAD_FAST`, `LAZY_PARSE`, `NO_CACHE`,
`NO_CLEAR`) affect only local parsing behavior.

## 2.6 Pre-Trust ClassAds: the "POD" Form

During the security handshake (Chapter 4), ClassAds are exchanged before
the peer is authenticated. For this purpose the C++ implementation
provides `getPODClassAd` ("plain old data",
`classad_oldnew.cpp:114-199`), which accepts the same wire format but
with a restricted grammar:

- at most **100** attributes;
- attribute values MUST be literals (booleans, integers, reals,
  strings) — expressions, lists, nested ads, and function calls are
  rejected.

Handshake implementations MUST apply equivalent restrictions when
parsing pre-authentication ads; the Go implementation enforces size
bounds via `GetClassAdWithMaxSize` (`classad.go:197-287`).

## 2.7 Worked Example: A Machine Ad

A slot ad as a collector might return it, with non-empty type strings
and a mix of value kinds:

```
Name         = "slot1@worker-01.example.net"
OpSys        = "LINUX"
Memory       = 2048
Requirements = (Memory >= 1024) && (Cpus > 0)
```

serializes (plaintext channel) as:

```
00 00 00 00 00 00 00 04                           numExprs = 4
4E 61 6D 65 20 3D 20 22 73 6C 6F 74 31 40 …  22 00
                    "Name = \"slot1@worker-01.example.net\"\0"
4F 70 53 79 73 20 3D 20 22 4C 49 4E 55 58 22 00
                    "OpSys = \"LINUX\"\0"
4D 65 6D 6F 72 79 20 3D 20 32 30 34 38 00
                    "Memory = 2048\0"
52 65 71 75 69 72 65 6D 65 6E 74 73 20 3D 20 28 …  29 00
                    "Requirements = (Memory >= 1024) && (Cpus > 0)\0"
4D 61 63 68 69 6E 65 00                           MyType = "Machine"
4A 6F 62 00                                       TargetType = "Job"
```

Every attribute — including the integer and the boolean expression — is
carried as expression *text* inside a CEDAR string; the quotes around
`slot1@…` and `LINUX` are part of that text (ClassAd string literals),
while `2048` and the `Requirements` expression are unquoted.

The same ad including a V1 secret attribute `ClaimId` on a stream that
holds a key but has bulk encryption off (§2.4) would carry `numExprs = 5`
and, in place of the fifth attribute string:

```
5A 4B 4D 00                                       marker string "ZKM"
00 00 00 00 00 00 00 2C                           secret length = 44 (example)
xx × 44                                           ciphertext of
                                                  "ClaimId = \"<10.0.0.17:9618>#1719…#42…\"\0"
```

— i.e. the secret is a length-prefixed encrypted string (§1.3.4), and
the `ZKM` marker occupies the attribute's slot in the count.

## 2.8 Other Composite Values

CEDAR has no other framed container types. Ad-hoc composites in the
protocols of Chapters 3–5 are built directly from primitives, e.g.
"length integer followed by raw bytes" for cryptographic material, or
comma/space-separated items inside a single string (method lists,
`ValidCommands`, CCB contact lists). ClassAd *lists of ads* are sent as
an integer count followed by that many serialized ClassAds, typically
with a leading command integer (collector query responses).

There is no compression at any CEDAR layer.
