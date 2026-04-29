/*
 * Bchaves: Bitcoin Performance Engine
 * 
 * Descrição: Suite mínima de regressão para corretude criptográfica.
 *            Usa asserts reais contra vetores de teste conhecidos.
 * 
 * Repository: https://github.com/carlosatec/Bchaves
 * Author:     Carlos
 * License:    MIT (c) 2026
 */
#include "core/secp256k1.hpp"
#include "core/address.hpp"
#include "core/hash.hpp"
#include "system/checkpoint.hpp"
#include "system/cli.hpp"

#include <cassert>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using namespace bchaves::core;

static int g_pass = 0;
static int g_fail = 0;

#define EXPECT(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "[FAIL] " << msg << "\n"; \
        ++g_fail; \
    } else { \
        ++g_pass; \
    } \
} while (0)

static std::vector<char*> make_argv(std::vector<std::string>& args) {
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (std::string& arg : args) {
        argv.push_back(arg.data());
    }
    return argv;
}

// ============================================================
// SHA-256: vetores de teste NIST
// ============================================================
static void test_sha256() {
    std::cout << "[*] test_sha256\n";

    // Vetor 1: SHA-256("") = e3b0c44298fc1c149afbf4c8996fb924...
    {
        auto h = sha256(nullptr, 0);
        const char* expected = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
        EXPECT(to_hex(std::vector<uint8_t>(h.begin(), h.end())) == expected,
               "SHA-256 empty string");
    }

    // Vetor 2: SHA-256("abc") = ba7816bf8f01cfea414140de5dae2223...
    {
        const uint8_t msg[] = {'a', 'b', 'c'};
        auto h = sha256(msg, 3);
        const char* expected = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
        EXPECT(to_hex(std::vector<uint8_t>(h.begin(), h.end())) == expected,
               "SHA-256 'abc'");
    }
}

// ============================================================
// RIPEMD-160: vetores de teste padrão
// ============================================================
static void test_ripemd160() {
    std::cout << "[*] test_ripemd160\n";

    // RIPEMD-160("abc") = 8eb208f7e05d987a9b044a8e98c6b087f15a0bfc
    {
        const uint8_t msg[] = {'a', 'b', 'c'};
        auto h = ripemd160(msg, 3);
        const char* expected = "8eb208f7e05d987a9b044a8e98c6b087f15a0bfc";
        EXPECT(to_hex(std::vector<uint8_t>(h.begin(), h.end())) == expected,
               "RIPEMD-160 'abc'");
    }
}

// ============================================================
// Hash160: SHA-256 + RIPEMD-160 de pubkey comprimida conhecida
// ============================================================
static void test_hash160_pubkey() {
    std::cout << "[*] test_hash160_pubkey\n";

    // Chave privada = 1
    // Pubkey comprimida: 0279BE667EF9DCBBAC55A06295CE870B07029BFCDB2DCE28D959F2815B16F81798
    // Hash160:          751e76e8199196d454941c45d1b3a323f1433bd6
    BigInt priv(1);
    Secp256k1Point pub = secp256k1_multiply(priv);
    EXPECT(!pub.infinity, "multiply(1) not infinity");

    uint8_t serialized[33];
    serialize_pubkey(pub, true, serialized);

    auto sha = sha256(serialized, 33);
    auto rmd = ripemd160(sha.data(), sha.size());
    const char* expected = "751e76e8199196d454941c45d1b3a323f1433bd6";
    EXPECT(to_hex(std::vector<uint8_t>(rmd.begin(), rmd.end())) == expected,
           "Hash160 of G (privkey=1)");
}

// ============================================================
// Scalar Multiplication: vetores conhecidos de secp256k1
// ============================================================
static void test_scalar_multiply() {
    std::cout << "[*] test_scalar_multiply\n";

    // G (privkey=1)
    {
        BigInt priv(1);
        Secp256k1Point pub = secp256k1_multiply(priv);
        EXPECT(!pub.infinity, "G not infinity");
        const char* gx = "79be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798";
        EXPECT(bigint_to_hex(pub.x) == gx, "G.x matches");
    }

    // 2*G
    {
        BigInt priv(2);
        Secp256k1Point pub = secp256k1_multiply(priv);
        EXPECT(!pub.infinity, "2G not infinity");
        const char* expected_x = "c6047f9441ed7d6d3045406e95c07cd85c778e4b8cef3ca7abac09b95c709ee5";
        EXPECT(bigint_to_hex(pub.x) == expected_x, "2G.x matches");
    }

    // 7*G
    {
        BigInt priv(7);
        Secp256k1Point pub = secp256k1_multiply(priv);
        EXPECT(!pub.infinity, "7G not infinity");
        const char* expected_x = "5cbdf0646e5db4eaa398f365f2ea7a0e3d419b7e0330e39ce92bddedcac4f9bc";
        EXPECT(bigint_to_hex(pub.x) == expected_x, "7G.x matches");
    }
}

