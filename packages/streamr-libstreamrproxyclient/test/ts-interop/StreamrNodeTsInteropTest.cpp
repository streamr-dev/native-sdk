// End-to-end test of the streamrNode* C API against a TypeScript
// streamr network (milestone D exit criterion for the shared-library
// surface): the TS side is the pinned @streamr/trackerless-network
// 103.8.0-rc.3 from npm running in a child node process
// (entryPointNode.js in this directory). It acts as the websocket entry
// point and stream-part entry point with a node id derived from a known
// ethereum address, echoes RECEIVED <text> for every message it sees,
// and broadcasts on a PUBLISH <text> stdin command. A client-only C API
// node (no websocket server — the mobile configuration) joins through
// it; a C publish must reach the TS node and a TS publish must reach
// the C callback.
//
// The pinned package's dependencies require a node runtime >= 20.
// Discovery order: $STREAMR_TS_INTEROP_NODE, `node` on PATH, newest
// ~/.nvm version. Only a missing runtime SKIPs the test; every other
// failure (npm ci, driver crash, join failure) is a hard failure.
//
// Unlike the other interop tests this one deliberately uses ONLY the
// public C API (streamrnode.h) — no C++ modules.
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>
#include <array>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <gtest/gtest.h>
#include "streamrnode.h"

namespace {

constexpr uint16_t tsEntryPointPort = 17801;
constexpr const char* tsEthereumAddress =
    "0x2222222222222222222222222222222222222222";
constexpr const char* cppEthereumAddress =
    "0x1111111111111111111111111111111111111111";
constexpr const char* interopStreamPartId = "interop-stream#0";
constexpr int minNodeMajorVersion = 20;
constexpr size_t driverStdoutChunkSize = 4096;
constexpr size_t versionLineBufferSize = 256;
// Conventional shell exit codes: command found but not executable / not
// found; used by the forked child when exec fails.
constexpr int chdirFailedExitCode = 126;
constexpr int execFailedExitCode = 127;
constexpr std::chrono::seconds driverStartTimeout{90};
constexpr std::chrono::seconds joinTimeout{60};
constexpr std::chrono::seconds messageTimeout{30};

std::optional<std::string> runAndCaptureFirstLine(const std::string& command) {
    FILE* pipe = popen(command.c_str(), "r"); // NOLINT
    if (pipe == nullptr) {
        return std::nullopt;
    }
    std::array<char, versionLineBufferSize> buffer{};
    std::string output;
    if (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        output = buffer.data();
    }
    const int status = pclose(pipe);
    if (status != 0 || output.empty()) {
        return std::nullopt;
    }
    while (!output.empty() &&
           (output.back() == '\n' || output.back() == '\r')) {
        output.pop_back();
    }
    return output;
}

// Parses the major version out of `<node> --version` output ("v22.17.1").
std::optional<int> nodeMajorVersion(const std::string& nodeBinary) {
    const auto version =
        runAndCaptureFirstLine("'" + nodeBinary + "' --version 2>/dev/null");
    if (!version || version->size() < 2 || (*version)[0] != 'v') {
        return std::nullopt;
    }
    return std::atoi(version->c_str() + 1); // NOLINT
}

std::optional<std::string> findNodeBinary() {
    if (const char* fromEnv = std::getenv("STREAMR_TS_INTEROP_NODE")) {
        return std::string{fromEnv};
    }
    if (const auto onPath = runAndCaptureFirstLine("command -v node")) {
        const auto major = nodeMajorVersion(*onPath);
        if (major && *major >= minNodeMajorVersion) {
            return onPath;
        }
    }
    // Local-development convenience: PATH may point at an old node while
    // nvm has suitable ones installed.
    std::optional<std::string> best;
    int bestMajor = 0;
    if (const char* home = std::getenv("HOME")) {
        const auto nvmDir =
            std::filesystem::path(home) / ".nvm" / "versions" / "node";
        std::error_code ec;
        for (const auto& entry :
             std::filesystem::directory_iterator(nvmDir, ec)) {
            const auto candidate = entry.path() / "bin" / "node";
            if (!std::filesystem::exists(candidate)) {
                continue;
            }
            const auto major = nodeMajorVersion(candidate.string());
            if (major && *major >= minNodeMajorVersion && *major > bestMajor) {
                bestMajor = *major;
                best = candidate.string();
            }
        }
    }
    return best;
}

// Collects the contents delivered to the C API subscription callback
// (invoked on an internal network thread).
struct ReceivedMessages {
    std::mutex mutex;
    std::set<std::string> contents;

