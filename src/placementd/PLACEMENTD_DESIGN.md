# The Placement Daemon (condor_placementd)

This document is an architectural overview of `condor_placementd`. It gives
developers the mental model needed to read the code in this directory:

- `placementd.cpp` holds the daemon, the CEDAR command handlers, and login.
- `placementd_db.{h,cpp}` holds the SQLite schema, migration, and data access.
- `placementd_reconcile.{h,cpp}` imports the map files.
- `placementd_edit.{h,cpp}` implements `PLACEMENT_EDIT`.

---

## 1. What the daemon is for

An access point (AP) often serves people who have no Unix account on it and
who authenticate with an external identity provider (IdP), such as CILogon or
an institutional OIDC server. The placementd turns such a person into an
HTCondor identity on the AP. A trusted front end, usually a web application,
authenticates the person with the IdP. It then asks the placementd to mint a
**placement token** on that person's behalf:

```
 person ──IdP login──▶ front end ──PLACEMENT_USER_LOGIN(identity)──▶ placementd
                                                                  │
            token ◀───────────────────────────────────────────────┘
 person ──submit with token (IDTOKENS)──▶ schedd
```

A placement token is an IDTOKEN signed with the pool's signing key. Its
subject is an **AP account** such as `alice@ap.example.org`, not the person's
IdP identity. Its authorizations are bounded to what that account was granted,
and it may carry a `project` claim. It is minted as a *capability* token, so
the subject and scope live in `TOKENS_DATABASE` (`idtokens_minting`) and the
project in `idtokens_minting_extra_claims`, not in the JWT. Minting into the
database requires `SEC_USE_TOKENS_DATABASE`. Any daemon that can open
`TOKENS_DATABASE` looks capability tokens up there when it validates them.

Two inputs say who may receive what:

- the **user map file** (`PLACEMENTD_MAPFILE`) maps identities to accounts,
  authorizations, projects and an expiry;
- the **authorizations map file** (`PLACEMENTD_AUTHORIZATIONS_MAPFILE`) gives
  display metadata for authorization names.

The placementd stores this information in SQLite and lets an administrator
create, edit and delete it over CEDAR. The map files remain supported and are
imported into the same tables.

---

## 2. Terminology

| Term | Meaning |
|---|---|
| **Identity** | A login name produced by the front end from the IdP, for example `alice@wisc.edu`. Its key attribute on the wire is `Identity`; the legacy commands call it `UserName`. An identity maps to exactly one account and may expire. |
| **Account** | The AP identity tokens are minted for, always canonical `user@domain`. Several identities may map to one account, for example the same person through two IdPs. |
| **Project** | A name an account may stamp into its token's `project` claim, matched case-insensitively. One of an account's projects may be its default. |
| **Authorization** | A name that may go into the token's bounding set, such as READ or WRITE, plus label, color and description for display. `requestable` marks the ones a front end offers as choices. Granted names that are not requestable, such as INSTRUCTOR, are still minted. |
| **Grant** | An account's link to an authorization or a project. Grants belong to the account, not the identity. |
| **Source** | Who owns a row. The reconcile maintains `mapfile` rows and `PLACEMENT_EDIT` maintains `internal` rows. Ownership moves only through explicit transfers: Adopt (§5.2), reclaim and hand-off (§4.4). |
| **Change set / modification** | Audit-log records: one change set per committed edit batch or per reconcile that changed something, and one modification per entity it touched. |

---

## 3. Data model

```
 Authorization ─┐                         ┌─ Project
                │ account_authorizations  │ account_projects (is_default)
                ▼                         ▼
                         Account
                            ▲
                            │ account_id
                         Identity (expires_at)
```

All tables live in `TOKENS_DATABASE`, next to the authentication layer's
`idtokens_minting*` tables and the pre-existing `placementd_tokens` issue log.
Every placementd table is prefixed `placementd_`. `placementd_db.cpp` holds the
full DDL. The points that matter:

- **Immutable ids.** Every entity row has a random UUID assigned at creation,
  kept across renames and map-file reloads, and never reused. The CEDAR API
  accepts either the UUID (`Id`) or the natural key: `Identity` for an
  identity, `Name` for the other types. Entity rowids never leave the daemon. The audit log's integer ids do, as
