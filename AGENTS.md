# HTCondor GitHub Copilot Instructions

This file provides guidance to GitHub Copilot when assisting with HTCondor development. It follows best practices for modern C++ projects and Copilot instruction files.

## Quick Reference Card

| **Aspect** | **HTCondor Convention** |
|------------|------------------------|
| **Language** | C++20 standard |
| **Build System** | CMake (out-of-tree builds only) |
| **Logging** | `dprintf(D_LEVEL, fmt, ...)` not `printf()` |
| **File I/O** | `safe_open_wrapper()` / `safe_fopen_wrapper()` not `open()` / `fopen()` |
| **Errors** | `EXCEPT("message")` for fatal errors |
| **Member vars** | Prefix with `m_` (e.g., `m_count`) |
| **Indentation** | Tabs (size 4), not spaces |
| **Warnings** | Aim for zero warnings; some CI/build configs may enable `-Werror` |
| **Namespaces** | ClassAd uses `classad::` namespace |
| **Config** | Use `param()` family, never hardcode paths |

## Project Overview

HTCondor is a **distributed high-throughput computing system** written in C++20. The codebase consists of multiple daemons communicating via ClassAd messages over network sockets to schedule and execute jobs across distributed resources.

**Key Characteristics:**
- **Event-driven architecture** using DaemonCore framework
- **Mature codebase** with 30+ years of history (respect legacy patterns)
- **Security-critical** (runs with elevated privileges, handles user data)
- **Cross-platform** (Linux, Windows, macOS)
- **Large scale** (manages 100,000+ compute nodes in production)

## Architecture: Core Daemons

HTCondor uses a daemon-based architecture. The five essential daemons are:

1. **condor_master** (`src/condor_master.V6/`) - Root daemon, starts/monitors all others
2. **condor_schedd** (`src/condor_schedd.V6/`) - Job scheduler, manages job queue
3. **condor_startd** (`src/condor_startd.V6/`) - Worker node manager, executes jobs
4. **condor_collector** (`src/condor_collector.V6/`) - Central information repository
5. **condor_negotiator** (`src/condor_negotiator.V6/`) - Matchmaker (jobs ↔ resources)

Supporting daemons:
- **condor_shadow** (`src/condor_shadow.V6.1/`) - Schedd-side job proxy
- **condor_starter** (`src/condor_starter.V6.1/`) - Worker-side job executor

*Note: `.V6` suffixes are historical, ignore them.*

## Core Frameworks & Patterns

### DaemonCore Framework

All daemons are built on **DaemonCore** (`src/condor_daemon_core.V6/condor_daemon_core.h`), an event-driven framework providing:

- **Command registration**: `daemonCore->Register_Command(CMD_*, handler, ...)`
- **Timer registration**: `daemonCore->Register_Timer(interval, handler, ...)`
- **Signal handling**: `daemonCore->Register_Signal(SIGTERM, handler)`
- **Process spawning**: `daemonCore->Create_Process(...)`
- **Socket/stream management**: Inter-daemon communication primitives

**Golden Rule**: In daemon code, ALWAYS use DaemonCore for timers/signals/sockets. Never use raw POSIX calls (`timer_create()`, `signal()`, etc.) as they bypass the event loop.

### ClassAd System

**ClassAds** (`src/classad/`) are HTCondor's universal data structure—attribute-value pairs with expression evaluation. Everything is a ClassAd: jobs, machines, policies.

**Key Points:**
- **Namespace**: Lives in `classad::` namespace (`classad::ClassAd`)
- **Don't forward-declare**: Never `class ClassAd;` — include `condor_classad.h` instead
- **Case-insensitive**: Attribute names are case-insensitive
- **Use macros**: Reference attributes via `ATTR_*` constants from `condor_attributes.h`

### Logging with dprintf

Use `dprintf()` (from `condor_debug.h`) **instead of `printf()`** for all output:

**Debug Categories:** For daemons, enable fine-grained logging via `ALL_DEBUG`, `DEFAULT_DEBUG`, and `<SUBSYS>_DEBUG` config parameters; for command-line tools, use `TOOL_DEBUG`. Common categories: `D_ALWAYS`, `D_ERROR`, `D_FULLDEBUG`, `D_NETWORK`, `D_SECURITY`, `D_DAEMONCORE`, `D_COMMAND`, `D_MATCH`.

**Best Practices:**
- Use `D_ERROR` for errors (not `D_ALWAYS`)
- Use `D_FULLDEBUG` for verbose tracing (not `D_ALWAYS`)
- Use category-specific levels (`D_NETWORK`, `D_SECURITY`) when appropriate
- Always include newlines (`\n`)

### Error Handling with EXCEPT

Use `EXCEPT()` macro for **fatal errors** (invariant violations, impossible conditions):

**When to use:**
- Assertion-like checks that should never fail
- Unrecoverable errors (corrupted data structures, etc.)
- Programming errors (wrong API usage)