// ============================================================
// Pubkey Serialization / Deserialization round-trip
// ============================================================
static void test_pubkey_serde() {
    std::cout << "[*] test_pubkey_serde\n";

    BigInt priv(42);
    Secp256k1Point original = secp256k1_multiply(priv);

    // Compressed round-trip
    {
        uint8_t buf[33];
        serialize_pubkey(original, true, buf);
        EXPECT(buf[0] == 0x02 || buf[0] == 0x03, "compressed prefix valid");
        Secp256k1Point recovered = deserialize_pubkey(buf, 33);
        EXPECT(!recovered.infinity, "compressed deserialized not infinity");
        EXPECT(recovered.x == original.x, "compressed round-trip x matches");
        EXPECT(recovered.y == original.y, "compressed round-trip y matches");
    }

    // Uncompressed round-trip
    {
        uint8_t buf[65];
        serialize_pubkey(original, false, buf);
        EXPECT(buf[0] == 0x04, "uncompressed prefix is 0x04");
        Secp256k1Point recovered = deserialize_pubkey(buf, 65);
        EXPECT(!recovered.infinity, "uncompressed deserialized not infinity");
        EXPECT(recovered.x == original.x, "uncompressed round-trip x matches");
        EXPECT(recovered.y == original.y, "uncompressed round-trip y matches");
    }
}

// ============================================================
// WIF: Base58Check com versão 0x80 e checksum
// ============================================================
static void test_wif() {
    std::cout << "[*] test_wif\n";

    // Privkey = 1
    // WIF compressed: KwDiBf89QgGbjEhKnhXJuH7LrciVrZi3qYjgd9M7rFU73sVHnoWn
    // WIF uncompressed: 5HpHagT65TZzG1PH3CSu63k8DbpvD8s5ip4nEB3kEsreAnchuDf
    BigInt priv(1);
    DerivedKeyInfo info;
    bool ok = derive_key_info(priv, info);
    EXPECT(ok, "derive_key_info(1) succeeds");
    EXPECT(info.wif_compressed == "KwDiBf89QgGbjEhKnhXJuH7LrciVrZi3qYjgd9M7rFU73sVHnoWn",
           "WIF compressed for privkey 1");
    EXPECT(info.wif_uncompressed == "5HpHagT65TZzG1PH3CSu63k8DbpvD8s5ip4nEB3kEsreAnchuDf",
           "WIF uncompressed for privkey 1");
}

// ============================================================
// Address: P2PKH do privkey 1
// ============================================================
static void test_address() {
    std::cout << "[*] test_address\n";

    // Privkey = 1 → compressed address = 1BgGZ9tcN4rm9KBzDn7KprQz87SZ26SAMH
    BigInt priv(1);
    DerivedKeyInfo info;
    bool ok = derive_key_info(priv, info);
    EXPECT(ok, "derive_key_info(1) for address");
    EXPECT(info.address_compressed == "1BgGZ9tcN4rm9KBzDn7KprQz87SZ26SAMH",
           "P2PKH compressed address for privkey 1");
}

// ============================================================
// BigInt Arithmetic: sanidade básica
// ============================================================
static void test_bigint_ops() {
    std::cout << "[*] test_bigint_ops\n";

    BigInt a(0xFFFFFFFFFFFFFFFFULL);
    BigInt b(1);
    BigInt c = a + b;
    EXPECT(c.limbs[0] == 0 && c.limbs[1] == 1, "BigInt carry propagation");

    BigInt d(100);
    BigInt e(42);
    BigInt f = d - e;
    EXPECT(f.limbs[0] == 58, "BigInt subtraction");

    BigInt g(1000);
    BigInt h(1000);
    EXPECT(g == h, "BigInt equality");
    EXPECT(!(g < h), "BigInt not less-than equal");

    BigInt i(999);
    EXPECT(i < g, "BigInt less-than");
}

