# Chapter 1. Message Framing and Primitive Encodings

This chapter specifies the lowest layer of CEDAR: how a byte stream is
divided into frames and messages, how primitive values are encoded inside
a message, and how frames are protected with AES-256-GCM once a channel
key has been established.

## 1.1 The Stream / Message / Frame Model

A CEDAR **stream** is a bidirectional, reliable, ordered byte channel —
in practice a TCP connection. Application data is exchanged as
**messages**. A message is an ordered sequence of encoded values
(integers, strings, ClassAds, raw bytes) terminated by an explicit
end-of-message marker. The receiver cannot begin decoding a value until
it knows the message boundary semantics; both implementations buffer an
entire frame before yielding data to the decoder.

Each message is carried as one or more **frames**. Senders flush a frame
when their buffer reaches an implementation-chosen threshold (the C++
implementation defaults to a 4 KiB buffer; the Go implementation targets
16 KiB frames, `golang-cedar/message/message.go`), and mark the final
frame of the message with the end-of-message flag.

There is no connection preamble: the first bytes on a fresh CEDAR TCP
connection are the header of the first frame.

## 1.2 Frame Header

Every frame begins with a 5-byte header
(`src/condor_io/reli_sock.cpp:45`, `golang-cedar/stream/stream.go:82-96`):

```
 Offset  Size  Field
 ------  ----  -----------------------------------------------------
      0     1  end-of-message flag:
               0 = additional frames of this message follow
               1 = this frame completes the message
    1-4     4  payload length in bytes, unsigned, big-endian
    5-…     N  payload (N = payload length)
```

Constraints (`src/condor_io/reli_sock.cpp:901-931`):

- The payload length MUST be greater than 0 and MUST NOT exceed
  **1,048,576 bytes (1 MiB)**. A receiver MUST reject frames outside
  this range and abort the connection.
- The C++ receiver accepts end-of-message flag values 0 through 10 and
  treats any nonzero value as end-of-message; values 2–10 are reserved.
  Senders MUST send only 0 or 1.

Messages larger than 1 MiB are simply carried in multiple frames; there
is no limit on total message size at the framing layer (higher layers
impose their own limits).

> **Legacy note.** When the legacy per-frame MAC ("MD") mode is active
> the header is extended by a 16- or 32-byte MAC following the 5 fixed
> bytes. This mode is tied to the legacy ciphers and is out of scope for
> this document; the AES-GCM mechanism of §1.4 provides integrity via
> the GCM authentication tag instead and uses the unmodified 5-byte
> header.

## 1.3 Primitive Encodings

The following encodings apply to the byte sequence inside message
payloads (after decryption, when encryption is active). They are
implemented in `src/condor_io/stream.cpp` and
`golang-cedar/message/message.go`.

### 1.3.1 Integers

**All** integer types — `char`-sized values excepted — are widened to 64
bits and transmitted as **8 bytes, big-endian, two's complement**
(`stream.cpp:559-612`, `message.go:502-529`). This applies uniformly to
`short`, `int`, `long`, `long long`, and their unsigned variants; the Go
`PutInt32`/`PutInt64`/`PutUint32` helpers all delegate to the same 8-byte
encoding. Booleans are transmitted as integers 0 or 1.

### 1.3.2 Single Bytes and Raw Byte Arrays

A `char`/byte value is transmitted as a single octet. Raw byte arrays
(`put_bytes`) are transmitted verbatim with no length prefix or
terminator; the surrounding protocol must convey the length (typically
via a preceding integer field).

### 1.3.3 Floating-Point Values

Floating-point values are **not** sent as IEEE-754 bit patterns.
A `float` is first widened to `double`; the double is then decomposed
with `frexp()` into a normalized fraction and a binary exponent, and both
parts are sent as CEDAR integers (§1.3.1) — 16 bytes total
(`stream.cpp:623-629, 852-862`, `message.go:291-305, 531-550`):

```
frac, exp = frexp(d)                    // 0.5 <= |frac| < 1.0
fracInt   = trunc(frac * 2147483647)    // scaled by 2^31 - 1
wire      = int64(fracInt) || int64(exp)
```

