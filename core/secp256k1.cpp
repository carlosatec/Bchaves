/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Implementação de curvas elípticas Secp256k1 com otimização GLV.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#include "core/secp256k1.hpp"

#include <algorithm>
#include <mutex>
#include <vector>

namespace bchaves::core {

namespace {

void mul_wide_256(const BigInt& a, const BigInt& b, std::uint64_t out[8]) {
    for (std::size_t i = 0; i < 8; ++i) out[i] = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        unsigned __int128 carry = 0;
        for (std::size_t j = 0; j < 4; ++j) {
            const unsigned __int128 cur =
                static_cast<unsigned __int128>(a.limbs[i]) * b.limbs[j] + out[i + j] + carry;
            out[i + j] = static_cast<std::uint64_t>(cur);
            carry = cur >> 64u;
        }
        for (std::size_t k = i + 4; carry != 0 && k < 8; ++k) {
            const unsigned __int128 cur = static_cast<unsigned __int128>(out[k]) + carry;
            out[k] = static_cast<std::uint64_t>(cur);
            carry = cur >> 64u;
        }
    }
}

int compare_5_to_4(const std::array<std::uint64_t, 5>& lhs, const BigInt& rhs) {
    if (lhs[4] != 0) return 1;
    for (int i = 3; i >= 0; --i) {
        if (lhs[static_cast<std::size_t>(i)] != rhs.limbs[static_cast<std::size_t>(i)]) {
            return lhs[static_cast<std::size_t>(i)] < rhs.limbs[static_cast<std::size_t>(i)] ? -1 : 1;
        }
    }
    return 0;
}

void sub_5_by_4(std::array<std::uint64_t, 5>& lhs, const BigInt& rhs) {
    std::uint64_t borrow = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        const unsigned __int128 left = lhs[i];
        const unsigned __int128 right = static_cast<unsigned __int128>(rhs.limbs[i]) + borrow;
        if (left >= right) {
            lhs[i] = static_cast<std::uint64_t>(left - right);
            borrow = 0;
        } else {
            lhs[i] = static_cast<std::uint64_t>((static_cast<unsigned __int128>(1) << 64u) + left - right);
            borrow = 1;
        }
    }
    if (borrow != 0) {
        lhs[4] -= borrow;
    }
}

BigInt reduce_wide_mod(const std::uint64_t wide[8], const BigInt& mod) {
    std::array<std::uint64_t, 5> rem{};
    for (int bit = 511; bit >= 0; --bit) {
        std::uint64_t carry = 0;
        for (std::size_t limb = 0; limb < rem.size(); ++limb) {
            const std::uint64_t next_carry = rem[limb] >> 63u;
            rem[limb] = (rem[limb] << 1u) | carry;
            carry = next_carry;
        }
        const std::size_t word = static_cast<std::size_t>(bit / 64);
        const std::size_t shift = static_cast<std::size_t>(bit % 64);
        rem[0] |= (wide[word] >> shift) & 1ULL;

        if (compare_5_to_4(rem, mod) >= 0) {
            sub_5_by_4(rem, mod);
        }
    }

    BigInt out;
    for (std::size_t i = 0; i < 4; ++i) out.limbs[i] = rem[i];
    return out;
}

BigInt add_mod_exact(const BigInt& a, const BigInt& b, const BigInt& mod) {
    std::array<std::uint64_t, 5> sum{};
    unsigned __int128 carry = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        carry = static_cast<unsigned __int128>(a.limbs[i]) + b.limbs[i] + carry;
        sum[i] = static_cast<std::uint64_t>(carry);
        carry >>= 64u;
    }
    sum[4] = static_cast<std::uint64_t>(carry);
    if (compare_5_to_4(sum, mod) >= 0) {
        sub_5_by_4(sum, mod);
    }
    BigInt out;
    for (std::size_t i = 0; i < 4; ++i) out.limbs[i] = sum[i];
    return out;
}

BigInt sub_mod_exact(const BigInt& a, const BigInt& b, const BigInt& mod) {
    if (a >= b) {
        return a - b;
    }

    std::array<std::uint64_t, 5> value{};
    unsigned __int128 carry = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        carry = static_cast<unsigned __int128>(a.limbs[i]) + mod.limbs[i] + carry;
        value[i] = static_cast<std::uint64_t>(carry);
        carry >>= 64u;
    }
    value[4] = static_cast<std::uint64_t>(carry);
    sub_5_by_4(value, b);

    BigInt out;
    for (std::size_t i = 0; i < 4; ++i) out.limbs[i] = value[i];
    return out;
}

}  // namespace

