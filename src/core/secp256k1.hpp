#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace qchaves::core {

struct BigInt {
    std::array<std::uint32_t, 8> limbs{};

    BigInt() = default;
    BigInt(std::uint64_t value);

    bool is_zero() const;
    bool is_odd() const;
    bool fits_u64() const;
    std::uint64_t to_u64() const;
    bool bit(std::size_t index) const;
};

bool operator==(const BigInt& lhs, const BigInt& rhs);
bool operator!=(const BigInt& lhs, const BigInt& rhs);
bool operator<(const BigInt& lhs, const BigInt& rhs);
bool operator>(const BigInt& lhs, const BigInt& rhs);
bool operator<=(const BigInt& lhs, const BigInt& rhs);
bool operator>=(const BigInt& lhs, const BigInt& rhs);

BigInt operator+(const BigInt& lhs, const BigInt& rhs);
BigInt operator-(const BigInt& lhs, const BigInt& rhs);
BigInt operator<<(const BigInt& value, std::size_t shift);
BigInt operator>>(const BigInt& value, std::size_t shift);
BigInt& operator+=(BigInt& lhs, const BigInt& rhs);
BigInt& operator-=(BigInt& lhs, const BigInt& rhs);
BigInt& operator++(BigInt& value);
BigInt& operator--(BigInt& value);

struct Secp256k1Point {
    BigInt x{};
    BigInt y{};
    bool infinity = true;
};

struct Secp256k1BackendInfo {
    const char* name = "portable";
    bool optimized = false;
    bool external = false;
};

bool parse_big_int(const std::string& text, BigInt& out);
const BigInt& secp256k1_curve_order();
bool is_valid_private_key(const BigInt& value);
bool bigint_to_u64(const BigInt& value, std::uint64_t& out);
std::string bigint_to_hex(const BigInt& value, std::size_t width_bytes = 32);
std::string bigint_to_decimal(const BigInt& value);
const Secp256k1BackendInfo& secp256k1_backend_info();

Secp256k1Point secp256k1_multiply(const BigInt& scalar);
std::vector<std::uint8_t> serialize_pubkey(const Secp256k1Point& point, bool compressed);

}  // namespace qchaves::core
