#include "core/secp256k1.hpp"

#include <algorithm>

namespace qchaves::core {
namespace {

struct BigIntWide {
    std::array<std::uint32_t, 16> limbs{};

    bool bit(std::size_t index) const {
        const std::size_t limb = index / 32u;
        const std::size_t offset = index % 32u;
        return ((limbs[limb] >> offset) & 1u) != 0u;
    }
};

BigInt from_hex_literal(const char* text) {
    BigInt out;
    parse_big_int(text, out);
    return out;
}

const BigInt kFieldPrime = from_hex_literal("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFC2F");
const BigInt kCurveOrder = from_hex_literal("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141");
const BigInt kGeneratorX = from_hex_literal("79BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798");
const BigInt kGeneratorY = from_hex_literal("483ADA7726A3C4655DA4FBFC0E1108A8FD17B448A68554199C47D08FFB10D4B8");

bool add_in_place(BigInt& lhs, const BigInt& rhs) {
    std::uint64_t carry = 0;
    for (std::size_t i = 0; i < lhs.limbs.size(); ++i) {
        const std::uint64_t sum = static_cast<std::uint64_t>(lhs.limbs[i]) + rhs.limbs[i] + carry;
        lhs.limbs[i] = static_cast<std::uint32_t>(sum & 0xffffffffu);
        carry = sum >> 32u;
    }
    return carry != 0;
}

bool sub_in_place(BigInt& lhs, const BigInt& rhs) {
    std::uint64_t borrow = 0;
    for (std::size_t i = 0; i < lhs.limbs.size(); ++i) {
        const std::uint64_t left = static_cast<std::uint64_t>(lhs.limbs[i]);
        const std::uint64_t right = static_cast<std::uint64_t>(rhs.limbs[i]) + borrow;
        if (left >= right) {
            lhs.limbs[i] = static_cast<std::uint32_t>(left - right);
            borrow = 0;
        } else {
            lhs.limbs[i] = static_cast<std::uint32_t>((1ull << 32u) + left - right);
            borrow = 1;
        }
    }
    return borrow != 0;
}

bool mul_small_in_place(BigInt& value, std::uint32_t factor) {
    std::uint64_t carry = 0;
    for (std::size_t i = 0; i < value.limbs.size(); ++i) {
        const std::uint64_t product = static_cast<std::uint64_t>(value.limbs[i]) * factor + carry;
        value.limbs[i] = static_cast<std::uint32_t>(product & 0xffffffffu);
        carry = product >> 32u;
    }
    return carry != 0;
}

bool add_small_in_place(BigInt& value, std::uint32_t addend) {
    std::uint64_t carry = addend;
    for (std::size_t i = 0; i < value.limbs.size() && carry != 0; ++i) {
        const std::uint64_t sum = static_cast<std::uint64_t>(value.limbs[i]) + carry;
        value.limbs[i] = static_cast<std::uint32_t>(sum & 0xffffffffu);
        carry = sum >> 32u;
    }
    return carry != 0;
}

BigInt one() {
    return BigInt(1);
}

BigIntWide multiply_wide(const BigInt& lhs, const BigInt& rhs) {
    BigIntWide out;
    for (std::size_t i = 0; i < 8; ++i) {
        std::uint64_t carry = 0;
        for (std::size_t j = 0; j < 8; ++j) {
            const std::size_t index = i + j;
            const std::uint64_t total = static_cast<std::uint64_t>(out.limbs[index]) +
                                        static_cast<std::uint64_t>(lhs.limbs[i]) * rhs.limbs[j] +
                                        carry;
            out.limbs[index] = static_cast<std::uint32_t>(total & 0xffffffffu);
            carry = total >> 32u;
        }
        std::size_t index = i + 8;
        while (carry != 0 && index < out.limbs.size()) {
            const std::uint64_t total = static_cast<std::uint64_t>(out.limbs[index]) + carry;
            out.limbs[index] = static_cast<std::uint32_t>(total & 0xffffffffu);
            carry = total >> 32u;
            ++index;
        }
    }
    return out;
}

BigInt shl1(const BigInt& value) {
    BigInt out;
    std::uint64_t carry = 0;
    for (std::size_t i = 0; i < value.limbs.size(); ++i) {
        const std::uint64_t current = (static_cast<std::uint64_t>(value.limbs[i]) << 1u) | carry;
        out.limbs[i] = static_cast<std::uint32_t>(current & 0xffffffffu);
        carry = current >> 32u;
    }
    return out;
}

BigInt shr1(const BigInt& value) {
    BigInt out;
    std::uint32_t carry = 0;
    for (std::size_t i = value.limbs.size(); i-- > 0;) {
        const std::uint32_t current = value.limbs[i];
        out.limbs[i] = (current >> 1u) | (carry << 31u);
        carry = current & 1u;
    }
    return out;
}

BigInt mod_reduce(const BigIntWide& value, const BigInt& modulus) {
    BigInt result;
    for (std::size_t bit = 512; bit-- > 0;) {
        result = shl1(result);
        if (value.bit(bit)) {
            result.limbs[0] |= 1u;
        }
        if (result >= modulus) {
            result -= modulus;
        }
    }
    return result;
}

BigInt mod_reduce(const BigInt& value, const BigInt& modulus) {
    BigInt result;
    for (std::size_t bit = 256; bit-- > 0;) {
        result = shl1(result);
        if (value.bit(bit)) {
            result.limbs[0] |= 1u;
        }
        if (result >= modulus) {
            result -= modulus;
        }
    }
    return result;
}

BigInt mod_add(const BigInt& lhs, const BigInt& rhs, const BigInt& modulus) {
    BigIntWide wide;
    for (std::size_t i = 0; i < 8; ++i) {
        wide.limbs[i] = lhs.limbs[i];
    }
    std::uint64_t carry = 0;
    for (std::size_t i = 0; i < 8; ++i) {
        const std::uint64_t sum = static_cast<std::uint64_t>(wide.limbs[i]) + rhs.limbs[i] + carry;
        wide.limbs[i] = static_cast<std::uint32_t>(sum & 0xffffffffu);
        carry = sum >> 32u;
    }
    wide.limbs[8] = static_cast<std::uint32_t>(carry);
    return mod_reduce(wide, modulus);
}

BigInt mod_sub(const BigInt& lhs, const BigInt& rhs, const BigInt& modulus) {
    if (lhs >= rhs) {
        return lhs - rhs;
    }
    BigInt tmp = modulus - rhs;
    return tmp + lhs;
}

BigInt mod_mul(const BigInt& lhs, const BigInt& rhs, const BigInt& modulus) {
    return mod_reduce(multiply_wide(lhs, rhs), modulus);
}

BigInt mod_pow(BigInt base, BigInt exponent, const BigInt& modulus) {
    base = mod_reduce(base, modulus);
    BigInt result = one();
    while (!exponent.is_zero()) {
        if (exponent.is_odd()) {
            result = mod_mul(result, base, modulus);
        }
        exponent = shr1(exponent);
        if (!exponent.is_zero()) {
            base = mod_mul(base, base, modulus);
        }
    }
    return result;
}

BigInt mod_inverse(const BigInt& value, const BigInt& modulus) {
    return mod_pow(value, modulus - BigInt(2), modulus);
}

Secp256k1Point point_double(const Secp256k1Point& point) {
    if (point.infinity || point.y.is_zero()) {
        return {};
    }

    const BigInt x2 = mod_mul(point.x, point.x, kFieldPrime);
    const BigInt three_x2 = mod_add(mod_add(x2, x2, kFieldPrime), x2, kFieldPrime);
    const BigInt two_y = mod_add(point.y, point.y, kFieldPrime);
    const BigInt lambda = mod_mul(three_x2, mod_inverse(two_y, kFieldPrime), kFieldPrime);
    const BigInt lambda2 = mod_mul(lambda, lambda, kFieldPrime);
    const BigInt rx = mod_sub(mod_sub(lambda2, point.x, kFieldPrime), point.x, kFieldPrime);
    const BigInt ry = mod_sub(mod_mul(lambda, mod_sub(point.x, rx, kFieldPrime), kFieldPrime), point.y, kFieldPrime);
    return {rx, ry, false};
}

Secp256k1Point point_add(const Secp256k1Point& lhs, const Secp256k1Point& rhs) {
    if (lhs.infinity) {
        return rhs;
    }
    if (rhs.infinity) {
        return lhs;
    }
    if (lhs.x == rhs.x) {
        if (mod_add(lhs.y, rhs.y, kFieldPrime).is_zero()) {
            return {};
        }
        return point_double(lhs);
    }

    const BigInt numerator = mod_sub(rhs.y, lhs.y, kFieldPrime);
    const BigInt denominator = mod_sub(rhs.x, lhs.x, kFieldPrime);
    const BigInt lambda = mod_mul(numerator, mod_inverse(denominator, kFieldPrime), kFieldPrime);
    const BigInt lambda2 = mod_mul(lambda, lambda, kFieldPrime);
    const BigInt rx = mod_sub(mod_sub(lambda2, lhs.x, kFieldPrime), rhs.x, kFieldPrime);
    const BigInt ry = mod_sub(mod_mul(lambda, mod_sub(lhs.x, rx, kFieldPrime), kFieldPrime), lhs.y, kFieldPrime);
    return {rx, ry, false};
}

std::array<std::uint8_t, 32> to_bytes32(const BigInt& value) {
    std::array<std::uint8_t, 32> out{};
    for (std::size_t i = 0; i < 8; ++i) {
        const std::uint32_t limb = value.limbs[i];
        out[31u - i * 4u] = static_cast<std::uint8_t>(limb & 0xffu);
        out[31u - i * 4u - 1u] = static_cast<std::uint8_t>((limb >> 8u) & 0xffu);
        out[31u - i * 4u - 2u] = static_cast<std::uint8_t>((limb >> 16u) & 0xffu);
        out[31u - i * 4u - 3u] = static_cast<std::uint8_t>((limb >> 24u) & 0xffu);
    }
    return out;
}

}  // namespace

BigInt::BigInt(std::uint64_t value) {
    limbs[0] = static_cast<std::uint32_t>(value & 0xffffffffu);
    limbs[1] = static_cast<std::uint32_t>((value >> 32u) & 0xffffffffu);
}

bool BigInt::is_zero() const {
    for (const auto limb : limbs) {
        if (limb != 0u) {
            return false;
        }
    }
    return true;
}

bool BigInt::is_odd() const {
    return (limbs[0] & 1u) != 0u;
}

bool BigInt::fits_u64() const {
    for (std::size_t i = 2; i < limbs.size(); ++i) {
        if (limbs[i] != 0u) {
            return false;
        }
    }
    return true;
}

std::uint64_t BigInt::to_u64() const {
    return static_cast<std::uint64_t>(limbs[0]) |
           (static_cast<std::uint64_t>(limbs[1]) << 32u);
}

bool BigInt::bit(std::size_t index) const {
    const std::size_t limb = index / 32u;
    const std::size_t offset = index % 32u;
    return ((limbs[limb] >> offset) & 1u) != 0u;
}

bool operator==(const BigInt& lhs, const BigInt& rhs) {
    return lhs.limbs == rhs.limbs;
}

bool operator!=(const BigInt& lhs, const BigInt& rhs) {
    return !(lhs == rhs);
}

bool operator<(const BigInt& lhs, const BigInt& rhs) {
    for (std::size_t i = lhs.limbs.size(); i-- > 0;) {
        if (lhs.limbs[i] != rhs.limbs[i]) {
            return lhs.limbs[i] < rhs.limbs[i];
        }
    }
    return false;
}

bool operator>(const BigInt& lhs, const BigInt& rhs) {
    return rhs < lhs;
}

bool operator<=(const BigInt& lhs, const BigInt& rhs) {
    return !(rhs < lhs);
}

bool operator>=(const BigInt& lhs, const BigInt& rhs) {
    return !(lhs < rhs);
}

BigInt operator+(const BigInt& lhs, const BigInt& rhs) {
    BigInt out = lhs;
    add_in_place(out, rhs);
    return out;
}

BigInt operator-(const BigInt& lhs, const BigInt& rhs) {
    BigInt out = lhs;
    sub_in_place(out, rhs);
    return out;
}

BigInt operator<<(const BigInt& value, std::size_t shift) {
    if (shift >= 256u) {
        return {};
    }
    BigInt out;
    const std::size_t limb_shift = shift / 32u;
    const std::size_t bit_shift = shift % 32u;
    for (std::size_t i = out.limbs.size(); i-- > limb_shift;) {
        std::uint64_t current = static_cast<std::uint64_t>(value.limbs[i - limb_shift]) << bit_shift;
        out.limbs[i] |= static_cast<std::uint32_t>(current & 0xffffffffu);
        if (bit_shift != 0u && i + 1 < out.limbs.size()) {
            out.limbs[i + 1] |= static_cast<std::uint32_t>(current >> 32u);
        }
    }
    return out;
}

BigInt operator>>(const BigInt& value, std::size_t shift) {
    if (shift >= 256u) {
        return {};
    }
    BigInt out;
    const std::size_t limb_shift = shift / 32u;
    const std::size_t bit_shift = shift % 32u;
    for (std::size_t i = limb_shift; i < value.limbs.size(); ++i) {
        const std::size_t target = i - limb_shift;
        out.limbs[target] |= value.limbs[i] >> bit_shift;
        if (bit_shift != 0u && i + 1 < value.limbs.size()) {
            out.limbs[target] |= value.limbs[i + 1] << (32u - bit_shift);
        }
    }
    return out;
}

BigInt& operator+=(BigInt& lhs, const BigInt& rhs) {
    add_in_place(lhs, rhs);
    return lhs;
}

BigInt& operator-=(BigInt& lhs, const BigInt& rhs) {
    sub_in_place(lhs, rhs);
    return lhs;
}

BigInt& operator++(BigInt& value) {
    add_small_in_place(value, 1u);
    return value;
}

BigInt& operator--(BigInt& value) {
    sub_in_place(value, BigInt(1));
    return value;
}

bool parse_big_int(const std::string& text, BigInt& out) {
    if (text.empty()) {
        return false;
    }

    std::string value = text;
    int base = 10;
    if (value.size() > 2u && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
        base = 16;
        value = value.substr(2);
    } else {
        bool has_hex_letter = false;
        for (const char ch : value) {
            if ((ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) {
                has_hex_letter = true;
                break;
            }
        }
        if (has_hex_letter) {
            base = 16;
        }
    }

    out = {};
    for (const char ch : value) {
        std::uint32_t digit = 0;
        if (ch >= '0' && ch <= '9') {
            digit = static_cast<std::uint32_t>(ch - '0');
        } else if (base == 16 && ch >= 'a' && ch <= 'f') {
            digit = static_cast<std::uint32_t>(10 + ch - 'a');
        } else if (base == 16 && ch >= 'A' && ch <= 'F') {
            digit = static_cast<std::uint32_t>(10 + ch - 'A');
        } else {
            return false;
        }
        if (digit >= static_cast<std::uint32_t>(base)) {
            return false;
        }
        if (mul_small_in_place(out, static_cast<std::uint32_t>(base)) || add_small_in_place(out, digit)) {
            return false;
        }
    }
    return true;
}

const BigInt& secp256k1_curve_order() {
    return kCurveOrder;
}

bool is_valid_private_key(const BigInt& value) {
    return !value.is_zero() && value < kCurveOrder;
}

bool bigint_to_u64(const BigInt& value, std::uint64_t& out) {
    if (!value.fits_u64()) {
        return false;
    }
    out = value.to_u64();
    return true;
}

std::string bigint_to_hex(const BigInt& value, std::size_t width_bytes) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out(width_bytes * 2u, '0');
    const auto bytes = to_bytes32(value);
    const std::size_t take = std::min(width_bytes, bytes.size());
    const std::size_t skip = bytes.size() - take;
    for (std::size_t i = 0; i < take; ++i) {
        out[i * 2u] = kDigits[(bytes[skip + i] >> 4u) & 0x0f];
        out[i * 2u + 1u] = kDigits[bytes[skip + i] & 0x0f];
    }
    return out;
}

std::string bigint_to_decimal(const BigInt& value) {
    if (value.is_zero()) {
        return "0";
    }
    BigInt temp = value;
    std::string out;
    while (!temp.is_zero()) {
        std::uint64_t remainder = 0;
        for (std::size_t i = temp.limbs.size(); i-- > 0;) {
            const std::uint64_t current = (remainder << 32u) | temp.limbs[i];
            temp.limbs[i] = static_cast<std::uint32_t>(current / 10u);
            remainder = current % 10u;
        }
        out.push_back(static_cast<char>('0' + remainder));
    }
    std::reverse(out.begin(), out.end());
    return out;
}

Secp256k1Point secp256k1_multiply(const BigInt& scalar) {
    if (!is_valid_private_key(scalar)) {
        return {};
    }

    Secp256k1Point result;
    Secp256k1Point addend{kGeneratorX, kGeneratorY, false};
    BigInt k = scalar;
    while (!k.is_zero()) {
        if (k.is_odd()) {
            result = point_add(result, addend);
        }
        addend = point_double(addend);
        k = shr1(k);
    }
    return result;
}

std::vector<std::uint8_t> serialize_pubkey(const Secp256k1Point& point, bool compressed) {
    if (point.infinity) {
        return {};
    }
    const auto x = to_bytes32(point.x);
    const auto y = to_bytes32(point.y);
    std::vector<std::uint8_t> out;
    if (compressed) {
        out.reserve(33);
        out.push_back(point.y.is_odd() ? 0x03u : 0x02u);
        out.insert(out.end(), x.begin(), x.end());
    } else {
        out.reserve(65);
        out.push_back(0x04u);
        out.insert(out.end(), x.begin(), x.end());
        out.insert(out.end(), y.begin(), y.end());
    }
    return out;
}

}  // namespace qchaves::core
