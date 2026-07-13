// C++<->TS stream interop test (milestone C exit criterion): a mixed
// network of TypeScript and C++ full nodes shares one stream part, and
// messages published from either side reach all nodes. The TS side is
// the pinned @streamr/trackerless-network 103.8.0-rc.3 from npm running
// in a child node process (fullNode.js in this directory): it acts as
// the websocket entry point and stream-part entry point, prints its
// PeerDescriptor (protobuf binary as hex) once ready, echoes RECEIVED
// <text> for every message it sees, and broadcasts on a PUBLISH <text>
// stdin command. Two C++ NetworkNodes join through it; a C++ publish
// must reach the other C++ node AND the TS node, and a TS publish must
// reach both C++ nodes.
//
// The pinned package's dependencies require a node runtime >= 20.
// Discovery order: $STREAMR_TS_INTEROP_NODE, `node` on PATH, newest
// ~/.nvm version. Only a missing runtime SKIPs the test; every other
// failure (npm ci, driver crash, join failure) is a hard failure.
#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>
#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <gtest/gtest.h>

#include <coroutine> // IWYU pragma: keep

import streamr.utils.CoroutineHelper;
import streamr.trackerlessnetwork.ContentDeliveryManager;
import streamr.trackerlessnetwork.NetworkNode;
import streamr.trackerlessnetwork.NetworkStack;
import streamr.trackerlessnetwork.protos;
import streamr.dht.DhtNode;
import streamr.dht.Identifiers;
import streamr.dht.PortRange;
import streamr.dht.protos;
import streamr.utils.BinaryUtils;
import streamr.utils.StreamPartID;
import streamr.utils.waitForCondition;

using ::dht::PeerDescriptor;
using streamr::dht::DhtNodeOptions;
using streamr::dht::Identifiers;
using streamr::dht::types::PortRange;
using streamr::trackerlessnetwork::createNetworkNode;
using streamr::trackerlessnetwork::NetworkNode;
using streamr::trackerlessnetwork::NetworkOptions;
using streamr::utils::BinaryUtils;
using streamr::utils::blockingWait;
using streamr::utils::StreamPartID;
using streamr::utils::StreamPartIDUtils;
using streamr::utils::waitForCondition;

namespace {

constexpr uint16_t tsEntryPointPort = 17791;
constexpr uint16_t cppNodePort1 = 17792;
constexpr uint16_t cppNodePort2 = 17793;
constexpr int minNodeMajorVersion = 20;
constexpr std::chrono::seconds driverStartTimeout{90};
constexpr std::chrono::seconds joinTimeout{30};
constexpr std::chrono::seconds messageTimeout{30};

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

StreamMessage createTestMessage(
    const std::string& content, const StreamPartID& streamPartId) {
    StreamMessage msg;
    auto* messageId = msg.mutable_messageid();
    messageId->set_streamid(StreamPartIDUtils::getStreamID(streamPartId));
    messageId->set_streampartition(
        static_cast<int32_t>(
            StreamPartIDUtils::getStreamPartition(streamPartId).value_or(0)));
    messageId->set_sequencenumber(0);
    messageId->set_timestamp(666); // NOLINT
    messageId->set_publisherid(
        BinaryUtils::hexToBinaryString(
            "0x1234567890123456789012345678901234567890"));
    messageId->set_messagechainid("cpp-interop-chain");
    msg.set_signaturetype(SignatureType::ECDSA_SECP256K1_EVM);
    msg.set_signature(BinaryUtils::hexToBinaryString("0x1234"));
    auto* contentMessage = msg.mutable_contentmessage();
    contentMessage->set_encryptiontype(EncryptionType::NONE);
    contentMessage->set_contenttype(ContentType::JSON);
    contentMessage->set_content(content);
    return msg;
}

} // namespace

class TsInteropStreamTest : public ::testing::Test {
protected:
    StreamPartID streamPartId = StreamPartIDUtils::parse("interop-stream#0");
    PeerDescriptor tsPeerDescriptor;
    std::shared_ptr<NetworkNode> cppNode1;
    std::shared_ptr<NetworkNode> cppNode2;
    pid_t driverPid = -1;
    int driverStdout = -1;
    int driverStdin = -1;
    std::string lineBuffer;
    std::mutex receivedMutex;
    std::set<std::string> tsReceived; // texts the TS node reported

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