// BigInt methods
BigInt::BigInt(std::uint64_t value) {
    limbs[0] = value;
}

bool BigInt::is_zero() const {
    return limbs[0] == 0 && limbs[1] == 0 && limbs[2] == 0 && limbs[3] == 0;
}

bool BigInt::is_odd() const {
    return (limbs[0] & 1) != 0;
}

bool BigInt::fits_u64() const {
    return limbs[1] == 0 && limbs[2] == 0 && limbs[3] == 0;
}

std::uint64_t BigInt::to_u64() const {
    return limbs[0];
}

bool BigInt::bit(std::size_t index) const {
    std::size_t limb = index / 64;
    std::size_t offset = index % 64;
    if (limb >= limbs.size()) return false;
    return (limbs[limb] >> offset) & 1;
}

// Operators
bool operator==(const BigInt& lhs, const BigInt& rhs) {
    return lhs.limbs == rhs.limbs;
}

bool operator!=(const BigInt& lhs, const BigInt& rhs) {
    return !(lhs == rhs);
}

bool operator<(const BigInt& lhs, const BigInt& rhs) {
    for (std::size_t i = lhs.limbs.size(); i-- > 0;) {
        if (lhs.limbs[i] != rhs.limbs[i]) return lhs.limbs[i] < rhs.limbs[i];
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
    BigInt out;
    unsigned __int128 carry = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        carry += (unsigned __int128)lhs.limbs[i] + rhs.limbs[i];
        out.limbs[i] = (std::uint64_t)carry;
        carry >>= 64;
    }
    return out;
}

BigInt operator-(const BigInt& lhs, const BigInt& rhs) {
    BigInt out;
    __int128 borrow = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        borrow = (__int128)lhs.limbs[i] - rhs.limbs[i] - (borrow < 0 ? 1 : 0);
        out.limbs[i] = (std::uint64_t)borrow;
    }
    return out;
}

BigInt operator<<(const BigInt& value, std::size_t shift) {
    if (shift >= 256) return {};
    BigInt out;
    std::size_t limb_shift = shift / 64;
    std::size_t bit_shift = shift % 64;
    for (std::size_t i = 3; i >= limb_shift; --i) {
        out.limbs[i] = value.limbs[i - limb_shift] << bit_shift;
        if (bit_shift && i > limb_shift) {
            out.limbs[i] |= value.limbs[i - limb_shift - 1] >> (64 - bit_shift);
        }
        if (i == 0) break;
    }
    return out;
}

BigInt operator>>(const BigInt& value, std::size_t shift) {
    if (shift >= 256) return {};
    BigInt out;
    std::size_t limb_shift = shift / 64;
    std::size_t bit_shift = shift % 64;
    for (std::size_t i = 0; i < 4 - limb_shift; ++i) {
        out.limbs[i] = value.limbs[i + limb_shift] >> bit_shift;
        if (bit_shift && i + 1 < 4 - limb_shift) {
            out.limbs[i] |= value.limbs[i + limb_shift + 1] << (64 - bit_shift);
        }
    }
    return out;
}

BigInt& operator+=(BigInt& lhs, const BigInt& rhs) {
    unsigned __int128 carry = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        carry += (unsigned __int128)lhs.limbs[i] + rhs.limbs[i];
        lhs.limbs[i] = (std::uint64_t)carry;
        carry >>= 64;
    }
    return lhs;
}

BigInt& operator-=(BigInt& lhs, const BigInt& rhs) {
    __int128 borrow = 0;
    for (std::size_t i = 0; i < 4; ++i) {
        __int128 diff = (__int128)lhs.limbs[i] - rhs.limbs[i] - borrow;
        lhs.limbs[i] = (std::uint64_t)diff;
        borrow = diff < 0 ? 1 : 0;
    }
    return lhs;
}

BigInt& operator++(BigInt& value) {
    unsigned __int128 carry = 1;
    for (std::size_t i = 0; i < 4 && carry; ++i) {
        carry += (unsigned __int128)value.limbs[i];
        value.limbs[i] = (std::uint64_t)carry;
        carry >>= 64;
    }
    return value;
}

BigInt& operator--(BigInt& value) {
    value -= BigInt(1);
    return value;
}

