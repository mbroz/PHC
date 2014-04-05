#include <stddef.h>
#include <stdint.h>

int yarn(void *out, size_t outlen, const void *in, size_t inlen, const void *salt, size_t saltlen, unsigned int t_cost, unsigned int m_cost, unsigned int par, unsigned int initrnd, unsigned int m_step, const void *pers, size_t perslen);
void blake2b(void *out, size_t outlen, const void *in, size_t inlen, const void *salt, size_t saltlen, const void *pers, size_t perslen);
void blake2b_expand(uint64_t state[8], void *exp, size_t explen, size_t outlen, const void *in, size_t inlen, const void *salt, size_t saltlen, const void *pers, size_t perslen);

static inline int yarn_with_some_default_values(void *out, size_t outlen, const void *in, size_t inlen, const void *salt, size_t saltlen, unsigned int t_cost, unsigned int m_cost) {
	return yarn(out, outlen, in, inlen, salt, saltlen, t_cost, m_cost, 6, 10, 72, NULL, 0);
}

int yarn_with_some_default_values(void *out, size_t outlen, const void *in, size_t inlen,
    const void *salt, size_t saltlen, unsigned int t_cost, unsigned int m_cost);