**When NOT to use:**
- Expected failures (file not found, network timeout)
- User errors (bad input, permission denied)
- Use regular error handling (return codes, exceptions) for these

### Network I/O Patterns

Communication uses **Stream** objects from `condor_io.h`:

**Stream Types:**
- **ReliSock** - Reliable TCP connections
- **SafeSock** - UDP datagrams
-- Use ReliSock and SafeSock where possible instead of raw sockets.

**Critical**: ALWAYS check return values—network operations are async and can fail.

## Build System & Development Workflow

HTCondor uses **CMake** with **out-of-tree builds only** (in-source builds are not supported). The codebase requires **C++20** standard.

### Quick Build Commands

```bash
# Standard development build
mkdir __build && cd __build
../configure_uw -GNinja -DWITH_VOMS:bool=false -DWITH_LIBVIRT:bool=false ..
ninja -j8 install
```

### Build Configuration

**Important CMake Options:**
- `-D_DEBUG:BOOL=TRUE` - Non-optimized build with debug symbols
- `-DCMAKE_INSTALL_PREFIX:PATH=/path` - Installation location
- `-GNinja` - Use Ninja build system (faster than Make)
- `-DWITH_VOMS:bool=false` - Disable VOMS support (reduces dependencies)
- `-DWITH_LIBVIRT:bool=false` - Disable libvirt support

### Warning-as-Error Policy

**CRITICAL**: HTCondor builds with **`-Werror`** enabled. All compiler warnings are fatal errors.

**Common build failures:**
- **Unused variables**: Remove or use `[[maybe_unused]]`
- **Unused parameters**: Use `[[maybe_unused]]` if needed for interface compatibility
- **After refactoring**: Always audit local variables to ensure none became unused

### Key Build Files

- `CMakeLists.txt` - Root build configuration
- `src/CMakeLists.txt` - Per-directory subdirectory declarations
- `src/condor_includes/` - Common headers (on global include path)
- `src/condor_utils/` - Shared utility libraries
- `build/` - Build scripts and CMake helpers

### Header Visibility Rules

**Global headers** (`src/condor_includes/`):
- On global include path, visible to all daemons
- Use for widely-shared interfaces

**Daemon-specific headers** (e.g., `src/condor_negotiator.V6/`):
- Only visible to that daemon's sources
- NOT visible to other daemons

**Rule**: If a header needs to be shared across multiple daemons, it MUST live in `src/condor_includes/` or `src/condor_utils/`.

## Coding Conventions & Style

### HTCondor Style Guidelines

Based on `src/CODING_GUIDELINES`:

1. **Member variables**: Prefix with `m_`
2. **Indentation**: Use **tabs (size 4)**, NOT spaces
3. **Capitalization**:
   - **Global variables**: Begin with capitals (`FooBarGlarch`)
   - **Local variables**: All lowercase with underscores (`foo_bar_glarch`)
   - **Macros**: All uppercase (`MAX_VALUE`, `ATTR_JOB_ID`)
4. **Function definitions**: Return type on separate line, function name at column 1:
5. **Braces**: Opening `{` on same line as control statement:
6. **Preprocessor**: Use `#if FOO` not `#ifdef FOO` (handles `FOO=0` correctly)
7. **Memory ownership**: Document in comments whether functions take ownership

### Modern C++ (C++20) Best Practices

HTCondor uses **C++20 standard**. When writing new code or refactoring, embrace modern C++:

**Type Deduction & Inference:**
**Smart Pointers (Ownership):**
**Ranges & Algorithms (C++20):**
**String Views (Non-owning):**
**Lambda Expressions:**
**Designated Initializers (C++20):**
**Constexpr & Consteval:**

### Safe File I/O (SECURITY-CRITICAL)

** Avoid using  `open()` or `fopen()` directly in daemon code if it is possible to use safe wrappers from `src/safefile/`.

**Function Selection Guide:**

| **Use Case** | **Function** | **Behavior** |
|--------------|--------------|--------------|
| Read existing config/log | `safe_open_no_create()` | Fails if missing |
| Create lock file | `safe_create_fail_if_exists()` | Fails if exists |
| Append to log | `safe_create_keep_if_exists()` | Opens or creates |
| Update checkpoint | `safe_create_replace_if_exists()` | Atomically replaces |
| **Default choice** | `safe_open_wrapper()` | Smart behavior |
| User-specified path | `safe_open_wrapper_follow()` | Follows symlinks |

## Testing

HTCondor uses **Ornithology** test framework (`src/condor_tests/`) - a modern pytest-based system.

## Important Subsystems

### Job Queue Management
- **Location**: `src/condor_schedd.V6/qmgmt.{h,cpp}`
- **Purpose**: Persistent job storage and queue operations
- **Key APIs**: `GetAttributeInt()`, `SetAttribute()`, `NewJob()`, `DestroyJob()`

