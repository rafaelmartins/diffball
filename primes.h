#ifndef _HEADER_PRIMES
#define _HEADER_PRIMES 1

struct prime_ctx {
	unsigned int *base_primes;
	unsigned long prime_count;
	unsigned long array_size;
};

void init_primes(struct prime_ctx *ctx);
void free_primes(struct prime_ctx *ctx);
unsigned long get_nearest_prime(struct prime_ctx *ctx, unsigned long near);
#endif