`Version`, `ModificationId` and `ChangesetId`.
- **Natural keys:**
  - identity: case-sensitive, as the old in-memory map was;
  - account: case-sensitive canonical `user@domain`;
  - project and authorization names: `COLLATE NOCASE`, as the old
    `strcasecmp` / `contains_anycase` comparisons were.
- **Grants on the account.** An account has one set of authorizations and
  projects, however its people log in. The map file can express something the
  schema cannot: two lines for the same account with different grants. §4.3
  covers how such conflicts are resolved.
- **No per-row audit columns.** A row carries only `modification_id`, a
  reference to its latest modification. `CreatedAt`, `UpdatedAt` and
  `ModifiedBy` are derived from the audit log (§7).
- **`Version` is the latest modification id.** It is unique and increasing
  across the database rather than per row. Clients treat it as opaque and send
  it back as `ExpectedVersion` for optimistic concurrency.
- **The token log** (`placementd_tokens`) keeps its original text columns as
  an audit record that survives deletes. It also records the UUIDs of the
  identity, account and project used at issue time, which revocation (§6.4)
  looks tokens up by; both are indexed, and the indexes are created at startup
  if a database lacks them.

### Schema versions and migration

`placementd_meta.schema_version` records the version. A database with no
`schema_version` row is version 1, the format before this design (only
`placementd_tokens`). At startup, inside one `BEGIN IMMEDIATE`, a version-1 database
gets the version-2 tables and the new token-log columns.

A database that claims a version newer than the daemon understands, or a
non-numeric one, is refused with `EXCEPT`. So is one that claims version 2 but
lacks one of the eight placementd tables or an entity table's
`modification_id` column. The first map-file reconcile then runs before any command is
registered, so the first login after an upgrade sees the imported data.

---

## 4. Map files and the reconcile

### 4.1 Formats

```
# PLACEMENTD_MAPFILE: identity  ap-account  authorizations  expiry  [projects]
alice@wisc.edu   *                     READ,WRITE             0           ProjA,ProjB
bob@idp.example  bob@ap.example.org    READ,WRITE,INSTRUCTOR  1767225600  ProjA

# PLACEMENTD_AUTHORIZATIONS_MAPFILE: name|label|color|description
READ|Read|normal|Get a list of jobs, examine jobs
```

- **`*` account column:** derives the account from the identity's part before
  the last `@`.
- **Bare account names:** a name with no `@` gets `@` and
  `PLACEMENTD_UID_DOMAIN` appended, falling back to `UID_DOMAIN`.
- **Expiry:** `0` or a negative value means never.
- **Projects:** the first project listed is the account's default.
- **Authorizations-file lines:** an empty label defaults to the name, and an
  empty color to `normal`. The code before this design rejected a line with
  empty fields.

### 4.2 When it runs, and what it skips

The reconcile runs at startup, on the 300 s timer, and on `condor_reconfig`.
Before reading, it `stat()`s both files. If both are unchanged since the last
successful run, it does nothing. A file modified within the last two seconds
never sets the "unchanged" signature, because a same-second rewrite could
otherwise be missed on coarse-timestamp filesystems.

The signature also covers the account domain and the reserved-account lists
(§6.2). A committed `PLACEMENT_EDIT` clears it, so the next pass is a full
one. The timer interval is fixed. A skipped run updates
`PlacementdMapfileLoadAttemptTime` but writes nothing.

Setting `PLACEMENTD_MAPFILE` to empty disables reconciling. The mapfile rows
stay in place, owned by `mapfile`, until an administrator adopts them (§5.2).

### 4.3 Parsing: deny-biased handling of bad input

Both files are parsed completely before the database is touched. The rule is
that **bad or conflicting input must never keep access that the file revokes**.

- **Unreadable or missing user map file:** the run is aborted, and existing
  rows are kept. A missing file is not an empty file. A warning is logged on
  every pass while it is missing. To revoke everyone, empty the file.