// BigInt Multiplication
BigInt operator*(const BigInt& lhs, const BigInt& rhs) {
    std::uint64_t wide[8]{};
    mul_wide_256(lhs, rhs, wide);
    BigInt out;
    for (std::size_t i = 0; i < 4; ++i) out.limbs[i] = wide[i];
    return out;
}

BigInt operator%(const BigInt& lhs, const BigInt& rhs) {
    if (rhs.is_zero()) return {};
    if (lhs < rhs) return lhs;
    BigInt rem = lhs;
    BigInt d = rhs;
    int shift = 0;
    while (!(d.limbs[3] & 0x8000000000000000ULL) && (d << 1) <= rem) {
        d = (d << 1);
        ++shift;
    }
    for (int i = 0; i <= shift; ++i) {
        if (rem >= d) rem -= d;
        d = (d >> 1);
    }
    return rem;
}

BigInt operator/(const BigInt& lhs, const BigInt& rhs) {
    if (rhs.is_zero()) return {};
    BigInt quote;
    BigInt rem = lhs;
    BigInt d = rhs;
    int shift = 0;
    while (!(d.limbs[3] & 0x8000000000000000ULL) && (d << 1) <= rem) {
        d = (d << 1);
        ++shift;
    }
    for (int i = 0; i <= shift; ++i) {
        if (rem >= d) {
            rem -= d;
            const int bit_pos = shift - i;
            quote.limbs[static_cast<std::size_t>(bit_pos / 64)] |= (1ULL << (bit_pos % 64));
        }
        d = (d >> 1);
    }
    return quote;
}

bool mul_small_in_place(BigInt& value, std::uint32_t multiplier) {
    unsigned __int128 carry = 0;
    for (std::size_t i = 0; i < value.limbs.size(); ++i) {
        const unsigned __int128 prod =
            static_cast<unsigned __int128>(value.limbs[i]) * multiplier + carry;
        value.limbs[i] = static_cast<std::uint64_t>(prod);
        carry = prod >> 64u;
    }
    return carry != 0;
}

BigInt mod_add(const BigInt& a, const BigInt& b, const BigInt& p) {
    return add_mod_exact(a, b, p);
}

BigInt mod_sub(const BigInt& a, const BigInt& b, const BigInt& p) {
    return sub_mod_exact(a, b, p);
}

// Fast 256-bit reduction for P = 2^256 - 2^32 - 977
void reduce_p256_64(std::uint64_t* res, const std::uint64_t* wide) {
    // P = 2^256 - 2^32 - 977 => 2^256 = 2^32 + 977
    // Wide = [L0, L1, L2, L3, H0, H1, H2, H3]
    // Result = L + H * (2^32 + 977)
    
    unsigned __int128 carry = 0;
    std::uint64_t h[4] = {wide[4], wide[5], wide[6], wide[7]};
    
    // Step 1: res = L + H * 977
    for (int i = 0; i < 4; ++i) {
        carry += (unsigned __int128)h[i] * 977 + wide[i];
        res[i] = (std::uint64_t)carry;
        carry >>= 64;
    }
    
    // Step 2: res = res + (H << 32)
    unsigned __int128 c = carry;
    for(int i=0; i<4; ++i) {
        uint64_t term = (h[i] << 32);
        if (i > 0) term |= (h[i-1] >> 32);
        
        c += (unsigned __int128)res[i] + term;
        res[i] = (uint64_t)c;
        c >>= 64;
    }
    
    // Step 3: ripple high bits (H << 32 high part and carry)
    uint64_t ripple = (h[3] >> 32) + (uint64_t)c;
    if (ripple > 0) {
        unsigned __int128 c2 = 0;
        uint64_t extra = ripple * 977;
        for(int i=0; i<4; ++i) {
            uint64_t term = (i == 0) ? (extra) : 0;
            // ripple << 32
            if (i == 0) term += (ripple << 32);
            if (i == 1) term += (ripple >> 32);
            
            c2 += (unsigned __int128)res[i] + term;
            res[i] = (uint64_t)c2;
            c2 >>= 64;
        }
    }
}

BigInt mod_mul_k1(const BigInt& a, const BigInt& b) {
    std::uint64_t wide[8]{};
    mul_wide_256(a, b, wide);
    return reduce_wide_mod(wide, kFieldPrime);
}

BigInt mod_square_k1(const BigInt& a) {
    return mod_mul_k1(a, a);
}