// ============================================================
// GLV Decomposition: k1 + lambda*k2 ≡ k (mod n)
// ============================================================
static void test_glv_decomposition() {
    std::cout << "[*] test_glv_decomposition\n";

    // Teste: multiplicar via GLV deve dar o mesmo resultado que multiplicar via escalar direto
    BigInt priv(123456789);
    Secp256k1Point p_direct = secp256k1_multiply(priv);
    Secp256k1Point p_glv = secp256k1_multiply_glv(priv);

    EXPECT(!p_direct.infinity, "direct multiply not infinity");
    EXPECT(!p_glv.infinity, "glv multiply not infinity");
    EXPECT(p_direct.x == p_glv.x, "GLV x matches direct x");
    EXPECT(p_direct.y == p_glv.y, "GLV y matches direct y");

    // Teste com escalar grande
    BigInt big;
    parse_big_int("DEADBEEFCAFEBABE1234567890ABCDEF", big);
    Secp256k1Point p2_direct = secp256k1_multiply(big);
    Secp256k1Point p2_glv = secp256k1_multiply_glv(big);
    EXPECT(p2_direct.x == p2_glv.x, "GLV large scalar x matches");
    EXPECT(p2_direct.y == p2_glv.y, "GLV large scalar y matches");
}

// ============================================================
// mul_small_in_place: validação contra operador *
// ============================================================
static void test_mul_small() {
    std::cout << "[*] test_mul_small\n";

    BigInt a;
    a.limbs[0] = 0xFFFFFFFFFFFFFFFFULL;
    a.limbs[1] = 0x1234;
    BigInt expected = a * BigInt(7);
    BigInt actual = a;
    mul_small_in_place(actual, 7);
    EXPECT(actual == expected, "mul_small_in_place matches operator*");
}

// ============================================================
// to_bytes32 / round-trip
// ============================================================
static void test_bytes32_roundtrip() {
    std::cout << "[*] test_bytes32_roundtrip\n";

    BigInt original;
    original.limbs[0] = 0xDEADBEEFCAFEBABEULL;
    original.limbs[1] = 0x1234567890ABCDEFULL;
    original.limbs[2] = 0xFEDCBA0987654321ULL;
    original.limbs[3] = 0x0011223344556677ULL;

    auto bytes = to_bytes32(original);

    // Reconstruct: to_bytes32 produces big-endian
    BigInt reconstructed;
    for (int i = 0; i < 4; ++i) {
        uint64_t limb = 0;
        for (int j = 0; j < 8; ++j) {
            limb |= static_cast<uint64_t>(bytes[31 - (i * 8 + j)]) << (j * 8);
        }
        reconstructed.limbs[i] = limb;
    }
    EXPECT(reconstructed == original, "to_bytes32 round-trip");
}

// ============================================================
// batch_normalize: normalização de pontos Jacobian
// ============================================================
static void test_batch_normalize() {
    std::cout << "[*] test_batch_normalize\n";

    constexpr size_t N = 8;
    PointJacobian jac[N];
    Secp256k1Point affine[N];

    BigInt priv(1);
    Secp256k1Point g = secp256k1_multiply(priv);
    PointJacobian cur = to_jacobian(g.x, g.y);

    for (size_t i = 0; i < N; ++i) {
        jac[i] = cur;
        cur = add_points_mixed(cur, g);
    }

    batch_normalize(jac, affine, N);

    // Verify each affine point matches direct computation
    for (size_t i = 0; i < N; ++i) {
        BigInt k(static_cast<uint64_t>(i + 1));
        Secp256k1Point expected = secp256k1_multiply(k);
        EXPECT(affine[i].x == expected.x, "batch_normalize x[" + std::to_string(i) + "]");
        EXPECT(affine[i].y == expected.y, "batch_normalize y[" + std::to_string(i) + "]");
    }
}

