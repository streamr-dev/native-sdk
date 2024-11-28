#ifndef STREAMER_PROTORPC_PLUGIN_STREAM_PRINTER_HPP
#define STREAMER_PROTORPC_PLUGIN_STREAM_PRINTER_HPP

#include <google/protobuf/io/zero_copy_stream.h>

namespace streamr::protorpc {

// Logic copied from
// https://github.com/end2endzone/protobuf-pbop-plugin/blob/master/src/protobuf-pbop-plugin/StreamPrinter.cpp
// Changed size_t to int64_t
class StreamPrinter {
private:
    google::protobuf::io::ZeroCopyOutputStream* mStream;

public:
    explicit StreamPrinter(google::protobuf::io::ZeroCopyOutputStream* iStream)
        : mStream(iStream) {}

    void print(const unsigned char* iValue, int64_t iLength) {
        int64_t dumpIndex = 0;

        while (dumpIndex < iLength) {
            // get a buffer from the stream
            void* buffer = nullptr;
            int bufferSize = 0;
            mStream->Next(&buffer, &bufferSize);

            if (buffer && bufferSize > 0) {
                // compute dumpsize
                int64_t dumpSize = iLength - dumpIndex;
                if (dumpSize > bufferSize) {
                    dumpSize = bufferSize;
                }

                // dump
                const unsigned char* dumpData = &iValue[dumpIndex];
                memcpy(buffer, dumpData, dumpSize);

                if (dumpSize < bufferSize) {
                    int backupSize = bufferSize - dumpSize; // NOLINT
                    mStream->BackUp(backupSize);
                }

                dumpIndex += dumpSize;
            }
        }
    }
};

}; // namespace streamr::protorpc
#endif // PROTOBUF_STREAMR_PLUGIN_PLUGIN_CODE_GENERATOR_HPP