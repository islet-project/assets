#include "measurement/measurement.h"
#include "kvm/sha2.h"
#include <stdio.h>

static void measurement_print(unsigned char *measurement,
			      const enum hash_algo algorithm)
{
	unsigned int size = 0U;
	assert(measurement != NULL);

	printf("Measurement ");

	switch (algorithm) {
	case HASH_ALGO_SHA256:
		printf("(SHA256): 0x");
		size = SHA256_SIZE;
		break;
	case HASH_ALGO_SHA512:
		printf("(SHA512): 0x");
		size = SHA512_SIZE;
		break;
	default:
		/* Prevent static check and MISRA warnings */
		assert(false);
	}

	for (unsigned int i = 0U; i < size; ++i) {
		printf("%02x", *(measurement+i));
	}
	printf("\n");
}

void measurement_hash_compute(enum hash_algo hash_algo,
			      void *data,
			      size_t size,
			      unsigned char *out)
{
	assert(size <= GRANULE_SIZE);
	assert((data != NULL) && (out != NULL));

	switch(hash_algo) {
		case HASH_ALGO_SHA256:
			sha256((unsigned char*)data, size, out);
			break;
		case HASH_ALGO_SHA512:
			sha512((unsigned char*)data, size, out);
			break;
		default:
			assert(false);
	}
}

void measurement_extend(enum hash_algo hash_algo,
			void *current_measurement,
			void *extend_measurement,
			size_t extend_measurement_size,
			unsigned char *out)
{
	sha256_ctx sha256ctx;
	sha512_ctx sha512ctx;

	assert(current_measurement != NULL);
	assert(extend_measurement_size <= GRANULE_SIZE);
	assert(extend_measurement != NULL);
	assert(out != NULL);

	switch (hash_algo) {
	case HASH_ALGO_SHA256:
		sha256_init(&sha256ctx);
		sha256_update(&sha256ctx, current_measurement, SHA256_DIGEST_SIZE);
		sha256_update(&sha256ctx, extend_measurement, extend_measurement_size);
		sha256_final(&sha256ctx, out);
		break;
	case HASH_ALGO_SHA512:
		sha512_init(&sha512ctx);
		sha512_update(&sha512ctx, current_measurement, SHA512_DIGEST_SIZE);
		sha512_update(&sha512ctx, extend_measurement, extend_measurement_size);
		sha512_final(&sha512ctx, out);
		break;
	default:
		assert(false);
	}

	measurement_print(out, hash_algo);
}

