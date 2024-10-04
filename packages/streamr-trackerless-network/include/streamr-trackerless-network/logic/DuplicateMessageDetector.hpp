#ifndef STREAMR_TRACKERLESS_NETWORK_DUPLICATE_MESSAGE_DETECTOR_HPP
#define STREAMR_TRACKERLESS_NETWORK_DUPLICATE_MESSAGE_DETECTOR_HPP

#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "streamr-logger/SLogger.hpp"
namespace streamr::trackerlessnetwork {

using streamr::logger::SLogger;

/**
 * Represent a pair of numbers (a,b). Ordering between two pairs is defined as
 * follows. First compare first numbers. Compare second numbers if first are
 * equal.
 */
class NumberPair {
private:
    int64_t a;
    int64_t b;

public:
    NumberPair(int64_t a, int64_t b) : a(a), b(b) {} // NOLINT

    [[nodiscard]] bool greaterThanOrEqual(const NumberPair& otherPair) const {
        return this->greaterThan(otherPair) || this->equalTo(otherPair);
    }

    [[nodiscard]] bool greaterThan(const NumberPair& otherPair) const {
        return this->compareTo(otherPair) == 1;
    }

    [[nodiscard]] bool equalTo(const NumberPair& otherPair) const {
        return this->compareTo(otherPair) == 0;
    }

    [[nodiscard]] int compareTo(const NumberPair& otherPair) const {
        if (this->a > otherPair.a) {
            return 1;
        }
        if (this->a < otherPair.a) {
            return -1;
        }
        if (this->b > otherPair.b) {
            return 1;
        }
        if (this->b < otherPair.b) {
            return -1;
        }
        return 0;
    }

    [[nodiscard]] std::string toString() const {
        return std::to_string(this->a) + "|" + std::to_string(this->b);
    }
};

class InvalidNumberingError : public std::runtime_error {
public:
    InvalidNumberingError()
        : std::runtime_error("pre-condition: previousNumber < number") {}
};

class GapMisMatchError : public std::runtime_error {
public:
    GapMisMatchError(
        const std::string& state,
        const std::optional<NumberPair>& previousNumber,
        const NumberPair& number)
        : std::runtime_error(
              "pre-condition: gap overlap in given numbers: \n previousNumber = " +
              (previousNumber.has_value() ? previousNumber.value().toString()
                                          : "None") +
              " number = " + number.toString() + " state = " + state) {}
};

/**
 *
 * Keeps track of a stream's message numbers and reports already seen numbers
 * as duplicates.
 *
 * Leverages the fact that message are assigned numbers from a strictly
 * increasing integer sequence for lowered space complexity. For example,
 * if we know that all messages up to number N have been seen, we can only
 * store the number N to provide message identity check. This is because
 * anything less than N can be deemed a duplicate.
 *
 * Messages arriving out-of-order makes this a bit harder since gaps form.
 * Most of the code in this class is built to deal with this complexity.
 * Basically, we need to keep track of which intervals [N,M] could still
 * contain unseen messages. We should also remove intervals after we are sure
 * that they contain no unseen messages.
 *
 * In addition to the above, there needs to be a limit to the number of
 * intervals we store, as it could well be that some messages never
 * arrive. The strategy is to start removing the lowest numbered
 * intervals when storage limits are hit.
 *
 */

class DuplicateMessageDetector {
private:
    size_t maxGapCount;
    std::vector<std::pair<NumberPair, NumberPair>> gaps;

public:
    // NOLINTNEXTLINE
    explicit DuplicateMessageDetector(size_t maxGapCount = 10000) {
        this->maxGapCount = maxGapCount;
        this->gaps = {}; // ascending order of half-closed intervals (x,y]
                         // representing gaps that contain unseen message(s)
    }

    /**
     * returns true if number has not yet been seen (i.e. is not a duplicate)
     */
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    bool markAndCheck(
        std::optional<NumberPair> previousNumber, NumberPair number) {
        if (previousNumber.has_value() && previousNumber.value().greaterThanOrEqual(number)) {
            throw InvalidNumberingError();
        }

        if (this->gaps.empty()) {
            // NOLINTNEXTLINE(modernize-use-emplace)
            this->gaps.push_back(std::make_pair(
                number,
                NumberPair(
                    std::numeric_limits<int>::max(),
                    std::numeric_limits<int>::max())));
            return true;
        }

        // Handle special case where previousNumber is not provided. Only
        // minimal duplicate detection is provided (comparing against latest
        // known message number).
        if (!previousNumber.has_value()) {
            if (number.greaterThan(this->gaps.back().first)) {
                this->gaps.back().first = number;
                return true;
            }
            return false;
        }

        for (int i = static_cast<int>(this->gaps.size()) - 1; i >= 0; --i) {
            const auto& [lowerBound, upperBound] =
                this->gaps[i]; // invariant: upperBound > lowerBound

            // implies number > upperBound (would've been handled in
            // previous iteration if gap exists)
            if (previousNumber.value().greaterThanOrEqual(upperBound)) {
                return false;
            }
            if (previousNumber.value().greaterThanOrEqual(lowerBound)) {
                if (number.greaterThan(upperBound)) {
                    throw GapMisMatchError(
                        this->toString(), previousNumber, number);
                }
                if (previousNumber.value().equalTo(lowerBound)) {
                    if (number.equalTo(upperBound)) {
                        this->gaps.erase(this->gaps.begin() + i);
                    } else {
                        this->gaps[i] = std::make_pair(number, upperBound);
                    }
                } else if (number.equalTo(upperBound)) {
                    this->gaps[i] =
                        std::make_pair(lowerBound, previousNumber.value());
                } else {
                    this->gaps.erase(this->gaps.begin() + i);
                    this->gaps.insert(
                        this->gaps.begin() + i,
                        std::make_pair(lowerBound, previousNumber.value()));
                    this->gaps.insert(
                        this->gaps.begin() + i + 1,
                        std::make_pair(number, upperBound));
                }

                // invariants after:
                //   - gaps are in ascending order
                //   - the intersection between any two gaps is empty
                //   - there are no gaps that define the empty set
                //   - last gap is [n, Infinity]
                //   - anything not covered by a gap is considered seen

                this->dropLowestGapIfOverMaxGapCount();
                return true;
            }
            if (number.greaterThan(lowerBound)) {
                throw GapMisMatchError(
                    this->toString(), previousNumber, number);
            }
        }
        return false;
    }

private:
    void dropLowestGapIfOverMaxGapCount() {
        // invariant: this.gaps.length <= this.maxGapCount + 1
        if (this->gaps.size() > this->maxGapCount) {
            this->gaps.erase(this->gaps.begin());
        }
    }

    [[nodiscard]] std::string toString() const {
        std::stringstream ss;
        for (const auto& [lower, upper] : this->gaps) {
            ss << "(" << lower.toString() << ", " << upper.toString() << "]";
        }
        return ss.str();
    }
};

} // namespace streamr::trackerlessnetwork

#endif // STREAMR_TRACKERLESS_NETWORK_DUPLICATE_MESSAGE_DETECTOR_HPP