### DAGMan (Workflow Manager)
- **Location**: `src/condor_dagman/`
- **Purpose**: Directed acyclic graph workflow manager
- **Key Files**: `dag.cpp`, `parse.cpp`, `dagman_submit.cpp`

### File Transfer
- **Location**: `src/condor_filetransfer_plugins/`
- **Purpose**: Moving input/output files between submit and execute nodes
- **Plugins**: HTTP, S3, multifile curl

### Grid Integration
- **Location**: `src/condor_gridmanager/`
- **Purpose**: Interfacing with external HPC schedulers (SLURM, PBS, LSF)

### Security & Authentication
- **Location**: `src/condor_utils/condor_secman.h`
- **Purpose**: Authentication (Kerberos, GSI, SSL, tokens) and authorization
- **Key concepts**: Security sessions, user mappings, token validation

### CCB (Connection Brokering)
- **Location**: `src/ccb/`
- **Purpose**: NAT traversal for daemons behind firewalls
- **How**: Central broker forwards connections between private networks

## Python Bindings

HTCondor provides Python bindings for scripting and automation.

### Location & Structure

- **Python code**: `bindings/python/`
- **C++ glue**: `src/python-bindings/`

### Available Modules

**Modern (Python C API):**
- `htcondor2` - Drop-in replacement for `htcondor`
- `classad2` - Drop-in replacement for `classad`
- `classad3` - Future API-breaking improvements

## Documentation

HTCondor manual lives in `docs/` (Sphinx RST format).

### Documentation Structure

- `docs/users-manual/` - User-facing documentation
- `docs/admin-manual/` - Administrator documentation
- `docs/apis/` - API documentation (Python, etc.)
- `docs/man-pages/` - Command-line tool manpages
-- New man-pages in docs/man-pages should follow the template in docs/man-pages/man-page.template

### Custom Roles (Semantic Markup)

See `docs/STYLE.md` for documentation conventions:

```rst
:macro:`CONDOR_HOST`              - Configuration macro
:subcom:`universe`                - Submit file command
:ad-attr:`JobStatus`              - ClassAd attribute
:tool:`condor_q`                  - HTCondor command-line tool
:classad-function:`ifThenElse`    - ClassAd function
```

## Directory Structure Reference

```
src/
├── condor_*                    # Daemon and tool implementations
│   ├── condor_master.V6/       # Master daemon
│   ├── condor_schedd.V6/       # Scheduler daemon
│   ├── condor_startd.V6/       # Execute daemon
│   ├── condor_negotiator.V6/   # Negotiator daemon
│   ├── condor_collector.V6/    # Collector daemon
│   ├── condor_shadow.V6.1/     # Shadow process
│   ├── condor_starter.V6.1/    # Starter process
│   ├── condor_dagman/          # DAGMan workflow manager
│   └── condor_tests/           # Test suite (Ornithology)
├── condor_utils/               # Shared utility libraries
├── condor_includes/            # Global headers (on include path)
├── condor_daemon_core.V6/      # DaemonCore framework
├── condor_scripts/             # Admin/utility scripts (Python/Perl)
│                               # Scripts here must use only stdlib!
├── classad/                    # ClassAd library
├── safefile/                   # Safe file I/O wrappers
└── python-bindings/            # Python bindings C++ glue

bindings/
└── python/                     # Python binding implementations

docs/                           # Sphinx documentation
├── users-manual/
├── admin-manual/
├── man-pages/
└── apis/

build/                          # Build scripts and CMake helpers
```

## Key Reference Files

**Architecture & Frameworks:**
- `src/condor_daemon_core.V6/condor_daemon_core.h` - DaemonCore API
- `src/condor_includes/condor_debug.h` - Logging (dprintf) API
- `src/condor_includes/condor_io.h` - Network I/O (Streams, Socks)
- `src/condor_includes/condor_commands.h` - Inter-daemon command codes
- `src/condor_includes/condor_attributes.h` - ClassAd attribute constants

**File I/O Security:**
- `src/safefile/safe_open.h` - Safe file descriptor operations
- `src/safefile/safe_fopen.h` - Safe FILE* operations

**Configuration:**
- `src/condor_includes/condor_config.h` - Configuration API

**Style & Process:**
- `docs/STYLE.md` - Documentation style guide
- `docs/man-pages/man-page.template` - template for manual pages
- `.github/copilot-instructions.md` - This file

## Continuous Learning

**Stay Updated:**
- HTCondor evolves continuously
- This file may become outdated
- When in doubt, consult source code and existing patterns
- Ask experienced developers for architecture guidance

**Key Resources:**
- HTCondor manual: https://htcondor.readthedocs.io
- Ornithology docs: https://ornithology.readthedocs.io
- HTCondor Week presentations: https://agenda.hep.wisc.edu/
- Source code: The ultimate truth!
