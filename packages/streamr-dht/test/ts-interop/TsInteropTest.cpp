// C++<->TS interop test (milestone B exit criterion): a C++ DhtNode joins a
// real layer-0 DHT whose entry point is the pinned TypeScript implementation
// (@streamr/dht 103.8.0-rc.3 from npm) over real websockets on 127.0.0.1.
// The TS side runs in a child node process driven by entryPoint.js in this
// directory, which prints the entry point's PeerDescriptor (protobuf binary
// as hex) once ready and then its DHT neighbor set every 500 ms; mutual
// visibility is asserted from both sides.
//
// The pinned package's dependencies require a node runtime >= 20. Discovery
// order: $STREAMR_TS_INTEROP_NODE, `node` on PATH, newest ~/.nvm version.
// Only a missing runtime SKIPs the test; every other failure (npm ci, driver
// crash, join failure) is a hard failure.
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
#include <memory>
#include <optional>
#include <string>
#include <gtest/gtest.h>
#include "packages/dht/protos/DhtRpc.pb.h"

#include <coroutine> // IWYU pragma: keep

import streamr.dht.DhtNode;
import streamr.dht.Identifiers;
import streamr.dht.protos;
import streamr.utils.BinaryUtils;
import streamr.utils.CoroutineHelper;

using ::dht::PeerDescriptor;
using streamr::dht::DhtNode;
using streamr::dht::DhtNodeOptions;
using streamr::dht::Identifiers;
using streamr::utils::BinaryUtils;
using streamr::utils::blockingWait;

namespace {

constexpr uint16_t tsEntryPointPort = 17771;
constexpr int minNodeMajorVersion = 20;
constexpr std::chrono::seconds driverStartTimeout{60};
constexpr std::chrono::seconds mutualVisibilityTimeout{30};

std::optional<std::string> runAndCaptureFirstLine(const std::string& command) {
    FILE* pipe = popen(command.c_str(), "r"); // NOLINT
    if (pipe == nullptr) {
        return std::nullopt;
    }
    std::array<char, 256> buffer{};
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
    // Local-development convenience: PATH may point at an old node while nvm
    // has suitable ones installed.
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

} // namespace

class TsInteropTest : public ::testing::Test {
protected:
    PeerDescriptor tsPeerDescriptor;
    std::shared_ptr<DhtNode> cppNode;
    pid_t driverPid = -1;
    int driverStdout = -1;
    std::string lineBuffer;

    void SetUp() override {
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
        const auto descriptorHex =
            readLineWithPrefix("DESCRIPTOR ", driverStartTimeout);
        ASSERT_TRUE(descriptorHex.has_value())
            << "TS driver did not print its descriptor; see "
            << harnessDir() / "driver-stderr.log";
        ASSERT_TRUE(this->tsPeerDescriptor.ParseFromString(
            BinaryUtils::hexToBinaryString(*descriptorHex)));
    }

    void TearDown() override {
        if (this->cppNode) {
            this->cppNode->stop();
        }
        stopDriver();
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
        ASSERT_EQ(pipe(stdoutPipe.data()), 0);
        const pid_t pid = fork();
        ASSERT_NE(pid, -1);
        if (pid == 0) {
            dup2(stdoutPipe[1], STDOUT_FILENO);
            close(stdoutPipe[0]);
            close(stdoutPipe[1]);
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
                _exit(126);
            }
            const auto port = std::to_string(tsEntryPointPort);
            execl( // NOLINT
                nodeBinary.c_str(),
                nodeBinary.c_str(),
                "entryPoint.js",
                port.c_str(),
                static_cast<char*>(nullptr));
            _exit(127);
        }
        close(stdoutPipe[1]);
        this->driverPid = pid;
        this->driverStdout = stdoutPipe[0];
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
    }

    // Reads driver stdout until a line starting with the given prefix
    // arrives; returns the rest of that line. Non-matching lines (e.g.
    // NEIGHBORS heartbeats while waiting for something else) are dropped.
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
                    deadline - now)
                    .count();
            const int ready = poll(
                &pfd,
                1,
                static_cast<int>(
                    std::min<long long>(remaining, 200))); // NOLINT
            if (ready <= 0) {
                continue;
            }
            std::array<char, 4096> chunk{};
            const ssize_t bytes =
                read(this->driverStdout, chunk.data(), chunk.size());
            if (bytes > 0) {
                this->lineBuffer.append(chunk.data(), bytes);
            } else if (bytes == 0) {
                return std::nullopt; // driver exited
            }
        }
    }

    // True once the TS driver reports the given node id in a NEIGHBORS line.
    bool tsSeesNeighbor(
        const std::string& nodeId, std::chrono::milliseconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            const auto csv =
                readLineWithPrefix("NEIGHBORS ", std::chrono::seconds(2));
            if (!csv) {
                continue;
            }
            size_t start = 0;
            while (start <= csv->size()) {
                auto end = csv->find(',', start);
                if (end == std::string::npos) {
                    end = csv->size();
                }
                if (csv->substr(start, end - start) == nodeId) {
                    return true;
                }
                start = end + 1;
            }
        }
        return false;
    }
};

TEST_F(TsInteropTest, CppNodeJoinsTsEntryPointDht) {
    this->cppNode = std::make_shared<DhtNode>(
        DhtNodeOptions{.entryPoints = {this->tsPeerDescriptor}});
    blockingWait(this->cppNode->start());
    blockingWait(this->cppNode->joinDht({this->tsPeerDescriptor}));

    // C++ -> TS visibility: the TS entry point is in the routing table and
    // there is a live connection to it.
    const auto tsNodeId =
        Identifiers::getNodeIdFromPeerDescriptor(this->tsPeerDescriptor);
    EXPECT_GE(this->cppNode->getNeighborCount(), 1);
    const auto contacts = this->cppNode->getClosestContacts(1);
    ASSERT_EQ(contacts.size(), 1);
    EXPECT_TRUE(
        Identifiers::areEqualPeerDescriptors(
            contacts[0], this->tsPeerDescriptor));
    EXPECT_TRUE(this->cppNode->getConnectionsView()->hasConnection(tsNodeId));

    // TS -> C++ visibility: the driver reports the C++ node id among its
    // DHT neighbors.
    const auto cppNodeId = Identifiers::getNodeIdFromPeerDescriptor(
        this->cppNode->getLocalPeerDescriptor());
    EXPECT_TRUE(tsSeesNeighbor(cppNodeId, mutualVisibilityTimeout))
        << "TS entry point never listed the C++ node " << cppNodeId
        << " as a neighbor";
}