// Optimized mod_mul
BigInt mod_mul(const BigInt& a, const BigInt& b, const BigInt& p) {
    std::uint64_t wide[8]{};
    mul_wide_256(a, b, wide);
    return reduce_wide_mod(wide, p);
}

std::array<std::uint8_t, 32> to_bytes32(const BigInt& value) {
    std::array<std::uint8_t, 32> out{};
    for (int i = 0; i < 4; ++i) {
        std::uint64_t limb = value.limbs[i];
        for(int j=0; j<8; ++j) {
            out[31 - (i * 8 + j)] = static_cast<std::uint8_t>((limb >> (j * 8)) & 0xff);
        }
    }
    return out;
}

static BigInt parse_hex(const char* text) {
    BigInt out;
    parse_big_int(text, out);
    return out;
}

const BigInt kFieldPrime = parse_hex("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFC2F");
const BigInt kCurveOrder = parse_hex("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141");
const BigInt kGeneratorX = parse_hex("79BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798");
const BigInt kGeneratorY = parse_hex("483ADA7726A3C4655DA4FBFC0E1108A8FD17B448A68554199C47D08FFB10D4B8");

// GLV Constants
const BigInt kGLV_Beta = parse_hex("7AE96A2B657C07106E64479EAC3434E99CF0497512F58995C1396C28719501EE");
const BigInt kGLV_Beta2 = parse_hex("851695D49A83F8EF919BB86153CBCB16630FB68AED0A766A3EC693D68E6AFA40");
const BigInt kGLV_Lambda = parse_hex("5363AD4CC05C30E0A5261C028812645A122E22EA2081667870F197FBF390947");
const BigInt kGLV_Lambda2 = parse_hex("AC9C52B33FA3CF1F5AD9E3FD77ED9BA4A880B9FC8EC739C2E0CFC810B51283CE");

std::string to_hex(const std::vector<std::uint8_t>& data) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out;
    out.reserve(data.size() * 2);
    for (auto b : data) {
        out.push_back(kDigits[(b >> 4) & 0xf]);
        out.push_back(kDigits[b & 0xf]);
    }
    return out;
}

BigInt mod_pow_k1(BigInt base, BigInt exponent) {
    BigInt result(1);
    while (!exponent.is_zero()) {
        if (exponent.is_odd()) {
            result = mod_mul_k1(result, base);
        }
        exponent = exponent >> 1;
        if (!exponent.is_zero()) {
            base = mod_square_k1(base);
        }
    }
    return result;
}

bool is_point_on_curve(const BigInt& x, const BigInt& y) {
    if (x >= kFieldPrime || y >= kFieldPrime) {
        return false;
    }
    const BigInt lhs = mod_square_k1(y);
    const BigInt rhs = mod_add(mod_mul_k1(mod_square_k1(x), x), BigInt(7), kFieldPrime);
    return lhs == rhs;
}

// Modular Inversion using Extended Euclidean Algorithm
BigInt mod_inv(const BigInt& a, const BigInt& p) {
    if (a.is_zero()) return BigInt(0);
    if (p == kFieldPrime) {
        return mod_pow_k1(a, kFieldPrime - BigInt(2));
    }

    BigInt exponent = p - BigInt(2);
    BigInt result(1);
    BigInt base = a % p;
    while (!exponent.is_zero()) {
        if (exponent.is_odd()) {
            result = mod_mul(result, base, p);
        }
        exponent = exponent >> 1;
        if (!exponent.is_zero()) {
            base = mod_mul(base, base, p);
        }
    }
    return result;
}

PointJacobian to_jacobian(const BigInt& x, const BigInt& y) {
    return {x, y, 1};
}

Secp256k1Point from_jacobian(const PointJacobian& p) {
    if (p.z.is_zero()) return {0, 0, true};
    BigInt z_inv = mod_inv(p.z, kFieldPrime);
    BigInt z_inv2 = mod_square_k1(z_inv);
    BigInt z_inv3 = mod_mul_k1(z_inv2, z_inv);
    return {mod_mul_k1(p.x, z_inv2), mod_mul_k1(p.y, z_inv3), false};
}