    // Null-guarded like the other end-to-end fixtures.
    void TearDown() override {
        if (this->cppNode1) {
            blockingWait(this->cppNode1->stop());
        }
        if (this->cppNode2) {
            blockingWait(this->cppNode2->stop());
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
                _exit(126);
            }
            const auto port = std::to_string(tsEntryPointPort);
            execl( // NOLINT
                nodeBinary.c_str(),
                nodeBinary.c_str(),
                "fullNode.js",
                port.c_str(),
                static_cast<char*>(nullptr));
            _exit(127);
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

    void sendDriverCommand(const std::string& line) {
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
                    std::scoped_lock lock(this->receivedMutex);
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
                std::array<char, 4096> buffer{};
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
                std::scoped_lock lock(this->receivedMutex);
                if (this->tsReceived.contains(text)) {
                    return true;
                }
            }
            // Drain whatever the driver has printed; the prefix never
            // matches, so this just pumps RECEIVED lines for up to 200ms.
            readLineWithPrefix(
                "\x01never", std::chrono::milliseconds(200)); // NOLINT
        }
        std::scoped_lock lock(this->receivedMutex);
        return this->tsReceived.contains(text);
    }
};

TEST_F(TsInteropStreamTest, MessagesFromEitherSideReachAllNodes) {
    // Two C++ full nodes join through the TS entry point.
    this->cppNode1 = createNetworkNode(
        NetworkOptions{
            .layer0 = DhtNodeOptions{
                .entryPoints = {this->tsPeerDescriptor},
                .websocketPortRange =
                    PortRange{.min = cppNodePort1, .max = cppNodePort1}}});
    this->cppNode2 = createNetworkNode(
        NetworkOptions{
            .layer0 = DhtNodeOptions{
                .entryPoints = {this->tsPeerDescriptor},
                .websocketPortRange =
                    PortRange{.min = cppNodePort2, .max = cppNodePort2}}});
    blockingWait(this->cppNode1->start());
    blockingWait(this->cppNode2->start());
    this->cppNode1->setStreamPartEntryPoints(
        this->streamPartId, {this->tsPeerDescriptor});
    this->cppNode2->setStreamPartEntryPoints(
        this->streamPartId, {this->tsPeerDescriptor});
    blockingWait(this->cppNode1->join(this->streamPartId));
    blockingWait(this->cppNode2->join(this->streamPartId));

    std::mutex cppReceivedMutex;
    std::set<std::string> received1;
    std::set<std::string> received2;
    this->cppNode1->addMessageListener([&](const StreamMessage& msg) {
        std::scoped_lock lock(cppReceivedMutex);
        received1.insert(msg.contentmessage().content());
    });
    this->cppNode2->addMessageListener([&](const StreamMessage& msg) {
        std::scoped_lock lock(cppReceivedMutex);
        received2.insert(msg.contentmessage().content());
    });

    blockingWait(waitForCondition(
        [this]() {
            return !this->cppNode1->getNeighbors(this->streamPartId).empty() &&
                !this->cppNode2->getNeighbors(this->streamPartId).empty();
        },
        joinTimeout));

    // C++ -> everyone.
    const std::string cppText = R"({ "from": "cpp" })";
    blockingWait(this->cppNode1->broadcast(
        createTestMessage(cppText, this->streamPartId)));
    EXPECT_TRUE(this->waitForTsReceived(cppText, messageTimeout))
        << "the TS node never reported the C++ message";
    blockingWait(waitForCondition(
        [&]() {
            std::scoped_lock lock(cppReceivedMutex);
            return received2.contains(cppText);
        },
        messageTimeout));

    // TS -> everyone.
    const std::string tsText = R"({ "from": "ts" })";
    sendDriverCommand("PUBLISH " + tsText);
    blockingWait(waitForCondition(
        [&]() {
            std::scoped_lock lock(cppReceivedMutex);
            return received1.contains(tsText) && received2.contains(tsText);
        },
        messageTimeout));
}
