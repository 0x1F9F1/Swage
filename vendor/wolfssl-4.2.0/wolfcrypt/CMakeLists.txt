add_library(wolfcrypt STATIC
    src/aes.c src/arc4.c src/asn.c src/blake2b.c
    src/camellia.c src/chacha.c src/coding.c src/compress.c src/des3.c
    src/dh.c src/dsa.c src/ecc.c src/error.c src/hc128.c src/hmac.c
    src/integer.c src/logging.c src/md2.c src/md4.c src/md5.c src/memory.c
    src/pkcs7.c src/pkcs12.c src/poly1305.c src/pwdbased.c src/rabbit.c
    src/random.c src/ripemd.c src/rsa.c src/sha.c src/sha256.c src/sha512.c
    src/tfm.c src/wc_port.c src/wc_encrypt.c src/hash.c src/wolfmath.c
    ../wolfssl/wolfcrypt/aes.h ../wolfssl/wolfcrypt/arc4.h ../wolfssl/wolfcrypt/asn.h ../wolfssl/wolfcrypt/blake2.h
    ../wolfssl/wolfcrypt/camellia.h ../wolfssl/wolfcrypt/chacha.h ../wolfssl/wolfcrypt/coding.h ../wolfssl/wolfcrypt/compress.h ../wolfssl/wolfcrypt/des3.h
    ../wolfssl/wolfcrypt/dh.h ../wolfssl/wolfcrypt/dsa.h ../wolfssl/wolfcrypt/ecc.h ../wolfssl/wolfcrypt/error-crypt.h ../wolfssl/wolfcrypt/hc128.h ../wolfssl/wolfcrypt/hmac.h
    ../wolfssl/wolfcrypt/integer.h ../wolfssl/wolfcrypt/logging.h ../wolfssl/wolfcrypt/md2.h ../wolfssl/wolfcrypt/md4.h ../wolfssl/wolfcrypt/md5.h ../wolfssl/wolfcrypt/memory.h
    ../wolfssl/wolfcrypt/misc.h ../wolfssl/wolfcrypt/pkcs7.h ../wolfssl/wolfcrypt/poly1305.h ../wolfssl/wolfcrypt/pwdbased.h ../wolfssl/wolfcrypt/rabbit.h
    ../wolfssl/wolfcrypt/random.h ../wolfssl/wolfcrypt/ripemd.h ../wolfssl/wolfcrypt/rsa.h ../wolfssl/wolfcrypt/sha.h ../wolfssl/wolfcrypt/sha256.h ../wolfssl/wolfcrypt/sha512.h
    ../wolfssl/wolfcrypt/tfm.h ../wolfssl/wolfcrypt/wc_port.h ../wolfssl/wolfcrypt/wc_encrypt.h
    ../wolfssl/wolfcrypt/hash.h)

target_include_directories(wolfcrypt PUBLIC ${CMAKE_CURRENT_LIST_DIR}/..)
