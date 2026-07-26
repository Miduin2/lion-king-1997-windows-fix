# Security policy

## Supported version

Only the latest release is supported.

## Reporting

Open a GitHub issue for ordinary compatibility problems. For a security issue,
use GitHub's private vulnerability reporting feature if it is enabled for the
repository.

Never attach copyrighted game files. Do not upload `LIONW.EXE`, game assets,
music, archives or disc images. A SHA-256 hash, Windows version, hardware
description and sanitized logs are normally sufficient.

The installer is intentionally fail-closed: it modifies only the supported
original executable hash and verifies every distributed component before
writing anything.
