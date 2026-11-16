#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    static const unsigned char expected[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad
    };
    unsigned char digest[EVP_MAX_MD_SIZE], entropy[32];
    unsigned int size = 0;
    SSL_CTX *ctx;
    if (RAND_bytes(entropy, sizeof(entropy)) != 1) {
        fputs("OS entropy initialization failed.\n", stderr);
        return 1;
    }
    if (EVP_Digest("abc", 3, digest, &size, EVP_sha256(), NULL) != 1 ||
        size != sizeof(expected) || memcmp(digest, expected, size) != 0) {
        fputs("SHA-256 known-answer test failed.\n", stderr);
        return 2;
    }
    ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx || SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION) != 1) {
        fputs("TLS client context initialization failed.\n", stderr);
        SSL_CTX_free(ctx);
        return 3;
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
    SSL_CTX_free(ctx);
    printf("%s: entropy, SHA-256 and TLS context OK\n", OpenSSL_version(OPENSSL_VERSION));
    return 0;
}