- **Missing or unreadable authorizations file:** the user file is still
  applied, including deletions and expiry changes. Existing mapfile
  authorizations keep their metadata and are not deleted. A `Missing` or
  `Unreadable` problem is reported, and the pass does not record an
  "unchanged" signature, since fixing permissions doesn't change the file's
  stat. A missing file from which nothing was ever imported counts as empty.
- **Malformed user line:** the identity named by its first field is
  quarantined, keeping its previous state, except that a parseable expiry on
  the line still tightens the identity's expiry. Malformed lines include a
  wrong field count, a non-numeric expiry, an invalid identity or account
  name, or a reserved account (§6.2). An invalid or privileged authorization
  or project name on a user line does not quarantine the line. That one name
  is dropped and reported as `Rejected`, and the rest of the line applies.
- **Revocation always applies.** A mapfile identity whose line was removed is
  deleted, and an earlier expiry is applied, even when the identity or its
  account is otherwise quarantined.
- **Duplicate identity lines:** the identity gets the earliest expiry. If the
  lines name the same account, their grants are merged as for conflicting
  lines below. If they name different accounts, the identity is quarantined.
- **Conflicting lines for one account:** lines that disagree on grants are
  merged:
  - authorizations are the **intersection**, compared case-insensitively;
  - projects are the **union**, since projects are accounting, not
    privilege;
  - the default is the first line's default. If the first line lists no
    projects, the account has no default.

  The conflict is reported. When the merge narrows an account's
  authorizations it is reported as `GrantsNarrowed`; when nothing remains, as
  `NoAuthorizations`. Such an account's logins are refused.
  `NoAuthorizations` also fires when every name on a single line was
  rejected.
- **Authorization lines** with a bad name or a privileged level are rejected.
  A malformed or duplicated authorization line quarantines only that
  authorization's display metadata. If no row exists and a user line grants
  that name, a placeholder is created (`requestable = 0`, label = name), so
  accounts that grant it are unaffected.

### 4.4 Apply: ownership, reclaim and hand-off

Applying runs in one `BEGIN IMMEDIATE` transaction. It goes table by table:
authorizations and projects, then accounts and their grants, then identities.

1. **Upsert by natural key.** A row changes, and gets a new modification, only
   if a field actually changed. A reload with nothing to do writes no audit
   records.
2. **The map file wins for keys it defines.** The map files *define*:
   - identities;
   - an account's grants;
   - authorizations listed in the authorizations file.

   If one of those keys exists as an `internal` row, the reconcile
   **reclaims** it. Ownership becomes `mapfile`, the file's values overwrite
   the row, the UUID is kept, a `Reclaimed` problem names the key, and the
   `reclaim` modification's `Changes` record what was overwritten. An
   identity's `DisplayName` and `Comment` are not expressed in the file, so
   they survive a reclaim.
3. **References do not reclaim.** A user line that merely names a project or
   authorization leaves an `internal` row of that name alone. A missing row is
   created as a mapfile placeholder.
4. **Delete** `mapfile` rows whose keys the file no longer mentions, unless
   they are quarantined.
5. **Hand-off.** A mapfile account, project or authorization that is no longer
   in the file but is still referenced by an `internal` row is not deleted.
   Ownership moves to `internal`.

A change that removes or narrows what a token was minted on revokes the
token, in the same transaction (§6.4): a deleted identity or account, an
identity moved to another account or given an earlier expiry (including by a
malformed line), and an account whose grants lose an authorization or project.

Any error rolls the whole apply back. Status is recorded only after the commit
succeeds.

### 4.5 Status

The outcome of the last attempt is published in the daemon ad and by
`PLACEMENT_QUERY` with `TargetType = "MapfileStatus"`:

| Attribute | Meaning |
|---|---|
| `PlacementdApiVersion` | `2`; clients use it to detect the commands in §5. |
| `PlacementdMapfileLoadTime` | Time of the last successful apply. |
| `PlacementdMapfileLoadAttemptTime` | Time of the last attempt. |
| `PlacementdMapfileOK` | Whether the last attempt applied with no problems, apart from reclaims. |
| `PlacementdMapfileQuarantined` | Number of quarantined identities and authorizations, plus rejected names. |
| `PlacementdMapfileConflicts` | Number of internal rows the files reclaimed. |