PointJacobian double_point(const PointJacobian& p) {
    if (p.z.is_zero()) return p;
    BigInt y2 = mod_square_k1(p.y);
    BigInt s = mod_mul_k1(BigInt(4), mod_mul_k1(p.x, y2));
    BigInt m = mod_mul_k1(BigInt(3), mod_square_k1(p.x));
    BigInt x_res = mod_sub(mod_square_k1(m), mod_mul_k1(BigInt(2), s), kFieldPrime);
    BigInt y_res = mod_sub(mod_mul_k1(m, mod_sub(s, x_res, kFieldPrime)), 
                           mod_mul_k1(BigInt(8), mod_square_k1(y2)), kFieldPrime);
    BigInt z_res = mod_mul_k1(BigInt(2), mod_mul_k1(p.y, p.z));
    return {x_res, y_res, z_res};
}

PointJacobian add_points(const PointJacobian& p1, const PointJacobian& p2) {
    if (p1.z.is_zero()) return p2;
    if (p2.z.is_zero()) return p1;
    BigInt z1_2 = mod_square_k1(p1.z);
    BigInt z2_2 = mod_square_k1(p2.z);
    BigInt u1 = mod_mul_k1(p1.x, z2_2);
    BigInt u2 = mod_mul_k1(p2.x, z1_2);
    BigInt s1 = mod_mul_k1(p1.y, mod_mul_k1(p2.z, z2_2));
    BigInt s2 = mod_mul_k1(p2.y, mod_mul_k1(p1.z, z1_2));
    if (u1 == u2) return (s1 == s2) ? double_point(p1) : PointJacobian{0, 0, 0};
    BigInt h = mod_sub(u2, u1, kFieldPrime);
    BigInt r = mod_sub(s2, s1, kFieldPrime);
    BigInt h2 = mod_square_k1(h);
    BigInt h3 = mod_mul_k1(h, h2);
    BigInt v = mod_mul_k1(u1, h2);
    BigInt x_res = mod_sub(mod_sub(mod_square_k1(r), h3, kFieldPrime), mod_mul_k1(BigInt(2), v), kFieldPrime);
    BigInt y_res = mod_sub(mod_mul_k1(r, mod_sub(v, x_res, kFieldPrime)), mod_mul_k1(s1, h3), kFieldPrime);
    BigInt z_res = mod_mul_k1(mod_mul_k1(p1.z, p2.z), h);
    return {x_res, y_res, z_res};
}

PointJacobian add_points_mixed(const PointJacobian& p1, const Secp256k1Point& p2) {
    if (p1.z.is_zero()) return to_jacobian(p2.x, p2.y);
    if (p2.infinity) return p1;
    BigInt z1_2 = mod_square_k1(p1.z);
    BigInt u2 = mod_mul_k1(p2.x, z1_2);
    BigInt s2 = mod_mul_k1(p2.y, mod_mul_k1(p1.z, z1_2));
    if (p1.x == u2) return (p1.y == s2) ? double_point(p1) : PointJacobian{0, 0, 0};
    BigInt h = mod_sub(u2, p1.x, kFieldPrime);
    BigInt r = mod_sub(s2, p1.y, kFieldPrime);
    BigInt h2 = mod_square_k1(h);
    BigInt h3 = mod_mul_k1(h, h2);
    BigInt v = mod_mul_k1(p1.x, h2);
    BigInt x_res = mod_sub(mod_sub(mod_square_k1(r), h3, kFieldPrime), mod_mul_k1(BigInt(2), v), kFieldPrime);
    BigInt y_res = mod_sub(mod_mul_k1(r, mod_sub(v, x_res, kFieldPrime)), mod_mul_k1(p1.y, h3), kFieldPrime);
    BigInt z_res = mod_mul_k1(p1.z, h);
    return {x_res, y_res, z_res};
}

