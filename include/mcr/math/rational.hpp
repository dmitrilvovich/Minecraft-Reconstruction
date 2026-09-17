#pragma once

#include <compare>
#include <cstdint>
#include <iosfwd>

namespace mcr {
// Exact within its explicit range. Every arithmetic overflow throws.
// INT64_MIN is excluded so signs can always be reversed safely.
class Rational {
public:
    Rational(std::int64_t numerator = 0, std::int64_t denominator = 1);
    [[nodiscard]] std::int64_t numerator() const { return n_; }
    [[nodiscard]] std::int64_t denominator() const { return d_; }
    [[nodiscard]] std::int64_t floor() const;
    friend bool operator==(const Rational&, const Rational&) = default;
    friend std::strong_ordering operator<=>(const Rational&, const Rational&);
    friend Rational operator+(const Rational&, const Rational&);
    friend Rational operator-(const Rational& a, const Rational& b) { return a + (-b); }
    friend Rational operator*(const Rational&, const Rational&);
    friend Rational operator/(const Rational&, const Rational&);
    Rational operator-() const { return {-n_, d_}; }
private:
    std::int64_t n_, d_;
};
std::ostream& operator<<(std::ostream&, const Rational&);
} // namespace mcr