    static void callback(
        uint64_t /* nodeHandle */,
        const char* /* streamPartId */,
        const char* content,
        uint64_t contentLength,
        void* userData) {
        auto* self = static_cast<ReceivedMessages*>(userData);
        const std::scoped_lock lock(self->mutex);
        self->contents.emplace(content, contentLength);
    }

    bool contains(const std::string& expected) {
        const std::scoped_lock lock(this->mutex);
        return this->contents.contains(expected);
    }
};

} // namespace

class StreamrNodeTsInteropTest : public ::testing::Test {
protected:
    pid_t driverPid = -1;
    int driverStdout = -1;
    int driverStdin = -1;
    std::string lineBuffer;
    std::mutex receivedMutex;
    std::set<std::string> tsReceived; // texts the TS node reported

    void SetUp() override {
        proxyClientInitLibrary();
        const auto nodeBinary = findNodeBinary();
        if (!nodeBinary) {
            GTEST_SKIP() << "No node runtime >= " << minNodeMajorVersion
                         << " found (set STREAMR_TS_INTEROP_NODE to one)";
        }
        installHarnessDependencies(*nodeBinary);
        if (::testing::Test::HasFatalFailure()) {
            return;
        }
        spawnDriver(*nodeBinary);
        ASSERT_NE(this->driverPid, -1);
        const auto ready = readLineWithPrefix("READY", driverStartTimeout);
        ASSERT_TRUE(ready.has_value()) << "TS driver did not report READY; see "
                                       << harnessDir() / "driver-stderr.log";
    }

    void TearDown() override {
        stopDriver();
        proxyClientCleanupLibrary();
    }

    static std::filesystem::path harnessDir() {
        return std::filesystem::path{TS_INTEROP_HARNESS_DIR};
    }

    // npm ci into the harness directory when node_modules is absent. npm
    // lives next to the chosen node binary, which is not necessarily the
    // PATH one.
    static void installHarnessDependencies(const std::string& nodeBinary) {
        if (std::filesystem::exists(harnessDir() / "node_modules")) {
            return;
        }
        const auto nodeBinDir =
            std::filesystem::path(nodeBinary).parent_path().string();
        std::string command = "cd '" + harnessDir().string() + "' && ";
        if (!nodeBinDir.empty()) {
            command += "PATH='" + nodeBinDir + "':\"$PATH\" ";
        }
        command += "npm ci --no-audit --no-fund > npm-install.log 2>&1";
        ASSERT_EQ(std::system(command.c_str()), 0) // NOLINT
            << "npm ci failed; see " << harnessDir() / "npm-install.log";
    }

