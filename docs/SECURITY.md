# Security Notes

## Password storage
- Algorithm: **scrypt** from OpenSSL 3 (`EVP_PBE_scrypt`), N=32768 (2^15), r=8, p=1, 32-byte key.
- Salt: 16 random bytes from `RAND_bytes`, unique per user, stored as hex.
- Stored value: `scrypt$N$r$p$hexkey`, so the cost can be raised later and old hashes still verify.
- Comparison with `CRYPTO_memcmp` (constant time). Parameters read back from the database are range-checked so a
  corrupted record cannot force huge memory use.
- **Why scrypt/OpenSSL:** a recognised, audited library (no home-made crypto); memory-hard, so GPU cracking is
  costly, unlike plain SHA-256; OpenSSL is available on nearly every system, avoiding an extra dependency.
  Argon2id (libsodium) is the modern alternative but was not available in the build environment.
- No plain-text password is stored, logged or printed. Console input is not echoed on Unix terminals.
- Policy: 8-128 characters, at least one letter and one digit, not equal to the username.
- Login failure message is the same for unknown user and wrong password; five failed attempts exit the program.

## Access control
Each User subclass answers `can(Permission)`; every service method checks the permission, and then checks the
relationship (lecturer teaches the course, HOD owns the department, student is enrolled). Students see marks only
after the HOD releases them.

## SQL injection and data integrity
Prepared statements with bound parameters only. Foreign keys and triggers back up the application rules.
The audit log cannot be updated or deleted, even through SQL.

## Known limitations
- No account lockout across restarts or password reset flow (admin must recreate the account).
- The database file is not encrypted; protect it with file-system permissions.
- Test clock option must never be enabled in release builds.