void batch_normalize(PointJacobian* points, Secp256k1Point* outputs, std::size_t count) {
    if (count == 0) return;
    constexpr std::size_t MAX_STACK = 4096;
    alignas(32) std::uint64_t prod_storage[MAX_STACK * 4];
    std::vector<BigInt> dynamic_prods;
    BigInt* prods = nullptr;
    if (count <= MAX_STACK) {
        prods = reinterpret_cast<BigInt*>(prod_storage);
    } else {
        dynamic_prods.resize(count);
        prods = dynamic_prods.data();
    }
    prods[0] = points[0].z;
    if (prods[0].is_zero()) prods[0] = 1;
    for (std::size_t i = 1; i < count; ++i) {
        BigInt z = points[i].z;
        if (z.is_zero()) z = 1;
        prods[i] = mod_mul_k1(prods[i-1], z);
    }
    BigInt inv = mod_inv(prods[count-1], kFieldPrime);
    for (std::size_t i = count - 1; i > 0; --i) {
        BigInt z = points[i].z;
        if (z.is_zero()) {
            outputs[i] = {0, 0, true};
            continue;
        }
        BigInt z_inv = mod_mul_k1(inv, prods[i-1]);
        inv = mod_mul_k1(inv, z);
        BigInt z_inv2 = mod_square_k1(z_inv);
        BigInt z_inv3 = mod_mul_k1(z_inv2, z_inv);
        outputs[i] = {mod_mul_k1(points[i].x, z_inv2), mod_mul_k1(points[i].y, z_inv3), false};
    }
    BigInt z_inv = inv;
    BigInt z_inv2 = mod_square_k1(z_inv);
    BigInt z_inv3 = mod_mul_k1(z_inv2, z_inv);
    outputs[0] = {mod_mul_k1(points[0].x, z_inv2), mod_mul_k1(points[0].y, z_inv3), points[0].z.is_zero()};
}

void batch_mod_inv_k1(BigInt* values, size_t count, BigInt* scratch) {
    if (count == 0) return;
    scratch[0] = values[0];
    if (scratch[0].is_zero()) scratch[0] = 1;
    for (size_t i = 1; i < count; ++i) {
        BigInt v = values[i];
        if (v.is_zero()) v = 1;
        scratch[i] = mod_mul_k1(scratch[i-1], v);
    }
    BigInt inv = mod_inv(scratch[count-1], kFieldPrime);
    for (size_t i = count - 1; i > 0; --i) {
        BigInt v = values[i];
        if (v.is_zero()) continue;
        values[i] = mod_mul_k1(inv, scratch[i-1]);
        inv = mod_mul_k1(inv, v);
    }
    if (!values[0].is_zero()) {
        values[0] = inv;
    }
}

Secp256k1Point secp256k1_add(const Secp256k1Point& a, const Secp256k1Point& b) {
    if (a.infinity) return b;
    if (b.infinity) return a;
    PointJacobian p1 = to_jacobian(a.x, a.y);
    PointJacobian res = add_points_mixed(p1, b);
    return from_jacobian(res);
}

namespace {
Secp256k1BackendKind g_selected_backend = Secp256k1BackendKind::portable;
const Secp256k1BackendInfo kInfoPort{"portable-64bit", true, false};

}  // anonymous namespace

Secp256k1BackendKind active_secp256k1_backend() {
    return g_selected_backend;
}

const Secp256k1BackendInfo& secp256k1_backend_info() {
    return kInfoPort;
}

// Refatorado - usando versões globais acima

Secp256k1Point secp256k1_multiply(const BigInt& scalar) {
    if (scalar.is_zero()) return {};
    
    // Windowed Scalar Multiplication (4-bit window)
    // Reduz o número de adições de ~128 (média) para ~64.
    static std::once_flag precomputed_once;
    static PointJacobian window[16];
    std::call_once(precomputed_once, []() {
        window[0] = {0, 0, 0}; // Unused
        window[1] = to_jacobian(kGeneratorX, kGeneratorY);
        for (int i = 2; i < 16; ++i) {
            if (i % 2 == 0) window[i] = double_point(window[i/2]);
            else window[i] = add_points(window[i-1], window[1]);
        }
    });

    PointJacobian res = {0, 0, 0};
    for (int i = 252; i >= 0; i -= 4) {
        res = double_point(res);
        res = double_point(res);
        res = double_point(res);
        res = double_point(res);
        
        uint32_t chunk = 0;
        for (int j = 0; j < 4; ++j) {
            if (scalar.bit(i + j)) chunk |= (1u << j);
        }
        if (chunk > 0) res = add_points(res, window[chunk]);
    }
    return from_jacobian(res);
}

std::vector<std::uint8_t> serialize_pubkey(const Secp256k1Point& point, bool compressed);
Secp256k1Point deserialize_pubkey(const std::vector<std::uint8_t>& data);

std::size_t serialize_pubkey(const Secp256k1Point& point, bool compressed, std::uint8_t* out) {
    if (point.infinity) return 0;
    if (compressed) {
        out[0] = point.y.is_odd() ? 0x03u : 0x02u;
        const auto x_bytes = to_bytes32(point.x);
        std::copy(x_bytes.begin(), x_bytes.end(), out + 1);
        return 33;
    } else {
        out[0] = 0x04u;
        const auto x_bytes = to_bytes32(point.x);
        const auto y_bytes = to_bytes32(point.y);
        std::copy(x_bytes.begin(), x_bytes.end(), out + 1);
        std::copy(y_bytes.begin(), y_bytes.end(), out + 33);
        return 65;
    }
}

