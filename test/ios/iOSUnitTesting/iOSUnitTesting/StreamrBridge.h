// Bridges the streamr proxyclient C API into the smoke-test app's Swift code.
// This is the package's permanent public C header — pure C, no C++ modules —
// so AppleClang compiles it without any BMI/modules machinery. Calling into it
// exercises the Homebrew-libc++-built XCFramework (folly, exceptions,
// exception_ptr) on the real device.
#include "streamrproxyclient.h"
