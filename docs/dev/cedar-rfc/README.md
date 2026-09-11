# The CEDAR Protocol

**Status:** Draft
**Editors:** Derived from the HTCondor C++ implementation and the Go
implementation in `golang-cedar` / `golang-classads` / `golang-ccb`.

## Abstract

CEDAR is the wire protocol used by all components of the HTCondor
distributed high-throughput computing system. It provides message-oriented
communication over TCP (and, in the C++ implementation, UDP), a portable
serialization format for primitive values and ClassAds, connection routing
through a shared listening port and through the Condor Connection Broker
(CCB) for daemons behind NATs and firewalls, and a command protocol with
integrated authentication, encryption, and integrity protection.

This document specifies CEDAR as implemented by two independent codebases:
the reference C++ implementation in HTCondor (`src/condor_io` and related
directories) and the Go implementation (`golang-cedar` and companion
modules). Where the two implementations differ, the difference is noted.
Per the scope of this document, channel encryption and integrity are
described only for the modern AES-256-GCM mechanism; the legacy Blowfish
and Triple-DES ciphers and the standalone per-frame MAC ("MD") mode are
mentioned only where their existence affects the wire format.

## Table of Contents

1. [Message Framing and Primitive Encodings](01-message-framing.md)
   — streams, messages, frames, the 5-byte frame header, encodings for
   integers, floating-point values, strings, and byte arrays, and the
   AES-256-GCM frame protection mechanism.
2. [Serialization of Complex Objects](02-serialization.md)
   — the on-the-wire representation of ClassAds (`putClassAd` /
   `getClassAd`), secret attributes, and the restricted "POD" ClassAd
   used before a channel is trusted.
3. [Transport over TCP: Addresses, Shared Port, and CCB](03-transport.md)
   — the *sinful string* address format, direct TCP connection
   establishment, the Shared Port protocol, and the Condor Connection
   Broker reverse-connection protocol.
4. [The Command Protocol and Security Negotiation](04-command-protocol.md)
   — command integers, the `DC_AUTHENTICATE` wrapper, the security
   policy handshake, ECDH key agreement, activation of AES-GCM channel
   protection, and security session caching and resumption.
5. [Authentication Protocols](05-authentication.md)
   — the method negotiation loop and the individual mechanisms
   implemented in both codebases: TOKEN/IDTOKENS and PASSWORD (AKEP2),
   SSL, SCITOKENS, FS/FS_REMOTE, and CLAIMTOBE.

## Conventions and Terminology

The key words **MUST**, **MUST NOT**, **SHOULD**, **SHOULD NOT**, and
**MAY** are to be interpreted as described in RFC 2119.

- **Stream** — a bidirectional byte channel between two endpoints,
  normally a TCP connection (called a *ReliSock* in the C++ code).
- **Frame** (also *packet* in the C++ code) — the unit of framing on a
  stream: a fixed header followed by a payload.
- **Message** — a logical unit of application data, carried as one or
  more frames; the last frame of a message is marked by an end-of-message
  flag. Called *end_of_message()* / `FinishMessage()` boundaries in the
  implementations.
- **Command** — an integer identifying the operation a client requests of
  a daemon; the first datum of the first message on a connection.
- **ClassAd** — HTCondor's schema-free attribute/expression record; the
  standard composite data structure carried over CEDAR.
- **Sinful string** — HTCondor's textual address format,
  `<host:port?params>`.
- **Daemon** — a CEDAR server process. **Client** — the connecting party
  (which may itself be a daemon).

All multi-byte integers on the wire are transmitted in **network byte
order (big-endian)**. Byte layouts in this document are drawn with offset
0 transmitted first.

Source references use the form `path:line` and refer to the C++ tree at
`htcondor/` and the Go trees at `golang-cedar/`, `golang-ccb/`, and
`golang-classads/`.
