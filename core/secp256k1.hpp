#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace bchaves::core {

enum class Secp256k1BackendKind {
    auto_select = 0,
    portable,
    external,
};

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
BigInt operator*(const BigInt& lhs, const BigInt& rhs);
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

extern const BigInt kFieldPrime;

bool parse_big_int(const std::string& text, BigInt& out);
const BigInt& secp256k1_curve_order();
bool is_valid_private_key(const BigInt& value);
bool bigint_to_u64(const BigInt& value, std::uint64_t& out);
std::string bigint_to_hex(const BigInt& value, std::size_t width_bytes = 32);
std::string bigint_to_decimal(const BigInt& value);
bool select_secp256k1_backend(Secp256k1BackendKind backend, std::string& error);
Secp256k1BackendKind active_secp256k1_backend();
const Secp256k1BackendInfo& secp256k1_backend_info();

Secp256k1Point secp256k1_multiply(const BigInt& scalar);
std::vector<std::uint8_t> serialize_pubkey(const Secp256k1Point& point, bool compressed);
Secp256k1Point deserialize_pubkey(const std::vector<std::uint8_t>& data);

Secp256k1Point secp256k1_add(const Secp256k1Point& a, const Secp256k1Point& b);
Secp256k1Point secp256k1_multiply_glv(const BigInt& scalar);

// Aritmética Modular e Utilitários Exportados
BigInt mod_add(const BigInt& a, const BigInt& b, const BigInt& p);
BigInt mod_sub(const BigInt& a, const BigInt& b, const BigInt& p);
BigInt mod_mul(const BigInt& a, const BigInt& b, const BigInt& p);
bool mul_small_in_place(BigInt& value, std::uint32_t multiplier);
std::array<std::uint8_t, 32> to_bytes32(const BigInt& value);
std::string to_hex(const std::vector<std::uint8_t>& data);

std::string to_lower(const std::string& text); 

}  // namespace bchaves::core