The query form adds `UserMapfile`, `AuthorizationsMapfile` and `Problems`, a
list of `[Kind; File; Line; Key; Message]` records (capped at 200). The problem kinds
are `Missing`, `Unreadable`, `Malformed`, `Rejected`, `Duplicate`,
`AccountConflict`, `GrantsNarrowed`, `NoAuthorizations`, `Reclaimed` and
`DatabaseError`.

A `DatabaseError` outcome is kept only in memory. After a restart, the last
status saved by a successful apply is reported. With `PLACEMENTD_MAPFILE`
empty, the status reports OK with no problems.

---

## 5. CEDAR commands

| Command | Number | Notes |
|---|---|---|
| `PLACEMENT_USER_LOGIN` | SCHED_VERS+155 | Mints a token (§6). Requires encryption. |
| `PLACEMENT_QUERY_USERS` | +156 | Legacy. Sends both `MappingExpration` (historical spelling) and `MappingExpiration`. Also lists identities that appear only in the token log with an unexpired token, with `Authorized = false`. `ApUserId` is the account the identity maps to now, and for an identity only in the token log, the account of its newest unexpired token. `MappedAccount` is the mapped account, absent for an identity that is not mapped. |
| `PLACEMENT_QUERY_TOKENS` | +157 | Legacy. Adds `IdentityId`, `AccountId` and `ProjectId` when known. |
| `PLACEMENT_QUERY_AUTHORIZATIONS` | +158 | Legacy. Returns the requestable authorizations, intersected with the user's grants when a user is named. |
| `PLACEMENT_QUERY` | +164 | Typed query over the entities, the map-file status and the audit log. |
| `PLACEMENT_EDIT` | +165 | Atomic batch of Create, Update, Delete, Adopt and Revoke. Requires encryption. |

All of these are registered at ADMINISTRATOR with forced authentication.
`PLACEMENT_USER_LOGIN` replies with a single ad carrying `Token`, or
`ErrorCode` and `ErrorString`. Every other command replies with result ads
followed by one `MyType = "Summary"` ad, all in one message. The Summary carries `ErrorCode`
and `ErrorString` on failure. The legacy commands' replies are unchanged from
before the database, apart from the additions noted above.

### 5.1 `PLACEMENT_QUERY`

The request is one ad:

- `TargetType` (required): `Identity`, `Account`, `Project`, `Authorization`,
  `MapfileStatus` or `Modification`.
- Optional selectors: `Id`, or the natural key (`Identity` for identities,
  `Name` otherwise). A missing `TargetType`, or `Name` on an identity query,
  is error 16, and an unknown type is error 19. A `Modification` query uses
  `EntityId` instead. Account names are canonicalized as in edits (§5.2).
- Optional `Requirements`, `Projection` and `Limit`. `Requirements` is a
  ClassAd expression evaluated against each result ad, never SQL. `Limit`
  applies after `Requirements`. Entity ads come back sorted by key.

Each entity ad has `MyType = "Placement<TargetType>"` and carries:

- `Id`, `Source`, `Version`, `ModificationId`;
- the derived `CreatedAt`, `UpdatedAt` and `ModifiedBy`;
- the type's fields;
- for display, denormalized values:
  - an identity's `Account`, `AccountId`, `Authorizations`, `Projects`,
    `Expired` and `LastTokenExpiration`;
  - an account's `Identities`;
  - a project's `Accounts`.

List values are ClassAd lists of strings.

Modification ads (`MyType = "PlacementModification"`) carry:

- `Id`, `ChangesetId`, `Time`, `Channel`, `Actor`, `OnBehalfOf`,
  `PeerAddress`, `Comment`;
- `Operation`, `EntityType`, `EntityId`;
- `Changes`, a nested ad whose attributes are `{old, new}` lists. A
  modification that revoked tokens (§6.4) also carries
  `RevokedTokens = {undefined, <count>}`.

Results come newest first.

### 5.2 `PLACEMENT_EDIT`

The request is one message: a header ad, then the operation ads.