The receiver reconstructs `ldexp(fracInt / 2147483647.0, exp)`. This
encoding is portable but lossy in the low-order bits of the mantissa
(the fraction is scaled into 31 bits).

### 1.3.4 Strings

Strings are sequences of non-NUL octets. Two encodings exist, selected
by whether channel encryption is currently active
(`stream.cpp:633-660, 875-898, 973-1021`, `message.go:309-364, 554-609`):

**Plaintext channel:**

```
string  := octets* 0x00        ; NUL-terminated
null    := 0xFF                ; a "no string" marker (BIN_NULL_CHAR)
```

An *empty* string is the single byte `0x00`; an *absent* (NULL) string is
the single byte `0xFF`.

**Encrypted channel:** because the decoder cannot scan ciphertext for a
terminator, strings are length-prefixed:

```
string  := int64(len) octets[len]
```

where `len` counts the octets **including** the trailing NUL, which is
still present, and the length is encoded as a CEDAR integer (8 bytes,
big-endian). The C++ receiver rejects encrypted string lengths above
10,000,000 bytes; the Go implementation offers `GetStringWithMaxSize`
for the same denial-of-service protection.

Senders MUST NOT embed a NUL inside a string (the Go implementation
truncates at the first NUL).

## 1.4 Frame Protection with AES-256-GCM

Once the command-protocol handshake (Chapter 4) has produced a 32-byte
symmetric key and selected the `AES` crypto method, every subsequent
frame payload is protected with AES-256-GCM
(`src/condor_io/reli_sock.cpp:984-1063, 1213-1293`,
`golang-cedar/stream/stream.go:578-805`). The 5-byte frame header
remains in cleartext; the length field gives the length of the
*ciphertext* payload.

### 1.4.1 Ciphertext Layout

```
first encrypted frame:
    header(5) || IV(16) || ciphertext || tag(16)
subsequent frames:
    header(5) || ciphertext || tag(16)
```

- The **IV** is 16 random bytes chosen by each sender and transmitted
  in the clear ahead of the first ciphertext it produces.
- The GCM **authentication tag** is 16 bytes and trails the ciphertext.
- Per-frame IVs after the first are derived deterministically: each
  direction maintains a frame counter, and the leading 32-bit word of
  the IV is the base value plus the counter (big-endian); the remaining
  12 bytes are unchanged. Senders and receivers keep independent
  counters per direction, so IVs never repeat under a given key.

### 1.4.2 Additional Authenticated Data and Transcript Binding

The GCM AAD for every frame includes the 5-byte cleartext frame header,
so a tampered length or end-of-message flag causes tag verification to
fail.

The **first** encrypted frame in each direction additionally binds the
plaintext pre-handshake transcript into the channel: both sides maintain
SHA-256 digests over all bytes sent and all bytes received *before*
encryption was enabled (capped at the first 1 MiB in each direction).
These digests are finalized when encryption starts and prepended to the
AAD of the first encrypted frame (`reli_sock.cpp:1231-1277`):

```
AAD(first frame, sender)   = sent_digest(32) || received_digest(32) || header(5)     ; 69 bytes
AAD(first frame, receiver) = received_digest(32) || sent_digest(32) || header(5)
AAD(later frames)          = header(5)
```

Because the security negotiation of Chapter 4 happens in cleartext, this
binding detects a man-in-the-middle who altered the negotiation (for
example, downgrading the offered methods): the two sides' transcript
digests will disagree, and the first encrypted frame will fail
authentication.

### 1.4.3 Secrets on Plaintext Channels

Even when full-channel encryption was not negotiated, individual values
may be sent encrypted ("secrets", used e.g. for `ZKM` ClassAd attributes,
§2.4) if a key is available: the sender enables the cipher for just those
bytes. The string encoding then follows the encrypted form of §1.3.4.

## 1.5 Worked Examples

### 1.5.1 A Single-Frame Message

A message containing three values — the integer `42`, the string
`"pool"`, and the real `0.25` — fits in one frame. The payload is
8 + 5 + 16 = 29 bytes:

