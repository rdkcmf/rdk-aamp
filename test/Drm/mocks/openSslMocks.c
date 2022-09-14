#include <openssl/evp.h>

EVP_CIPHER_CTX *EVP_CIPHER_CTX_new(void)
{
	return NULL;
}

void EVP_CIPHER_CTX_free(EVP_CIPHER_CTX *ctx)
{
}

#if OPENSSL_API_COMPAT < 0x10100000L
int EVP_CIPHER_CTX_reset(EVP_CIPHER_CTX *ctx)
{
	return 0;
}
#else
void EVP_CIPHER_CTX_init(EVP_CIPHER_CTX *a)
{
}

int EVP_CIPHER_CTX_cleanup(EVP_CIPHER_CTX *a)
{
	return 0;
}
#endif

const EVP_CIPHER *EVP_aes_128_ctr(void)
{
	return NULL;
}

int EVP_DecryptInit(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
		const unsigned char *key, const unsigned char *iv)
{
	return 0;
}

int EVP_DecryptInit_ex(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
		ENGINE *impl, const unsigned char *key,
		const unsigned char *iv)
{
	return 0;
}

int EVP_DecryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outl,
		const unsigned char *in, int inl)
{
	return 0;
}

int EVP_DecryptFinal_ex(EVP_CIPHER_CTX *ctx, unsigned char *outm, int *outl)
{
	return 0;
}