**Header.** `MyType = "PlacementEdit"` (set by clients; the daemon doesn't
check it), `NumOperations` (1–1000, else error 16, checked before
encryption), `DryRun` (boolean-valued, else error 16), and optional
`OnBehalfOf` and `Comment`.

**Each operation:**

- `Operation`: Create, Update, Delete, Adopt or Revoke.
- `TargetType`: `All` is valid only with Adopt.
- A selector: `Id` or the key attribute.
- Optional `ExpectedVersion`.
- `Cascade`, for Delete only.
- The fields to set.

Rules:

- **Update is a patch.** Only attributes present in the op change. Setting an
  attribute to the literal `undefined` clears it or resets it:
  - `ExpiresAt = undefined` (or `0`) means the identity never expires, and a
    negative value is error 16;
  - on an authorization, `undefined` restores the default (label = name,
    color `normal`, empty description, requestable);
  - an identity's `Identity` or `Account`, or an entity's `Name`, cannot be
    cleared (error 16).

  Account names are canonicalized like the map file's: a bare name gets the
  account domain, and `*` is refused. An identity's `Account` may be a name
  or a UUID. List attributes may be ClassAd lists or comma-separated
  strings. List attributes
  replace the whole set. Replacing an account's `Projects` clears a default
  that is no longer in the list. An update that changes nothing writes nothing and
  does not change `Version`.
- **The key attribute in an Update** is the new value when `Id` is present,
  and the selector otherwise. A rename keeps the UUID and creates
  modifications on everything whose visible names change: an account rename
  records updates on its identities, and a project or authorization rename
  records updates on the accounts that hold it. Renaming an account changes
  the subject of future tokens and revokes the account's tokens (§6.4). The
  result carries a `Warning` saying so, as does a project rename other than a
  change of case (which revokes nothing: projects are accounting). Renaming an
  internal project or authorization whose old name a map file line still
  lists is allowed, but the next reconcile gives that line's account a
  placeholder under the old name; the `Warning` (combined with the one above)
  names those accounts, from the last parsed map file. Renaming an
  authorization to a permission level (READ or WRITE, in any case) is error
  16 unless only the case changes: grants refer to the row, so every account
  holding it would mint that level from its next login. Create the level and
  grant it explicitly instead.
- **No implicit creation.** Naming an unknown account, project or
  authorization is error 13. A `DefaultProject` that isn't in the account's
  projects is error 16. One batch may create an account and then an
  identity that uses it.
- **Ownership.** Create always makes `internal` rows. Update and Delete on a
  `mapfile` row are error 15. Adopt moves rows to `internal`: Adopt on an
  account also adopts its identities, and Adopt with `TargetType = All`
  adopts everything and returns `Count`. Adopt honors `ExpectedVersion` and
  does nothing to a row that is already `internal`. Adopting an account does
  not adopt the projects or authorizations it holds. A key that the map file
  still defines is reclaimed on the next reconcile, so adopting it sticks only
  once its line is removed. The result carries a `Warning` when that is the
  case, and so does an Update of an adopted row whose key the last parsed map
  file still defines, since the reclaim undoes the update.
- **Delete** of a row that other rows reference is error 17, unless `Cascade`
  is set. A cascade that would have to remove or change a row the map file
  owns fails with error 15 instead.
- **Revoke**, on an `Identity` or an `Account` only (any other type is error
  19), revokes the unexpired tokens minted for it (§6.4) without changing it.
  It honors `ExpectedVersion`, needs no Adopt for a `mapfile` row, and is
  recorded as an `update` whose `Changes` hold only `RevokedTokens`. A Revoke
  that finds no token writes nothing.
- **Atomicity.** The batch runs in one `BEGIN IMMEDIATE`. Any failure rolls
  back everything, including its audit records.

The reply has one `PlacementEditResult` ad per operation. Each carries:

- `Index`;
- `Id` and `Version` (new values) when the batch committed:
  - Delete results carry `Id` only;
  - a dry run's results carry `Id` without `Version`;
  - a failed batch's results carry neither, because the writes were rolled
    back and the integer modification ids behind `Version` will be reused;
  - operations after the failing one get only `Index`;
