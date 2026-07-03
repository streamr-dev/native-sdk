# Why this overlay exists

Baseline: vcpkg 2026.06.24 (usrsctp 0.9.5.0). Identical to the upstream port
except two added patches carried over from the pre-2026 overlay (verified
still required — the iOS SDK ships neither header):

- `fix-missing-route-header-ios.patch`: iPhoneOS SDK has no <net/route.h>;
  provides the few routing-socket constants user_recv_thread.c needs.
  ("fatal error: 'net/route.h' file not found" on arm64-ios.)
- `add-iffaddrs-include.patch`: sctp_bsd_addr.c uses getifaddrs without
  including <ifaddrs.h>, which other platforms receive transitively.

Remove if upstream usrsctp gains iOS support.