// ============================================================
// CLI: contratos básicos dos motores
// ============================================================
static void test_cli_contracts() {
    std::cout << "[*] test_cli_contracts\n";

    {
        bchaves::system::AddressOptions options;
        std::string error;
        std::vector<std::string> args = {"address", "targets.txt", "-b", "40", "-R", "backward"};
        auto argv = make_argv(args);
        EXPECT(!bchaves::system::parse_address_cli(static_cast<int>(argv.size()), argv.data(), options, error),
               "address rejects backward mode");
        EXPECT(error.find("backward/both") != std::string::npos,
               "address reports unsupported backward/both");
    }

    {
        bchaves::system::BsgsOptions options;
        std::string error;
        std::vector<std::string> args = {"bsgs", "pubkey.txt", "-b", "40", "-k", "2048"};
        auto argv = make_argv(args);
        EXPECT(bchaves::system::parse_bsgs_cli(static_cast<int>(argv.size()), argv.data(), options, error),
               "bsgs accepts explicit -k");
        EXPECT(options.table_k == 2048, "bsgs stores parsed -k");
    }

    {
        bchaves::system::KangarooOptions options;
        std::string error;
        std::vector<std::string> args = {"kangaroo", "target.txt", "-b", "0"};
        auto argv = make_argv(args);
        EXPECT(!bchaves::system::parse_kangaroo_cli(static_cast<int>(argv.size()), argv.data(), options, error),
               "kangaroo rejects bit range 0");
        EXPECT(error.find("1 e 256") != std::string::npos,
               "kangaroo reports bit range validation");
    }

    {
        bchaves::system::KangarooOptions options;
        std::string error;
        std::vector<std::string> args = {"kangaroo", "target.txt", "-b", "75", "--trap-dir", "traps-test", "-c", "kangaroo.ckp"};
        auto argv = make_argv(args);
        EXPECT(bchaves::system::parse_kangaroo_cli(static_cast<int>(argv.size()), argv.data(), options, error),
               "kangaroo accepts trap dir and checkpoint path");
        EXPECT(options.trap_dir.has_value() && options.trap_dir->string() == "traps-test",
               "kangaroo stores trap_dir");
        EXPECT(options.checkpoint_path.has_value() && options.checkpoint_path->string() == "kangaroo.ckp",
               "kangaroo stores checkpoint path");
    }
}

// ============================================================
// Checkpoint: round-trip do formato v6
// ============================================================
static void test_checkpoint_roundtrip() {
    std::cout << "[*] test_checkpoint_roundtrip\n";

    bchaves::system::CheckpointState state;
    state.algorithm = "kangaroo";
    state.threads = 2;
    state.batch_size = 64;
    state.progress_primary = 123456;
    state.progress_secondary = 75;
    state.range_start = to_bytes32(BigInt(11));
    state.range_end = to_bytes32(BigInt(99));
    state.worker_currents.push_back(to_bytes32(BigInt(1)));
    state.worker_currents.push_back(to_bytes32(BigInt(2)));
    state.worker_currents.push_back(to_bytes32(BigInt(3)));
    state.timestamp = 42;

    const std::filesystem::path temp = std::filesystem::temp_directory_path() / "bchaves_checkpoint_test.ckp";
    std::string error;
    EXPECT(bchaves::system::save_checkpoint(temp, state, error), "save checkpoint round-trip file");

    bchaves::system::CheckpointState loaded;
    error.clear();
    EXPECT(bchaves::system::load_checkpoint(temp, loaded, error), "load checkpoint round-trip file");
    EXPECT(loaded.algorithm == state.algorithm, "checkpoint algorithm round-trip");
    EXPECT(loaded.threads == state.threads, "checkpoint threads round-trip");
    EXPECT(loaded.batch_size == state.batch_size, "checkpoint batch round-trip");
    EXPECT(loaded.progress_primary == state.progress_primary, "checkpoint progress round-trip");
    EXPECT(loaded.progress_secondary == state.progress_secondary, "checkpoint secondary round-trip");
    EXPECT(loaded.worker_currents == state.worker_currents, "checkpoint worker payload round-trip");

    std::error_code ec;
    std::filesystem::remove(temp, ec);
}

// ============================================================
// Main
// ============================================================
int main() {
    std::cout << "=== Bchaves Crypto Regression Suite ===\n\n";

    test_sha256();
    test_ripemd160();
    test_hash160_pubkey();
    test_scalar_multiply();
    test_pubkey_serde();
    test_wif();
    test_address();
    test_bigint_ops();
    test_glv_decomposition();
    test_mul_small();
    test_bytes32_roundtrip();
    test_batch_normalize();
    test_cli_contracts();
    test_checkpoint_roundtrip();

    std::cout << "\n=== Results: " << g_pass << " passed, " << g_fail << " failed ===\n";
    if (g_fail > 0) {
        std::cerr << "[!] SOME TESTS FAILED\n";
        return 1;
    }
    std::cout << "[+] ALL TESTS PASSED\n";
    return 0;
}
