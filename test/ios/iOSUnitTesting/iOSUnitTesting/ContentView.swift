//
//  ContentView.swift
//  iOSUnitTesting
//
//  On-device libc++ smoke test for the Homebrew-libc++ XCFramework.
//  Calls the proxyclient C API (no C++ modules) so the Homebrew-libc++
//  library runs on the device: static initializers, folly, and — via the
//  invalid-address rejection — the internal exception / exception_ptr path
//  (the __cxa_init_primary_exception machinery that was the historical iOS
//  blocker). Prints a machine-greppable line for devicectl --console.
//

import SwiftUI
import Foundation

func runStreamrSmokeTest() -> (Bool, String) {
    var out = "STREAMR_SMOKE: begin (Homebrew-libc++ XCFramework, DT 26)\n"

    proxyClientInitLibrary()

    // Trivial exported call — confirms the library is linked and callable.
    if let r = testRpc() {
        out += "STREAMR_SMOKE: testRpc -> \(String(cString: r))\n"
    }

    // Invalid Ethereum address: the library validates it internally and
    // reports the failure by THROWING and CATCHING inside the C++ boundary,
    // marshalling the result into the Error struct. This is the exact libc++
    // exception path we need to see work on the device runtime.
    var res: UnsafePointer<ProxyResult>? = nil
    let handle = proxyClientNew(&res,
        "not-a-valid-ethereum-address",
        "0xa000000000000000000000000000000000000000#01")
    let numErrors = res?.pointee.numErrors ?? 0
    var code = ""
    if let r = res, r.pointee.numErrors > 0, let errs = r.pointee.errors, let c = errs.pointee.code {
        code = String(cString: c)
    }
    out += "STREAMR_SMOKE: proxyClientNew(invalid) -> handle=\(handle) numErrors=\(numErrors) code=\(code)\n"
    if let r = res { proxyClientResultDelete(r) }

    proxyClientCleanupLibrary()

    // PASS = the library loaded, ran to completion without crashing, and its
    // internal exception/error path executed (invalid address rejected).
    let pass = (numErrors > 0)
    out += pass ? "STREAMR_SMOKE_RESULT: PASS\n" : "STREAMR_SMOKE_RESULT: FAIL\n"
    return (pass, out)
}

struct ContentView: View {
    @State private var report = "Running…"
    @State private var passed = false
    @State private var done = false

    var body: some View {
        VStack(spacing: 16) {
            Image(systemName: done ? (passed ? "checkmark.seal.fill" : "xmark.seal.fill") : "hourglass")
                .imageScale(.large)
                .foregroundStyle(done ? (passed ? .green : .red) : .orange)
            Text(done ? (passed ? "SMOKE PASS" : "SMOKE FAIL") : "Running…")
                .font(.headline)
            ScrollView {
                Text(report)
                    .font(.system(.footnote, design: .monospaced))
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
        }
        .padding()
        .onAppear {
            let (ok, text) = runStreamrSmokeTest()
            print(text)          // captured by `devicectl ... --console`
            fflush(stdout)
            NSLog("%@", text)    // also to the unified log
            passed = ok
            report = text
            done = true
            // Leave the result on screen briefly, then exit so the
            // devicectl --console stream returns with an exit code.
            DispatchQueue.main.asyncAfter(deadline: .now() + 3.0) {
                exit(ok ? 0 : 1)
            }
        }
    }
}