- `ErrorCode`/`ErrorString` on the operation that failed;
- `Warning`, when there is one;
- `RevokedTokens`, the number of tokens the operation revoked, when it revoked
  any, and always for a Revoke. A dry run reports what it would revoke.

Then comes the Summary.

### 5.3 Error codes

Codes 1–12 are the existing login codes. A login failure always carries a
non-empty `ErrorString`; when an HTCondor error lies underneath, its text is
included, but its code is never passed through (it may be 0, or collide with
1).

| Code | Meaning |
|---|---|
| 1 | Request not encrypted. |
| 2 | Missing `UserName`. |
| 3 | Identity not authorized, or its account may not be minted for. |
| 4 | Requested authorizations not granted, or the account has none, or a privileged level. |
| 6 | Identity mapping expired. |
| 7, 8, 9 | Requester unknown / lacks INSTRUCTOR / requester mapping expired. |
| 10 | Project not allowed for the account. |
| 11 | Database failure, including a failed signing-key lookup, token generation, or write of the token to either token table. |
| 12 | The schedd could not be located, queried or asked to create a record, or the user or project is disabled there. |
| 13 | Not found. |
| 14 | `ExpectedVersion` mismatch. |
| 15 | Row owned by another source. |
| 16 | Invalid argument, including a reserved account name, a privileged authorization, or renaming an authorization to a permission level. |
| 17 | Still referenced; retry with `Cascade`. |
| 18 | Duplicate natural key. |
| 19 | Unknown `TargetType` or `Operation`. |

---

## 6. Login and what may be minted

### 6.1 `PLACEMENT_USER_LOGIN`

The request carries `UserName` (the identity), optional `Authorizations`,
`Project` and `Requester`. In order, the daemon:

1. Requires an encrypted channel (error 1) and a `UserName` (error 2).
2. Looks up the identity, its account and the account's grants, and refuses
   an unknown identity (error 3).
3. If `Requester` differs from `UserName`, checks that the requester exists,
   holds INSTRUCTOR, and is unexpired (errors 7, 8, 9).
4. Re-checks the account name against §6.2 (error 3). This is defense in
   depth, since rows may predate a rule.
5. Checks the requested project, case-insensitively, against the account's
   projects (error 10). The token claim and the schedd check use the
   request's spelling.
6. Refuses an account with no authorizations (error 4).
7. Computes the bounding set: the requested authorizations, which must all be
   granted, or all grants if none were requested. It refuses a bounding set
   containing a privileged level (error 4).
8. Refuses an expired identity (error 6) and caps the lifetime
   (`PLACEMENTD_TOKEN_DURATION`, default 1 day) at the identity's expiry.
9. Checks the schedd's user and project records:
   - The account and project are quoted in the constraint.
   - Results are matched at least as broadly as the schedd matches names:
     - projects case-insensitively;
     - users by HTCondor's user-name comparison, with the domain matched by
       prefix and case ignored.
   - **Fail closed:** if any record that could be the account or the project
     is disabled, the login is refused (error 12).
   - A record is created only when no matching record was returned. Creating
     one goes through the schedd's enable path, which would re-enable a
     disabled record.
   - A schedd that cannot be located, queried or asked to create a record is
     error 12.
10. Mints a capability token. The authentication layer records it in
    `idtokens_minting`, and the daemon then records it in `placementd_tokens`.
    A failed signing-key lookup or token generation is error 11. If the
    `placementd_tokens` write fails, the daemon deletes the token's
    `idtokens_minting` row (and its extra claims), which revokes it, logs at
    `D_ERROR` if that delete also fails, and returns error 11 with no token.

The daemon releases all database locks before calling the schedd and before
the authentication layer writes the token (§8).

### 6.2 Limits on what may be minted

These are enforced when `PLACEMENT_EDIT` writes (error 16), when the map files
are read, and again at login. In the map files, a reserved account quarantines
the identity, and a privileged or invalid name on a user line is dropped as
`Rejected`.