// Removido - movido para o namespace público acima

std::string to_lower(const std::string& text) {
    std::string out = text;
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c += ('a' - 'A');
    }
    return out;
}

Secp256k1Point deserialize_pubkey(const std::uint8_t* data, std::size_t length) {
    if (length == 0) return {};
    if (data[0] == 0x04 && length == 65) {
        Secp256k1Point p;
        p.infinity = false;
        std::vector<uint8_t> x_v(data + 1, data + 33);
        std::vector<uint8_t> y_v(data + 33, data + 65);
        if (!parse_big_int(bchaves::core::to_hex(x_v).c_str(), p.x) ||
            !parse_big_int(bchaves::core::to_hex(y_v).c_str(), p.y) ||
            !is_point_on_curve(p.x, p.y)) {
            return {};
        }
        return p;
    }
    if ((data[0] == 0x02 || data[0] == 0x03) && length == 33) {
        Secp256k1Point p;
        p.infinity = false;
        std::vector<uint8_t> x_v(data + 1, data + 33);
        if (!parse_big_int(bchaves::core::to_hex(x_v).c_str(), p.x) || p.x >= kFieldPrime) {
            return {};
        }

        const BigInt rhs = mod_add(mod_mul_k1(mod_square_k1(p.x), p.x), BigInt(7), kFieldPrime);
        const BigInt sqrt_exp = (kFieldPrime + BigInt(1)) >> 2;
        BigInt y = mod_pow_k1(rhs, sqrt_exp);
        if (mod_square_k1(y) != rhs) {
            return {};
        }

        const bool expected_odd = data[0] == 0x03;
        if (y.is_odd() != expected_odd) {
            y = mod_sub(kFieldPrime, y, kFieldPrime);
        }
        if (!is_point_on_curve(p.x, y)) {
            return {};
        }

        p.y = y;
        return p; 
    }
    return {};
}

Secp256k1Point phi(const Secp256k1Point& p) {
    if (p.infinity) return p;
    return {mod_mul(p.x, kGLV_Beta, kFieldPrime), p.y, false};
}

Secp256k1Point multi_multiply_128(const Secp256k1Point& p1, const BigInt& s1, const Secp256k1Point& p2, const BigInt& s2) {
    PointJacobian res = {0, 0, 0};
    PointJacobian b1 = to_jacobian(p1.x, p1.y);
    PointJacobian b2 = to_jacobian(p2.x, p2.y);
    PointJacobian combined = add_points(b1, b2);

    // Shamir's Trick: Busca combinada 4-way
    for (int i = 127; i >= 0; --i) {
        res = double_point(res);
        bool bit1 = s1.bit(i);
        bool bit2 = s2.bit(i);
        if (bit1 && bit2) res = add_points(res, combined);
        else if (bit1) res = add_points(res, b1);
        else if (bit2) res = add_points(res, b2);
    }
    return from_jacobian(res);
}

void decompose_glv(const BigInt& k, BigInt& k1, BigInt& k2, bool& k1_neg, bool& k2_neg) {
    static const BigInt n = kCurveOrder;
    static const BigInt b11 = parse_hex("3086D221A7D46BC882C60D1B");
    static const BigInt b21 = parse_hex("E79E57A8705B4A33830ACDC355AC8123");
    // b12 = -b21, b22 = b11

    // c1 = round(k * b11 / n)
    // c2 = round(k * b21 / n)
    // Usamos aproximação de 128 bits para o arredondamento
    unsigned __int128 k_hi = ((unsigned __int128)k.limbs[3] << 64) | k.limbs[2];
    unsigned __int128 n_hi = ((unsigned __int128)n.limbs[3] << 64) | n.limbs[2];
    unsigned __int128 b11_val = ((unsigned __int128)b11.limbs[1] << 64) | b11.limbs[0];
    unsigned __int128 b21_val = ((unsigned __int128)b21.limbs[1] << 64) | b21.limbs[0];

    unsigned __int128 c1 = (k_hi * b11_val + (n_hi >> 1)) / n_hi;
    unsigned __int128 c2 = (k_hi * b21_val + (n_hi >> 1)) / n_hi;

    // k1 = k - (c1*b11 + c2*b21)
    // k2 = c1*b21 - c2*b11
    BigInt term11 = BigInt((uint64_t)c1) * b11;
    BigInt term21 = BigInt((uint64_t)c2) * b21;
    BigInt sum_k1 = term11 + term21;
    
    if (k >= sum_k1) {
        k1 = k - sum_k1;
        k1_neg = false;
    } else {
        k1 = sum_k1 - k;
        k1_neg = true;
    }

    BigInt term12 = BigInt((uint64_t)c1) * b21;
    BigInt term22 = BigInt((uint64_t)c2) * b11;
    
    if (term12 >= term22) {
        k2 = term12 - term22;
        k2_neg = false;
    } else {
        k2 = term22 - term12;
        k2_neg = true;
    }
}

