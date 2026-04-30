# Requirements

## 🎯 Goal
Stabilize the Bchaves performance engine by resolving cryptographic arithmetic errors and ensuring robust target loading across all modules.

## 📝 Functional Requirements

### 1. Cryptographic Correctness
- **[MUST]** Fix `reduce_p256_64` to correctly handle carries and modular reduction.
- **[MUST]** Ensure `mod_pow_k1` and square root calculations (Tonelli-Shanks) are accurate for Secp256k1.
- **[MUST]** Pass `crypto_test` validation suite.

### 2. Engine Stability
- **[MUST]** Fix point deserialization failure in `kangaroo` mode.
- **[MUST]** Ensure public keys with only digits are correctly identified and parsed (fix `parse_big_int`).
- **[SHOULD]** Optimize `deserialize_pubkey` to avoid expensive string conversions.

### 3. Checkpoint Integrity
- **[MUST]** Validate that checkpoints can be resumed without corruption after math fixes.

## 🛠️ Technical Requirements
- **Performance**: Maintain high throughput after arithmetic fixes.
- **Portability**: Ensure fixes work on both x86_64 (AVX2) and ARM64.