- **Reserved accounts are refused**, compared case-insensitively:
  - `condor@*`, `root@*` and `condor_pool@*`;
  - names in the reserved internal domains `password`, `child`,
    `matchsession`, `unmapped`, `family` and `parent`;
  - every name in `QUEUE_SUPER_USERS` (default `root`) and `OCU_SUPER_USERS`,
    in every domain: the user name is compared and the domain ignored, and a
    `%group` entry reserves its members' names. The schedd qualifies a bare
    super user with its own `UID_DOMAIN`, which need not be the account
    domain (`PLACEMENTD_UID_DOMAIN`), so matching only in the account domain
    would let `superuser@<schedd UID_DOMAIN>` through;
  - the user the daemon runs as, when `PERSONAL_CONDOR_IS_SUPER_USER` is true
    (the default), except on Windows.

  These lists are re-read on reconfig. Changing them forces a full reconcile
  pass. An identity whose account becomes reserved keeps its rows, but its
  logins are refused.
- **Privileged levels are refused.** An authorization name that is an
  HTCondor permission level other than READ or WRITE (ADMINISTRATOR, DAEMON,
  CONFIG, NEGOTIATOR, ADVERTISE_*, …) is refused. Names that are not
  permission levels, such as INSTRUCTOR, are allowed.
