/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Definições e interfaces para curvas elípticas Secp256k1 e BigInt.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
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
    std::array<std::uint64_t, 4> limbs{};

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
// ============================================================
// Conversion Utilities
// ============================================================
inline BigInt bytes32_to_bigint(const std::array<std::uint8_t, 32>& bytes) {
    BigInt out;
    const std::uint64_t* ptr = reinterpret_cast<const std::uint64_t*>(bytes.data());
    out.limbs[3] = __builtin_bswap64(ptr[0]);
    out.limbs[2] = __builtin_bswap64(ptr[1]);
    out.limbs[1] = __builtin_bswap64(ptr[2]);
    out.limbs[0] = __builtin_bswap64(ptr[3]);
    return out;
}

inline std::array<std::uint8_t, 32> to_bytes32(const BigInt& val) {
    std::array<std::uint8_t, 32> out;
    std::uint64_t* ptr = reinterpret_cast<std::uint64_t*>(out.data());
    ptr[0] = __builtin_bswap64(val.limbs[3]);
    ptr[1] = __builtin_bswap64(val.limbs[2]);
    ptr[2] = __builtin_bswap64(val.limbs[1]);
    ptr[3] = __builtin_bswap64(val.limbs[0]);
    return out;
}
BigInt& operator-=(BigInt& lhs, const BigInt& rhs);
BigInt& operator++(BigInt& value);
BigInt& operator--(BigInt& value);

struct Secp256k1Point {
    BigInt x{};
    BigInt y{};
    bool infinity = true;
};

struct PointJacobian {
    BigInt x{};
    BigInt y{};
    BigInt z{};
};

struct Secp256k1BackendInfo {
    const char* name = "portable";
    bool optimized = false;
    bool external = false;
};

extern const BigInt kFieldPrime;
extern const BigInt kCurveOrder;
extern const BigInt kGLV_Beta;
extern const BigInt kGLV_Beta2;
extern const BigInt kGLV_Lambda;
extern const BigInt kGLV_Lambda2;

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
std::size_t serialize_pubkey(const Secp256k1Point& point, bool compressed, std::uint8_t* out);
Secp256k1Point deserialize_pubkey(const std::uint8_t* data, std::size_t length);

Secp256k1Point secp256k1_add(const Secp256k1Point& a, const Secp256k1Point& b);
Secp256k1Point secp256k1_multiply_glv(const BigInt& scalar);

// Performance / Batching API
PointJacobian to_jacobian(const BigInt& x, const BigInt& y);
Secp256k1Point from_jacobian(const PointJacobian& p);
PointJacobian double_point(const PointJacobian& p);
PointJacobian add_points(const PointJacobian& p1, const PointJacobian& p2);
PointJacobian add_points_mixed(const PointJacobian& p1, const Secp256k1Point& p2);
void batch_normalize(PointJacobian* points, Secp256k1Point* outputs, std::size_t count);
void batch_mod_inv_k1(BigInt* values, size_t count, BigInt* scratch);

// Aritmética Modular e Utilitários Exportados
BigInt mod_add(const BigInt& a, const BigInt& b, const BigInt& p);
BigInt mod_sub(const BigInt& a, const BigInt& b, const BigInt& p);
BigInt mod_mul(const BigInt& a, const BigInt& b, const BigInt& p);
BigInt mod_mul_k1(const BigInt& a, const BigInt& b);
BigInt mod_square_k1(const BigInt& a);
bool mul_small_in_place(BigInt& value, std::uint32_t multiplier);
std::array<std::uint8_t, 32> to_bytes32(const BigInt& value);
std::string to_hex(const std::vector<std::uint8_t>& data);

std::string to_lower(const std::string& text); 

}  // namespace bchaves::core
