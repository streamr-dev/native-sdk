# Overlay ports

Empty since the 2026.06 vcpkg baseline bump: the previous overlays (folly,
usrsctp, libdatachannel, magic-enum, fmt) existed to patch ports at the old
2024 baseline and were all obsoleted by upstream.

Policy: add a port here only with a written justification in the port
directory (what fails without it, and the upstream issue/PR if one exists).
Prefer a `"builtin-baseline"`/`"overrides"` pin in `vcpkg.json` for version
problems; overlays are for patches that upstream does not carry.