- **Name characters.** A name must be valid UTF-8, at most 1024 bytes for an
  identity and 256 for anything else. No name may contain `"`, `\`, ASCII
  whitespace, or a control or format character (Unicode categories Cc and
  Cf: the bidi overrides and isolates, zero-width characters, and the like).
  Account, project and authorization names also may not contain `,`. An
  account needs exactly one `@`, with non-empty parts and a user other than
  `*`. Scope strings and schedd constraints are built from these names, so
  this check is what keeps them well-formed.

### 6.3 Capability tokens with empty scopes

The daemon never mints a token with an empty bounding set. The
authentication layer also guards against a capability token whose stored scope
yields no `condor:/` authorizations:

- **`SEC_ACCEPT_CAPABILITY_TOKENS` true:** it authorizes nothing beyond
  ALLOW-level commands (in `daemon_command.cpp`).
- **`SEC_ACCEPT_CAPABILITY_TOKENS` false:** it is refused at authentication
  (in `condor_auth_passwd.cpp`), rather than being treated as a plain token
  with no limit.

Before this, an empty list meant "no limit" on both paths.

### 6.4 Revocation

A token is valid until it expires (`PLACEMENTD_TOKEN_DURATION`, default 1
day) unless its row in the authentication layer's `idtokens_minting` table is
removed: the capability token is then refused wherever `TOKENS_DATABASE` is
consulted. The placementd removes the rows (and their
`idtokens_minting_extra_claims`) of every unexpired token whose basis an edit
or a reconcile removes or narrows, inside the transaction that makes the
change:

- deleting an identity, or an account (with its identities);
- renaming an identity, or moving it to another account;
- moving an identity's expiry earlier: only tokens that expire after the new
  expiry;
- renaming an account (its tokens carry the old name);
- removing an authorization or a project from an account, whether by an
  Update, a cascaded Delete of the authorization or project, or the map file;
- renaming an authorization other than in case (its holders' tokens carry the
  old name).

Renaming a project, or widening grants, revokes nothing. Revoke (§5.2) does it
on demand. The token log keeps its rows as the record of what was issued.
Revocation runs no network I/O, and finds tokens through the token log's
indexes.

---

## 7. Audit log

`placementd_changesets` holds one row per committed `PLACEMENT_EDIT` batch,
and one per reconcile that changed something. Its columns are:

- `id`, `at`;
- `channel`: `cedar` or `mapfile`;
- `actor`: the authenticated CEDAR peer, or `mapfile`;
- `on_behalf_of` and `comment`, from the edit header;
- `peer_address`.

`placementd_modifications` holds one row per entity touched in a change set:

- `changeset_id`, `entity_type`, `entity_uuid`;
- `operation`: create, update, delete, adopt, reclaim or handoff;
- `changes`, a JSON object `{"Field": [old, new]}`. Grants are recorded as
  lists on the account. A modification that revoked tokens adds
  `"RevokedTokens": [null, <count>]`.

`entity_uuid` deliberately has no foreign key, so history outlives the entity.

`OnBehalfOf` lets a front end that authenticates as itself name the person who
made a change. It is recorded and shown, never used for authorization. It and
`Comment` are stripped of invalid UTF-8 and of control and format characters
(Unicode Cc and Cf) and length-capped before they reach the database or a
`D_AUDIT` line. So is the entities' free text, from an edit or from the
authorizations file: an identity's `DisplayName` (256 bytes) and `Comment`
(4096), a project's `Description` (4096), and an authorization's `Label`
and `Color` (256 each) and `Description` (4096). A map-file problem's key and
message, and request strings the daemon logs, are cleaned the same way. Every applied operation also produces
a `D_AUDIT` line naming the peer.

A batch or reconcile that changes nothing writes no change set. A no-op
update and a dry run produce no `D_AUDIT` line. Each reconcile pass that is
not skipped logs one `D_AUDIT` summary line. `OnBehalfOf` is capped at 256
bytes and `Comment` at 4096, cut at a UTF-8 character boundary.

No pruning is done. Only real changes are written, so the log grows with
administrative activity, not with time. A future pruner must keep every
modification that some row still references, and each entity's first one.

---

## 8. Concurrency and the shared database

`TOKENS_DATABASE` is shared. Every daemon's authentication layer opens it when
it validates or mints database-backed tokens. The placementd is single-threaded
and is the only writer of its own tables. Its rules for not disturbing the
other users:

- **One connection.** `PRAGMA foreign_keys = ON` is set on it, with a 5 s
  busy timeout. The authentication layer's connections have a 2 s busy
  timeout of their own.
- **`journal_mode` is never changed.** WAL is persistent in the file and
  would change locking for every daemon.
- **Write transactions are short.** Map files and request ads are fully read
  before `BEGIN IMMEDIATE`, and no network I/O happens inside a transaction.
- **Locks are released before any send.** Prepared statements are cached. A
  cached statement that has been stepped but not reset holds a read lock. That
  lock blocks other daemons' token writes, and if it is still open at
  `BEGIN IMMEDIATE`, SQLite returns busy without waiting. Every handler
  therefore resets its statements before sending a reply, and before calling
  the schedd. Handlers that return many rows, such as the token log, read
  them all before sending. A write transaction resets the cache before
  `BEGIN IMMEDIATE`. A statement still part-way through its rows at that
  point is logged at `D_ERROR`, as a bug.

---

## 9. Compatibility

- **The map-file formats are unchanged.** Existing deployments import on
  first start.
- **The legacy commands' replies are unchanged**, apart from the additions in
  §5. Python's `htcondor2.Placementd` keeps its four methods and adds
  `query()` and `edit()`. An `HTCondorException` raised by `edit()` or a
  query carries the reply's result ads as `.results`.
- **Behavior changes, all deny-biased:**
  - a malformed line keeps the identity's previous state instead of dropping
    it, and a non-numeric expiry no longer crashes the daemon;
  - conflicting or duplicated lines are merged restrictively instead of "last
    line wins";
  - reserved accounts, privileged authorizations and accounts with no
    authorizations are refused;
  - schedd user/project checks fail closed.
- **Downgrade.** An older placementd ignores the version-2 tables. The extra
  token-log columns are harmless to its `INSERT`, which names its columns.

---

## 10. Testing

`src/condor_tests/test_placementd_db.py` drives a personal HTCondor with a
placementd through the Python bindings. It covers:

- the import round trip and stable UUIDs;
- the parsing, merge, ownership and status rules in §4;
- the edit rules and the audit log;
- the legacy commands and the login error matrix, including an unreachable
  schedd (error 12) and a failed token-log write, which revokes the token
  (error 11);
- the limits in §6.2, including the schedd fail-closed checks;
- the empty-capability rule in §6.3;
- revocation by each edit and reconcile trigger in §6.4, and by Revoke,
  checked by presenting the token to the schedd;
- migration and lock behavior with a concurrent writer.

It does not yet cover:
- a missing or unreadable user map file;
- the two-second rule for recently modified files;
- `%group` entries in the superuser lists.