```
01                        frame header: end-of-message = 1
00 00 00 1D               frame header: payload length = 29
00 00 00 00 00 00 00 2A   int 42            (8-byte big-endian)
70 6F 6F 6C 00            "pool"            (NUL-terminated)
00 00 00 00 3F FF FF FF   0.25, fraction:   trunc(0.5 × (2^31−1)) = 1073741823
FF FF FF FF FF FF FF FF   0.25, exponent:   −1
```

(`frexp(0.25)` yields fraction 0.5, exponent −1.) Note the encoding is
lossy: the receiver computes `ldexp(1073741823 / 2147483647.0, -1)` =
0.2499999998835…, not exactly 0.25 (§1.3.3).

### 1.5.2 A Message Spanning Multiple Frames

Frame boundaries are chosen by the sender's buffering, not by value
boundaries; a receiver MUST treat the concatenated payloads as one
contiguous byte sequence. Consider a message consisting of a single
string of 6,000 `'A'` characters (6,001 payload bytes including the
NUL), sent by the C++ implementation with its default 4 KiB buffer:

```
frame 1:
00                        end-of-message = 0 (more frames follow)
00 00 10 00               payload length = 4096
41 41 41 … 41             first 4096 bytes of the string

frame 2:
01                        end-of-message = 1 (message complete)
00 00 07 71               payload length = 1905
41 41 … 41 00             remaining 1904 'A's and the terminating NUL
```

The string value is split mid-value across the two frames; equally, a
single frame routinely carries many small values. The Go implementation
would send the same message split at its 16 KiB target instead — i.e.
as one frame — and both forms are valid: framing carries **no**
semantic information beyond the message boundary.

### 1.5.3 An Encrypted Frame

The message of §1.5.1 sent as the *first* frame after AES-256-GCM was
enabled (§1.4). The 29 plaintext bytes yield 29 ciphertext bytes; the
payload gains the 16-byte IV (first frame only) and the 16-byte tag,
so the header advertises 61 bytes:

```
01                        end-of-message = 1            (cleartext)
00 00 00 3D               payload length = 61           (cleartext)
xx × 16                   IV, randomly chosen by sender (cleartext)
xx × 29                   AES-256-GCM ciphertext of the 29 payload bytes
xx × 16                   GCM authentication tag
```

with

```
AAD = SHA-256(bytes sent before encryption)     (32 bytes)
   || SHA-256(bytes received before encryption) (32 bytes)
   || 01 00 00 00 3D                            (this frame's header)
```

A subsequent frame carrying the same message would omit the IV
(payload length 45) and use only the 5 header bytes as AAD; its IV is
the transmitted IV with the leading 32-bit word incremented by the
per-direction frame counter.

## 1.6 UDP Framing (C++ Only)

The C++ implementation also carries CEDAR messages over UDP
(*SafeSock*, `src/condor_io/SafeMsg.cpp`, `src/condor_includes/SafeMsg.h`).
A message is split into datagrams of at most 60,000 bytes, each carrying
a 25-byte header beginning with the magic string `"MaGic6.0"`, a
last-fragment flag, a 16-bit sequence number, and a message identifier
(sender IP, PID, timestamp, message number) used for reassembly. An
optional 10-byte extension header tagged `"CRAP"` carries per-datagram
crypto/MAC parameters keyed by security-session identifiers.

The Go implementation does not implement UDP transport, and modern
HTCondor deployments increasingly disable it (`noUDP`, §3.1). UDP
framing is therefore not specified further in this document.

## 1.7 Implementation Differences

| Aspect | C++ | Go |
|---|---|---|
| UDP (SafeSock) | yes | no |
| Legacy MAC header ("MD" mode) | yes | not implemented (`stream.go:83` TODO) |
| Frame flush threshold | 4 KiB buffer | 16 KiB target frame |
| Max frame payload | 1 MiB | 1 MiB |
| Bounded-size string reads | encrypted strings capped at 10 MB | explicit `GetStringWithMaxSize` |
