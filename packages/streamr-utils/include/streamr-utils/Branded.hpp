#ifndef STREAMR_UTILS_BRANDED_HPP
#define STREAMR_UTILS_BRANDED_HPP

#include <type_traits>
#include <utility>

namespace streamr::utils {

template <char... Chars>
using BrandString = std::integer_sequence<char, Chars...>;

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-string-literal-operator-template"
#endif

template <typename T, T... Chars>
constexpr BrandString<Chars...> operator""_brand() {
    return {};
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

template <typename T>
concept IntegralType = std::is_integral_v<T>;

template <typename T>
concept NonIntegralType = !std::is_integral_v<T>;

template <typename T, typename Brand>
class Branded;

// Specialization for non-integral types that can be inherited from

template <NonIntegralType T, typename Brand>
class Branded<T, Brand> : public T {
public:
    explicit Branded(T&& val) : T(std::move(val)) {}
    explicit Branded(const T& val) : T(val) {}
};

// Specialization for integral types that cannot be inherited from

template <IntegralType T, typename Brand>
class Branded<T, Brand> {
private:
    T value;

public:
    explicit Branded(T&& val) : value(std::move(val)) {}
    explicit Branded(const T& val) : value(val) {}

    Branded(Branded&& other) noexcept : value(std::move(other.value)) {}
    Branded(const Branded&) = default;

    Branded& operator=(Branded&& other) noexcept {
        if (this != &other) {
            value = std::move(other.value);
        }
        return *this;
    }

    Branded& operator=(const Branded&) = default;

    operator const T&() const { // NOLINT
        return value;
    }

    // Equality operators
    bool operator==(const Branded& other) const { return value == other.value; }

    bool operator!=(const Branded& other) const { return !(*this == other); }

    // Comparison operators
    bool operator<(const Branded& other) const { return value < other.value; }

    bool operator>(const Branded& other) const { return other < *this; }

    bool operator<=(const Branded& other) const { return !(other < *this); }

    bool operator>=(const Branded& other) const { return !(*this < other); }

    // Addition operators
    Branded operator+(const Branded& other) const {
        return Branded(value + other.value);
    }

    Branded& operator+=(const Branded& other) {
        value += other.value;
        return *this;
    }

    // Subtraction operators
    Branded operator-(const Branded& other) const {
        return Branded(value - other.value);
    }

    Branded& operator-=(const Branded& other) {
        value -= other.value;
        return *this;
    }

    // Multiplication operators
    Branded operator*(const Branded& other) const {
        return Branded(value * other.value);
    }

    Branded& operator*=(const Branded& other) {
        value *= other.value;
        return *this;
    }

    // Division operators
    Branded operator/(const Branded& other) const {
        return Branded(value / other.value);
    }

    Branded& operator/=(const Branded& other) {
        value /= other.value;
        return *this;
    }

    // Modulo operators
    Branded operator%(const Branded& other) const {
        return Branded(value % other.value);
    }

    Branded& operator%=(const Branded& other) {
        value %= other.value;
        return *this;
    }

    // Increment and decrement operators
    Branded& operator++() {
        ++value;
        return *this;
    }

    Branded operator++(int) {
        Branded temp(*this);
        ++(*this);
        return temp;
    }

    Branded& operator--() {
        --value;
        return *this;
    }

    Branded operator--(int) {
        Branded temp(*this);
        --(*this);
        return temp;
    }

    // Arithmetic operators with T
    Branded operator+(const T& other) const { return Branded(value + other); }

    Branded& operator+=(const T& other) {
        value += other;
        return *this;
    }

    Branded operator-(const T& other) const { return Branded(value - other); }

    Branded& operator-=(const T& other) {
        value -= other;
        return *this;
    }

    Branded operator*(const T& other) const { return Branded(value * other); }

    Branded& operator*=(const T& other) {
        value *= other;
        return *this;
    }

    Branded operator/(const T& other) const { return Branded(value / other); }

    Branded& operator/=(const T& other) {
        value /= other;
        return *this;
    }

    Branded operator%(const T& other) const { return Branded(value % other); }

    Branded& operator%=(const T& other) {
        value %= other;
        return *this;
    }

    // Comparison operators with T
    bool operator==(const T& other) const { return value == other; }

    bool operator!=(const T& other) const { return value != other; }

    bool operator<(const T& other) const { return value < other; }

    bool operator<=(const T& other) const { return value <= other; }

    bool operator>(const T& other) const { return value > other; }

    bool operator>=(const T& other) const { return value >= other; }
};

} // namespace streamr::utils

#endif // STREAMR_UTILS_BRANDED_HPP
