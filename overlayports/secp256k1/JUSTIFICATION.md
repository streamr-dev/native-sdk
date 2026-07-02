# Why this overlay exists

Baseline: vcpkg 2026.06.24 (secp256k1 0.7.1). Identical to the upstream port
except one added configure option:

- `-DSECP256K1_ENABLE_MODULE_RECOVERY=ON`: upstream secp256k1 disables the
  recovery module by default and the vcpkg port does not enable it;
  streamr-utils/SigningUtils.hpp needs <secp256k1_recovery.h> (ECDSA
  public-key recovery). Remove if the upstream port gains a "recovery"
  feature.
