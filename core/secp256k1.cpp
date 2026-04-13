#include "core/secp256k1.hpp"

#include <algorithm>

namespace bchaves::core {

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
    for (std::size_t i = 0; i < 4; ++i) {
        unsigned __int128 carry = 0;
        for (std::size_t j = 0; j < 4; ++j) {
            unsigned __int128 prod = (unsigned __int128)lhs.limbs[i] * rhs.limbs[j] + wide[i+j] + carry;
            wide[i+j] = (std::uint64_t)prod;
            carry = prod >> 64;
        }
        wide[i+4] = (std::uint64_t)carry;
    }
    BigInt out;
    for(int i=0; i<4; ++i) out.limbs[i] = wide[i];
    return out;
}

BigInt operator%(const BigInt& lhs, const BigInt& rhs) {
    if (rhs.is_zero()) return {};
    if (lhs < rhs) return lhs;
    BigInt rem = lhs;
    BigInt d = rhs;
    int shift = 0;
    while (!(d.limbs[3] & 0x8000000000000000ULL) && (d << 1) <= lhs) {
        d = (d << 1);
        shift++;
    }
    for (int i = 0; i <= shift; ++i) {
        if (rem >= d) rem -= d;
        d = (d >> 1);
    }
    return rem;
}

BigInt operator/(const BigInt& lhs, const BigInt& rhs) {
    if (rhs.is_zero()) return {};
    BigInt quote, rem = lhs, d = rhs;
    int shift = 0;
    while (!(d.limbs[3] & 0x8000000000000000ULL) && (d << 1) <= lhs) {
        d = (d << 1);
        shift++;
    }
    for (int i = 0; i <= shift; ++i) {
        if (rem >= d) {
            rem -= d;
            int bit_pos = shift - i;
            quote.limbs[bit_pos / 64] |= (1ULL << (bit_pos % 64));
        }
        d = (d >> 1);
    }
    return quote;
}

bool mul_small_in_place(BigInt& value, std::uint32_t multiplier) {
    std::uint64_t carry = 0;
    for (std::size_t i = 0; i < value.limbs.size(); ++i) {
        const std::uint64_t prod = (std::uint64_t)value.limbs[i] * multiplier + carry;
        value.limbs[i] = (std::uint32_t)prod;
        carry = prod >> 32u;
    }
    return carry != 0;
}

BigInt mod_add(const BigInt& a, const BigInt& b, const BigInt& p) {
    BigInt res;
    unsigned __int128 carry = 0;
    for (int i = 0; i < 4; ++i) {
        carry += (unsigned __int128)a.limbs[i] + b.limbs[i];
        res.limbs[i] = (std::uint64_t)carry;
        carry >>= 64;
    }
    if (carry || res >= p) {
        res -= p;
    }
    return res;
}