Secp256k1Point secp256k1_multiply_glv(const BigInt& scalar) {
    if (scalar.is_zero()) return {};
    
    BigInt k1, k2;
    bool k1_neg, k2_neg;
    decompose_glv(scalar, k1, k2, k1_neg, k2_neg);
    
    if (k2.is_zero()) return secp256k1_multiply(k1);
    
    Secp256k1Point P1 = {kGeneratorX, kGeneratorY, false};
    Secp256k1Point P2 = phi(P1);
    
    if (k1_neg) P1.y = mod_sub(BigInt(0), P1.y, kFieldPrime);
    if (k2_neg) P2.y = mod_sub(BigInt(0), P2.y, kFieldPrime);
    
    return multi_multiply_128(P1, k1, P2, k2);
}

bool parse_big_int(const std::string& text, BigInt& out) {
    if (text.empty()) return false;

    std::string value = text;
    int base = 10;
    if (value.size() > 2u && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
        base = 16;
        value = value.substr(2);
    } else {
        bool has_hex = false;
        for (char ch : value) {
            if ((ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) { has_hex = true; break; }
        }
        if (has_hex) base = 16;
    }

    out = {};
    for (char ch : value) {
        std::uint64_t digit = 0;
        if (ch >= '0' && ch <= '9') digit = ch - '0';
        else if (base == 16 && ch >= 'a' && ch <= 'f') digit = 10 + ch - 'a';
        else if (base == 16 && ch >= 'A' && ch <= 'F') digit = 10 + ch - 'A';
        else return false;
        if (digit >= static_cast<std::uint64_t>(base)) return false;
        
        // mul_small_in_place(out, base)
        unsigned __int128 carry = digit;
        for (int i = 0; i < 4; ++i) {
            carry += (unsigned __int128)out.limbs[i] * base;
            out.limbs[i] = (uint64_t)carry;
            carry >>= 64;
        }
        if (carry) return false; // overflow
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
    if (!value.fits_u64()) return false;
    out = value.to_u64();
    return true;
}

std::string bigint_to_hex(const BigInt& value, std::size_t width) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out;
    auto bytes = to_bytes32(value);
    std::size_t start = 32 - std::min((std::size_t)32, width);
    for (std::size_t i = start; i < 32; ++i) {
        out.push_back(kDigits[(bytes[i] >> 4) & 0xf]);
        out.push_back(kDigits[bytes[i] & 0xf]);
    }
    return out;
}

std::string bigint_to_decimal(const BigInt& value) {
    if (value.is_zero()) return "0";
    BigInt temp = value;
    std::string out;
    while (!temp.is_zero()) {
        std::uint64_t rem = 0;
        for (int i = 3; i >= 0; --i) {
            unsigned __int128 cur = ((unsigned __int128)rem << 64) | temp.limbs[i];
            temp.limbs[i] = static_cast<uint64_t>(cur / 10);
            rem = static_cast<uint64_t>(cur % 10);
        }
        out.push_back('0' + (uint8_t)rem);
    }
    std::reverse(out.begin(), out.end());
    return out;
}

bool select_secp256k1_backend(Secp256k1BackendKind backend, std::string& error) {
    if (backend == Secp256k1BackendKind::auto_select) {
        g_selected_backend = Secp256k1BackendKind::portable;
        return true;
    }
    if (backend == Secp256k1BackendKind::portable) {
        g_selected_backend = Secp256k1BackendKind::portable;
        return true;
    }
    error = "backend externo nao disponivel";
    return false;
}

}  // namespace bchaves::core
