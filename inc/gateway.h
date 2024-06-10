#ifndef _GATEWAY_H_
#define _GATEWAY_H_

// types
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE (4096)

/* RSI SMC FID */
#define RSI_VERSION                           (0xC4000190)
#define RSI_REALM_CONFIG                      (0xC4000196)
#define RSI_MEASUREMENT_READ                  (0xC4000192)
#define RSI_MEASUREMENT_EXTEND                (0xC4000193)
#define RSI_IPA_STATE_GET                     (0xC4000198)
#define RSI_IPA_STATE_SET                     (0xC4000197)
#define RSI_HOST_CALL                         (0xC4000199)
#define RSI_ATTESTATION_TOKEN_INIT            (0xC4000194)
#define RSI_ATTESTATION_TOKEN_CONTINUE        (0xC4000195)
#define RSI_FEATURES                          (0xC4000191)

/* Cloak */
#define RSI_CHANNEL_CREATE   (0xC4000200)
#define RSI_CLOAK_PRINT_CALL (0xC4000204)

#endif