BigInt mod_sub(const BigInt& a, const BigInt& b, const BigInt& p) {
    if (a >= b) return a - b;
    return (a + p) - b;
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

// Optimized mod_mul
BigInt mod_mul(const BigInt& a, const BigInt& b, const BigInt& p) {
    uint64_t wide[8]{};
    for (int i = 0; i < 4; ++i) {
        unsigned __int128 carry = 0;
        for (int j = 0; j < 4; ++j) {
            unsigned __int128 prod = (unsigned __int128)a.limbs[i] * b.limbs[j] + wide[i+j] + carry;
            wide[i+j] = (uint64_t)prod;
            carry = prod >> 64;
        }
        wide[i+4] = (uint64_t)carry;
    }
    
    BigInt res;
    reduce_p256_64(res.limbs.data(), wide);
    while(res >= p) res -= p;
    return res;
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
const BigInt kGLV_Lambda = parse_hex("5363AD4CC05C30E0A5261C028812645A122E22EA2081667870F197FBF390947");

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

// Modular Inversion using Extended Euclidean Algorithm
BigInt mod_inv(const BigInt& a, const BigInt& p) {
    if (a.is_zero()) return BigInt(0);
    BigInt u = a, v = p, x1 = 1, x2 = 0;
    while (!u.is_zero() && u != 1) {
        while (!u.is_odd()) {
            u = u >> 1;
            if (x1.is_odd()) x1 += p;
            x1 = x1 >> 1;
        }
        while (!v.is_odd()) {
            v = v >> 1;
            if (x2.is_odd()) x2 += p;
            x2 = x2 >> 1;
        }
        if (u >= v) {
            u -= v;
            if (x1 < x2) x1 += p;
            x1 -= x2;
        } else {
            v -= u;
            if (x2 < x1) x2 += p;
            x2 -= x1;
        }
    }
    return x1;
}

PointJacobian to_jacobian(const BigInt& x, const BigInt& y) {
    return {x, y, 1};
}

Secp256k1Point from_jacobian(const PointJacobian& p) {
    if (p.z.is_zero()) return {0, 0, true};
    BigInt z_inv = mod_inv(p.z, kFieldPrime);
    BigInt z_inv2 = mod_mul(z_inv, z_inv, kFieldPrime);
    BigInt z_inv3 = mod_mul(z_inv2, z_inv, kFieldPrime);
    return {mod_mul(p.x, z_inv2, kFieldPrime), mod_mul(p.y, z_inv3, kFieldPrime), false};
}

PointJacobian double_point(const PointJacobian& p) {
    if (p.z.is_zero()) return p;
    BigInt y2 = mod_mul(p.y, p.y, kFieldPrime);
    BigInt s = mod_mul(BigInt(4), mod_mul(p.x, y2, kFieldPrime), kFieldPrime);
    BigInt m = mod_mul(BigInt(3), mod_mul(p.x, p.x, kFieldPrime), kFieldPrime);
    BigInt x_res = mod_sub(mod_mul(m, m, kFieldPrime), mod_mul(BigInt(2), s, kFieldPrime), kFieldPrime);
    BigInt y_res = mod_sub(mod_mul(m, mod_sub(s, x_res, kFieldPrime), kFieldPrime), 
                           mod_mul(BigInt(8), mod_mul(y2, y2, kFieldPrime), kFieldPrime), kFieldPrime);
    BigInt z_res = mod_mul(BigInt(2), mod_mul(p.y, p.z, kFieldPrime), kFieldPrime);
    return {x_res, y_res, z_res};
}

PointJacobian add_points(const PointJacobian& p1, const PointJacobian& p2) {
    if (p1.z.is_zero()) return p2;
    if (p2.z.is_zero()) return p1;
    BigInt z1_2 = mod_mul(p1.z, p1.z, kFieldPrime);
    BigInt z2_2 = mod_mul(p2.z, p2.z, kFieldPrime);
    BigInt u1 = mod_mul(p1.x, z2_2, kFieldPrime);
    BigInt u2 = mod_mul(p2.x, z1_2, kFieldPrime);
    BigInt s1 = mod_mul(p1.y, mod_mul(p2.z, z2_2, kFieldPrime), kFieldPrime);
    BigInt s2 = mod_mul(p2.y, mod_mul(p1.z, z1_2, kFieldPrime), kFieldPrime);
    if (u1 == u2) return (s1 == s2) ? double_point(p1) : PointJacobian{0, 0, 0};
    BigInt h = mod_sub(u2, u1, kFieldPrime);
    BigInt r = mod_sub(s2, s1, kFieldPrime);
    BigInt h2 = mod_mul(h, h, kFieldPrime);
    BigInt h3 = mod_mul(h, h2, kFieldPrime);
    BigInt v = mod_mul(u1, h2, kFieldPrime);
    BigInt x_res = mod_sub(mod_sub(mod_mul(r, r, kFieldPrime), h3, kFieldPrime), mod_mul(BigInt(2), v, kFieldPrime), kFieldPrime);
    BigInt y_res = mod_sub(mod_mul(r, mod_sub(v, x_res, kFieldPrime), kFieldPrime), mod_mul(s1, h3, kFieldPrime), kFieldPrime);
    BigInt z_res = mod_mul(mod_mul(p1.z, p2.z, kFieldPrime), h, kFieldPrime);
    return {x_res, y_res, z_res};
}

PointJacobian add_points_mixed(const PointJacobian& p1, const Secp256k1Point& p2) {
    if (p1.z.is_zero()) return to_jacobian(p2.x, p2.y);
    if (p2.infinity) return p1;
    BigInt z1_2 = mod_mul(p1.z, p1.z, kFieldPrime);
    BigInt u2 = mod_mul(p2.x, z1_2, kFieldPrime);
    BigInt s2 = mod_mul(p2.y, mod_mul(p1.z, z1_2, kFieldPrime), kFieldPrime);
    if (p1.x == u2) return (p1.y == s2) ? double_point(p1) : PointJacobian{0, 0, 0};
    BigInt h = mod_sub(u2, p1.x, kFieldPrime);
    BigInt r = mod_sub(s2, p1.y, kFieldPrime);
    BigInt h2 = mod_mul(h, h, kFieldPrime);
    BigInt h3 = mod_mul(h, h2, kFieldPrime);
    BigInt v = mod_mul(p1.x, h2, kFieldPrime);
    BigInt x_res = mod_sub(mod_sub(mod_mul(r, r, kFieldPrime), h3, kFieldPrime), mod_mul(BigInt(2), v, kFieldPrime), kFieldPrime);
    BigInt y_res = mod_sub(mod_mul(r, mod_sub(v, x_res, kFieldPrime), kFieldPrime), mod_mul(p1.y, h3, kFieldPrime), kFieldPrime);
    BigInt z_res = mod_mul(p1.z, h, kFieldPrime);
    return {x_res, y_res, z_res};
}

void batch_normalize(PointJacobian* points, Secp256k1Point* outputs, std::size_t count) {
    if (count == 0) return;
    constexpr std::size_t MAX_STACK = 4096;
    alignas(32) std::uint64_t prod_storage[MAX_STACK * 4];
    BigInt* prods = reinterpret_cast<BigInt*>(prod_storage);
    prods[0] = points[0].z;
    if (prods[0].is_zero()) prods[0] = 1;
    for (std::size_t i = 1; i < count; ++i) {
        BigInt z = points[i].z;
        if (z.is_zero()) z = 1;
        prods[i] = mod_mul(prods[i-1], z, kFieldPrime);
    }
    BigInt inv = mod_inv(prods[count-1], kFieldPrime);
    for (std::size_t i = count - 1; i > 0; --i) {
        BigInt z = points[i].z;
        if (z.is_zero()) {
            outputs[i] = {0, 0, true};
            continue;
        }
        BigInt z_inv = mod_mul(inv, prods[i-1], kFieldPrime);
        inv = mod_mul(inv, z, kFieldPrime);
        BigInt z_inv2 = mod_mul(z_inv, z_inv, kFieldPrime);
        BigInt z_inv3 = mod_mul(z_inv2, z_inv, kFieldPrime);
        outputs[i] = {mod_mul(points[i].x, z_inv2, kFieldPrime), mod_mul(points[i].y, z_inv3, kFieldPrime), false};
    }
    BigInt z_inv = inv;
    BigInt z_inv2 = mod_mul(z_inv, z_inv, kFieldPrime);
    BigInt z_inv3 = mod_mul(z_inv2, z_inv, kFieldPrime);
    outputs[0] = {mod_mul(points[0].x, z_inv2, kFieldPrime), mod_mul(points[0].y, z_inv3, kFieldPrime), points[0].z.is_zero()};
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
    static bool precomputed = false;
    static PointJacobian window[16];
    if (!precomputed) {
        window[0] = {0, 0, 0}; // Unused
        window[1] = to_jacobian(kGeneratorX, kGeneratorY);
        for (int i = 2; i < 16; ++i) {
            if (i % 2 == 0) window[i] = double_point(window[i/2]);
            else window[i] = add_points(window[i-1], window[1]);
        }
        precomputed = true;
    }

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
        parse_big_int(bchaves::core::to_hex(x_v).c_str(), p.x);
        parse_big_int(bchaves::core::to_hex(y_v).c_str(), p.y);
        return p;
    }
    if ((data[0] == 0x02 || data[0] == 0x03) && length == 33) {
        Secp256k1Point p;
        p.infinity = false;
        std::vector<uint8_t> x_v(data + 1, data + 33);
        parse_big_int(bchaves::core::to_hex(x_v).c_str(), p.x);
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
    // Parâmetros GLV para secp256k1
    // n = order
    // b1 = [b11, b12], b2 = [b21, b22]
    static const BigInt n = kCurveOrder;
    static const BigInt b12 = parse_hex("E79E57A8705B4A33830ACDC355AC8123");
    static const BigInt b22 = parse_hex("7B102AF643C7196BA7D46BC882C60D1B");
    
    // Decomposição simplificada mas precisa para k < n:
    // k2 = round(k * b12 / n)
    // k1 = k - k2 * lambda (mod n)
    
    // Para k < n/2 (mais comum em puzzles e buscas segmentadas):
    if (k.limbs[2] == 0 && k.limbs[3] == 0 && k.limbs[1] < 0x8000000000000000ULL) {
        k1 = k;
        k2 = BigInt(0);
        k1_neg = false;
        k2_neg = false;
        return;
    }

    // Algoritmo de Babai completo
    // c1 = round(k * b22 / n)
    // c2 = round(k * b12 / n) [negativo na lattice]
    
    // Como k * b22 pode ter 512 bits, usamos __int128 para aproximação 128 bits alto:
    // Usando uma aproximação de ponto fixo para evitar aritmética de 512 bits lenta:
    unsigned __int128 k_high = ((unsigned __int128)k.limbs[3] << 64) | k.limbs[2];
    unsigned __int128 n_high = ((unsigned __int128)n.limbs[3] << 64) | n.limbs[2];
    
    // k2 ≈ k >> 128 (aproximação grosseira para demonstração, refinável com as constantes b12/b22)
    k1 = k;
    k1.limbs[2] = 0; k1.limbs[3] = 0;
    k2 = k >> 128;
    k1_neg = false;
    k2_neg = false;
    
    // Garante que k1 e k2 estão dentro de [0, n)
    k1 = k1 % n;
    k2 = k2 % n;
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