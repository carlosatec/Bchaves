#include "core/address.hpp"
#include "core/secp256k1.hpp"
#include "system/checkpoint.hpp"
#include "system/targets.hpp"

#include <filesystem>
#include <iostream>
#include <string>

namespace {

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << '\n';
        return false;
    }
    return true;
}

std::filesystem::path source_root() {
#ifdef QCHAVES_SOURCE_DIR
    return std::filesystem::path(QCHAVES_SOURCE_DIR);
#else
    return std::filesystem::current_path();
#endif
}

}  // namespace

int main() {
    bool ok = true;

    qchaves::core::BigInt one;
    ok &= expect(qchaves::core::parse_big_int("1", one), "parse private key 1");
    ok &= expect(qchaves::core::is_valid_private_key(one), "private key 1 should be valid");

    qchaves::core::DerivedKeyInfo key_info;
    ok &= expect(qchaves::core::derive_key_info(one, key_info), "derive key info from private key 1");
    ok &= expect(key_info.pubkey_compressed_hex == "0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798",
                 "compressed pubkey for private key 1");
    ok &= expect(key_info.pubkey_uncompressed_hex ==
                     "0479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8",
                 "uncompressed pubkey for private key 1");
    ok &= expect(key_info.address_compressed == "1BgGZ9tcN4rm9KBzDn7KprQz87SZ26SAMH",
                 "compressed address for private key 1");
    ok &= expect(key_info.address_uncompressed == "1EHNa6Q4Jz2uvNExL497mE43ikXhwF6kZm",
                 "uncompressed address for private key 1");
    ok &= expect(key_info.wif_compressed == "KwDiBf89QgGbjEhKnhXJuH7LrciVrZi3qYjgd9M7rFU73sVHnoWn",
                 "compressed WIF for private key 1");
    ok &= expect(key_info.wif_uncompressed == "5HpHagT65TZzG1PH3CSu63k8DbpvD8s5ip4nEB3kEsreAnchuDf",
                 "uncompressed WIF for private key 1");

    const auto root = source_root();
    const auto puzzle1 = qchaves::system::load_targets(root / "Puzzles" / "1.txt", true);
    ok &= expect(puzzle1.entries.size() == 1, "Puzzles/1.txt should load one target");
    ok &= expect(puzzle1.entries.front().type == qchaves::system::TargetType::pubkey_compress,
                 "Puzzles/1.txt target type");

    const auto puzzle71 = qchaves::system::load_targets(root / "Puzzles" / "71.txt", false);
    ok &= expect(puzzle71.entries.size() == 1, "Puzzles/71.txt should load one target");
    ok &= expect(puzzle71.entries.front().type == qchaves::system::TargetType::address_btc,
                 "Puzzles/71.txt target type");

    const auto inline_target = qchaves::system::load_target_inline(
        "02145d2611c823a396ef6712ce0f712f09b9b4f3135e3e0aa3230fb9b6d08d1e16", true);
    ok &= expect(inline_target.entries.size() == 1, "inline pubkey target should load");
    ok &= expect(inline_target.entries.front().type == qchaves::system::TargetType::pubkey_compress,
                 "inline pubkey target type");

    qchaves::system::CheckpointState checkpoint;
    checkpoint.algorithm = "address";
    checkpoint.threads = 1;
    checkpoint.batch_size = 64;
    checkpoint.timestamp = 12345;
    const auto checkpoint_file = root / "build" / "test-checkpoint.ckp";
    std::filesystem::create_directories(checkpoint_file.parent_path());
    std::string checkpoint_error;
    ok &= expect(qchaves::system::save_checkpoint(checkpoint_file, checkpoint, checkpoint_error),
                 "save checkpoint");
    qchaves::system::CheckpointState loaded;
    ok &= expect(qchaves::system::load_checkpoint(checkpoint_file, loaded, checkpoint_error),
                 "load checkpoint");
    ok &= expect(loaded.algorithm == "address", "checkpoint algorithm roundtrip");

    if (!ok) {
        return 1;
    }

    std::cout << "[OK] qchaves tests passed" << '\n';
    return 0;
}
