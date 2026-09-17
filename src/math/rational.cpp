#include "mcr/math/rational.hpp"

#include <limits>
#include <numeric>
#include <ostream>
#include <stdexcept>

namespace mcr {
namespace {
constexpr auto limit = std::numeric_limits<std::int64_t>::max();
std::int64_t magnitude(std::int64_t n) { return n < 0 ? -n : n; }
std::int64_t multiply(std::int64_t a, std::int64_t b) {
    if (b != 0 && magnitude(a) > limit / magnitude(b))
        throw std::overflow_error("rational multiplication exceeds int64 range");
    return a * b;
}
std::int64_t add(std::int64_t a, std::int64_t b) {
    if ((b > 0 && a > limit - b) || (b < 0 && a < -limit - b))
        throw std::overflow_error("rational addition exceeds int64 range");
    return a + b;
}
// Continued-fraction comparison: no potentially overflowing cross-products.
int compare_positive(std::int64_t an, std::int64_t ad,
                     std::int64_t bn, std::int64_t bd) {
    int direction = 1;
    for (;;) {
        const auto aq = an / ad, bq = bn / bd;
        if (aq != bq) return direction * (aq < bq ? -1 : 1);
        const auto ar = an % ad, br = bn % bd;
        if (ar == 0 || br == 0)
            return direction * (ar == br ? 0 : (ar == 0 ? -1 : 1));
        an = ad; ad = ar; bn = bd; bd = br;
        direction = -direction;
    }
}
}

Rational::Rational(std::int64_t n, std::int64_t d) : n_(n), d_(d) {
    if (d == 0) throw std::invalid_argument("zero rational denominator");
    if (n == std::numeric_limits<std::int64_t>::min() ||
        d == std::numeric_limits<std::int64_t>::min())
        throw std::overflow_error("INT64_MIN is outside rational range");
    if (d_ < 0) { n_ = -n_; d_ = -d_; }
    const auto g = std::gcd(n_, d_);
    n_ /= g; d_ /= g;
}
std::int64_t Rational::floor() const {
    return n_ / d_ - (n_ < 0 && n_ % d_ != 0 ? 1 : 0);
}
std::strong_ordering operator<=>(const Rational& a, const Rational& b) {
    if ((a.n_ < 0) != (b.n_ < 0)) return a.n_ < 0 ? std::strong_ordering::less : std::strong_ordering::greater;
    const auto order = compare_positive(magnitude(a.n_), a.d_, magnitude(b.n_), b.d_)
                     * (a.n_ < 0 ? -1 : 1);
    return order < 0 ? std::strong_ordering::less :
           order > 0 ? std::strong_ordering::greater : std::strong_ordering::equal;
}
Rational operator+(const Rational& a, const Rational& b) {
    const auto g = std::gcd(a.d_, b.d_);
    return {add(multiply(a.n_, b.d_ / g), multiply(b.n_, a.d_ / g)),
            multiply(a.d_, b.d_ / g)};
}
Rational operator*(const Rational& a, const Rational& b) {
    const auto g1 = std::gcd(a.n_, b.d_), g2 = std::gcd(b.n_, a.d_);
    return {multiply(a.n_ / g1, b.n_ / g2), multiply(a.d_ / g2, b.d_ / g1)};
}
Rational operator/(const Rational& a, const Rational& b) {
    if (b.n_ == 0) throw std::invalid_argument("division by zero rational");
    return a * Rational(b.d_, b.n_);
}
std::ostream& operator<<(std::ostream& out, const Rational& q) {
    return out << q.numerator() << '/' << q.denominator();
}
} // namespace mcr