    void spawnDriver(const std::string& nodeBinary) {
        std::array<int, 2> stdoutPipe{};
        std::array<int, 2> stdinPipe{};
        ASSERT_EQ(pipe(stdoutPipe.data()), 0);
        ASSERT_EQ(pipe(stdinPipe.data()), 0);
        const pid_t pid = fork();
        ASSERT_NE(pid, -1);
        if (pid == 0) {
            dup2(stdoutPipe[1], STDOUT_FILENO);
            dup2(stdinPipe[0], STDIN_FILENO);
            close(stdoutPipe[0]);
            close(stdoutPipe[1]);
            close(stdinPipe[0]);
            close(stdinPipe[1]);
            const auto stderrPath =
                (harnessDir() / "driver-stderr.log").string();
            const int stderrFd = open(
                stderrPath.c_str(),
                O_WRONLY | O_CREAT | O_TRUNC,
                0644); // NOLINT
            if (stderrFd != -1) {
                dup2(stderrFd, STDERR_FILENO);
                close(stderrFd);
            }
            if (chdir(harnessDir().c_str()) != 0) {
                _exit(chdirFailedExitCode);
            }
            const auto port = std::to_string(tsEntryPointPort);
            execl( // NOLINT
                nodeBinary.c_str(),
                nodeBinary.c_str(),
                "entryPointNode.js",
                port.c_str(),
                tsEthereumAddress,
                static_cast<char*>(nullptr));
            _exit(execFailedExitCode);
        }
        close(stdoutPipe[1]);
        close(stdinPipe[0]);
        this->driverPid = pid;
        this->driverStdout = stdoutPipe[0];
        this->driverStdin = stdinPipe[1];
        fcntl(this->driverStdout, F_SETFL, O_NONBLOCK); // NOLINT
    }

    void stopDriver() {
        if (this->driverPid != -1) {
            kill(this->driverPid, SIGTERM);
            const auto deadline = std::chrono::steady_clock::now() +
                std::chrono::seconds(10); // NOLINT
            int status = 0;
            while (waitpid(this->driverPid, &status, WNOHANG) == 0) {
                if (std::chrono::steady_clock::now() > deadline) {
                    kill(this->driverPid, SIGKILL);
                    waitpid(this->driverPid, &status, 0);
                    break;
                }
                usleep(50000); // NOLINT
            }
            this->driverPid = -1;
        }
        if (this->driverStdout != -1) {
            close(this->driverStdout);
            this->driverStdout = -1;
        }
        if (this->driverStdin != -1) {
            close(this->driverStdin);
            this->driverStdin = -1;
        }
    }

    void sendDriverCommand(const std::string& line) const {
        const auto data = line + "\n";
        ASSERT_EQ(
            write(this->driverStdin, data.data(), data.size()),
            static_cast<ssize_t>(data.size()));
    }

    // Reads driver stdout until a line starting with the given prefix
    // arrives; returns the rest of that line. RECEIVED lines are
    // collected into tsReceived as a side effect so no report is lost
    // while waiting for something else; other lines are dropped.
    std::optional<std::string> readLineWithPrefix(
        const std::string& prefix, std::chrono::milliseconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (true) {
            const auto newline = this->lineBuffer.find('\n');
            if (newline != std::string::npos) {
                std::string line = this->lineBuffer.substr(0, newline);
                this->lineBuffer.erase(0, newline + 1);
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                if (line.starts_with("RECEIVED ")) {
                    const std::scoped_lock lock(this->receivedMutex);
                    this->tsReceived.insert(
                        line.substr(std::string{"RECEIVED "}.size()));
                }
                if (line.starts_with(prefix)) {
                    return line.substr(prefix.size());
                }
                continue;
            }
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline) {
                return std::nullopt;
            }
            struct pollfd pfd{
                .fd = this->driverStdout, .events = POLLIN, .revents = 0};
            const auto remaining =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    deadline - now);
            const int pollTimeout = static_cast<int>(
                std::min<long long>(remaining.count(), 200)); // NOLINT
            if (poll(&pfd, 1, pollTimeout) > 0 &&
                ((pfd.revents & POLLIN) != 0)) {
                std::array<char, driverStdoutChunkSize> buffer{};
                const auto n =
                    read(this->driverStdout, buffer.data(), buffer.size());
                if (n > 0) {
                    this->lineBuffer.append(buffer.data(), n);
                }
            }
        }
    }

    // Polls driver stdout until the TS node has reported receiving the
    // given text (or the timeout passes).
    bool waitForTsReceived(
        const std::string& text, std::chrono::milliseconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            {
                const std::scoped_lock lock(this->receivedMutex);
                if (this->tsReceived.contains(text)) {
                    return true;
                }
            }
            // Drain whatever the driver has printed; the prefix never
            // matches, so this just pumps RECEIVED lines for up to 200ms.
            readLineWithPrefix(
                "\x01never", std::chrono::milliseconds(200)); // NOLINT
        }
        const std::scoped_lock lock(this->receivedMutex);
        return this->tsReceived.contains(text);
    }
};

