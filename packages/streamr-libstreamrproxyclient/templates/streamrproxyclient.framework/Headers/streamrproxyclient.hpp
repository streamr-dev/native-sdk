#ifndef LIBSTREAMRPROXYCLIENT_HPP
#define LIBSTREAMRPROXYCLIENT_HPP

#if defined(__clang__)
#define SHARED_EXPORT __attribute__((visibility("default")))
#define SHARED_LOCAL __attribute__((visibility("hidden")))
#endif

extern "C" const char* SHARED_EXPORT testRpc();


#endif // LIBSTREAMRPROXYCLIENT_HPP