TEST_F(StreamrNodeTsInteropTest, MessagesFlowBothWaysWithTsNetwork) {
    const std::string entryPointUrl =
        "ws://127.0.0.1:" + std::to_string(tsEntryPointPort);
    StreamrEntryPoint entryPoint{
        .websocketUrl = entryPointUrl.c_str(),
        .ethereumAddress = tsEthereumAddress};

    // A client-only node: no websocket server, exactly the mobile
    // configuration milestone D targets.
    const ProxyResult* result = nullptr;
    StreamrNodeConfig config{.entryPoints = &entryPoint, .numEntryPoints = 1};
    const uint64_t nodeHandle =
        streamrNodeNew(&result, cppEthereumAddress, &config);
    ASSERT_NE(nodeHandle, 0);
    proxyClientResultDelete(result);

    streamrNodeStart(&result, nodeHandle);
    ASSERT_EQ(result->numErrors, 0)
        << "start failed: " << result->errors[0].message;
    proxyClientResultDelete(result);

    streamrNodeJoinStreamPart(
        &result, nodeHandle, interopStreamPartId, &entryPoint, 1);
    ASSERT_EQ(result->numErrors, 0)
        << "join failed: " << result->errors[0].message;
    proxyClientResultDelete(result);

    ReceivedMessages received;
    const uint64_t subscriptionHandle = streamrNodeSubscribe(
        &result,
        nodeHandle,
        interopStreamPartId,
        ReceivedMessages::callback,
        &received);
    ASSERT_NE(subscriptionHandle, 0);
    proxyClientResultDelete(result);

    // Wait until the C node and the TS node are stream-part neighbors.
    const auto joinDeadline = std::chrono::steady_clock::now() + joinTimeout;
    uint64_t neighborCount = 0;
    while (std::chrono::steady_clock::now() < joinDeadline) {
        const ProxyResult* pollResult = nullptr;
        neighborCount = streamrNodeGetNeighborCount(
            &pollResult, nodeHandle, interopStreamPartId);
        proxyClientResultDelete(pollResult);
        if (neighborCount >= 1) {
            break;
        }
        usleep(200000); // NOLINT
    }
    ASSERT_GE(neighborCount, 1)
        << "the C node never got the TS node as a stream-part neighbor";

    // C -> TS.
    const std::string cppText = R"({ "from": "cpp-c-api" })";
    streamrNodePublish(
        &result,
        nodeHandle,
        interopStreamPartId,
        cppText.data(),
        cppText.size(),
        nullptr);
    ASSERT_EQ(result->numErrors, 0)
        << "publish failed: " << result->errors[0].message;
    proxyClientResultDelete(result);
    EXPECT_TRUE(this->waitForTsReceived(cppText, messageTimeout))
        << "the TS node never reported the C API message";

    // TS -> C.
    const std::string tsText = R"({ "from": "ts" })";
    sendDriverCommand("PUBLISH " + tsText);
    const auto messageDeadline =
        std::chrono::steady_clock::now() + messageTimeout;
    while (!received.contains(tsText) &&
           std::chrono::steady_clock::now() < messageDeadline) {
        usleep(200000); // NOLINT
    }
    EXPECT_TRUE(received.contains(tsText))
        << "the C API callback never got the TS message";

    streamrNodeUnsubscribe(&result, nodeHandle, subscriptionHandle);
    proxyClientResultDelete(result);
    streamrNodeStop(&result, nodeHandle);
    EXPECT_EQ(result->numErrors, 0);
    proxyClientResultDelete(result);
    streamrNodeDelete(&result, nodeHandle);
    proxyClientResultDelete(result);
}
