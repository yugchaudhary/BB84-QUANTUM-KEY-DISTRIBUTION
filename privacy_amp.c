/**
 * privacy_amp.c — Privacy Amplification Implementation
 * ======================================================
 * Engineering Physics Project — BB84 QKD Simulator
 *
 * Implements Toeplitz-matrix universal hashing to shrink the sifted key
 * into a shorter final key that Eve has negligible knowledge of.
 */

#include "privacy_amp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* -------------------------------------------------------------------------
 * binary_entropy()
 *
 * h₂(p) = -p·log₂(p) - (1-p)·log₂(1-p)
 *
 * Special cases:
 *   h₂(0) = 0  (certain outcome → zero uncertainty)
 *   h₂(1) = 0  (certain outcome → zero uncertainty)
 *   h₂(0.5) = 1  (maximum uncertainty)
 * ------------------------------------------------------------------------- */
double binary_entropy(double p)
{
    /* Guard against log(0). */
    if (p <= 0.0 || p >= 1.0) return 0.0;
    return -p * log2(p) - (1.0 - p) * log2(1.0 - p);
}

/* -------------------------------------------------------------------------
 * toeplitz_hash()  [internal]
 *
 * Multiplies an (m × n) random Toeplitz matrix by the n-bit input vector
 * over GF(2) (binary field: addition = XOR, multiplication = AND).
 *
 * A Toeplitz matrix is specified by its first row (n bits) and first column
 * (m bits), sharing the top-left element.  Total random material needed:
 *   n + m - 1  bits   (much cheaper than n×m for a fully random matrix).
 *
 * For position (i, j) in the Toeplitz matrix T:
 *   T[i][j] = r[j - i + (m - 1)]    where r is the (n + m - 1)-bit seed.
 *
 * The output bit k is:
 *   out[k] = XOR_{j=0}^{n-1} ( T[k][j] AND in[j] )
 *
 * @param out     Output buffer of m bits.
 * @param in      Input sifted key of n bits.
 * @param n       Length of the input (sifted key length).
 * @param m       Length of the output (final key length).
 * @param seed    Random seed vector of length (n + m - 1).
 * ------------------------------------------------------------------------- */
static void toeplitz_hash(uint8_t       *out,
                          const uint8_t *in,
                          int            n,
                          int            m,
                          const uint8_t *seed)
{
    for (int k = 0; k < m; k++) {    /* for each output bit k */
        uint8_t accumulator = 0;
        for (int j = 0; j < n; j++) {
            /* Index into the Toeplitz seed vector: position (j - k + m - 1) */
            int seed_idx = j - k + (m - 1);
            /* GF(2): accumulate XOR of (T[k][j] AND input[j]) */
            accumulator ^= (seed[seed_idx] & in[j]);
        }
        out[k] = accumulator;
    }
}

/* -------------------------------------------------------------------------
 * privacy_amplify()
 * ------------------------------------------------------------------------- */
AmplifiedKey privacy_amplify(const SiftedKey *sifted,
                              double           qber,
                              int              sec_param)
{
    AmplifiedKey ak;
    memset(&ak, 0, sizeof(AmplifiedKey));

    int n = sifted->length;
    ak.sifted_length = n;

    /* ------------------------------------------------------------------ *
     * Step 1 — Estimate Eve's information using the binary entropy bound. *
     *                                                                     *
     * For the intercept-resend attack, Shannon's channel coding theorem   *
     * gives an upper bound on Eve's mutual information I(K;E):            *
     *                                                                     *
     *   t = n × h₂(QBER)   bits                                          *
     *                                                                     *
     * At QBER = 0.25: h₂(0.25) ≈ 0.811, so t ≈ 0.811n.                 *
     * At QBER = 0.00: h₂(0.00) = 0,    so t = 0  (Eve has nothing).     *
     * ------------------------------------------------------------------ */
    double h2    = binary_entropy(qber);
    int    t     = (int)ceil((double)n * h2);       /* Eve's info upper bound */
    ak.estimated_eve_bits = (double)n * h2;

    /* ------------------------------------------------------------------ *
     * Step 2 — Compute final key length.                                  *
     *                                                                     *
     *   m = n - t - s                                                     *
     *                                                                     *
     * where s = sec_param is the security slack (40 recommended).        *
     * If m ≤ 0 the channel is too noisy to extract ANY secure key.       *
     * ------------------------------------------------------------------ */
    int m = n - t - sec_param;

    if (m <= 0) {
        printf("[PA] Key too short or QBER too high after amplification.\n"
               "     No secure key can be extracted (m = %d).\n", m);
        ak.length           = 0;
        ak.compression_ratio = 0.0;
        return ak;
    }

    ak.length = m;
    ak.compression_ratio = (double)m / (double)n;

    /* ------------------------------------------------------------------ *
     * Step 3 — Generate the Toeplitz seed (n + m - 1 random bits).       *
     *                                                                     *
     * In a real QKD system, this seed is agreed on over the public        *
     * authenticated classical channel AFTER sifting.  Since it is public  *
     * knowledge, Eve knowing the hash function does not help her:         *
     * the security proof holds even when the hash is public.              *
     * ------------------------------------------------------------------ */
    int        seed_len = n + m - 1;
    uint8_t   *seed     = (uint8_t *)malloc((size_t)seed_len);
    if (!seed) {
        fprintf(stderr, "[PA] malloc failed for Toeplitz seed.\n");
        ak.length = 0;
        return ak;
    }

    for (int i = 0; i < seed_len; i++) {
        seed[i] = (uint8_t)(rand() & 1);
    }

    /* ------------------------------------------------------------------ *
     * Step 4 — Apply the Toeplitz hash:  final_key = T × sifted_key      *
     * ------------------------------------------------------------------ */
    toeplitz_hash(ak.key, sifted->key, n, m, seed);

    free(seed);
    return ak;
}

/* -------------------------------------------------------------------------
 * print_amplified_key()
 * ------------------------------------------------------------------------- */
void print_amplified_key(const AmplifiedKey *ak)
{
    printf("\n========== PRIVACY AMPLIFICATION ===========\n");

    if (ak->length == 0) {
        printf("  [RESULT] No secure key could be extracted.\n");
        printf("  Reason: QBER too high — Eve's estimated info exceeds\n");
        printf("          the sifted key length + security parameter.\n");
        return;
    }

    printf("  Sifted key length   : %d bits\n",   ak->sifted_length);
    printf("  Eve's info estimate : %.1f bits  "
           "(= n × h₂(QBER))\n",                  ak->estimated_eve_bits);
    printf("  Final key length    : %d bits\n",   ak->length);
    printf("  Compression ratio   : %.2f  "
           "(%d / %d)\n",
           ak->compression_ratio, ak->length, ak->sifted_length);

    printf("\n  Final key bits: ");
    for (int i = 0; i < ak->length; i++) {
        printf("%d", ak->key[i]);
        if ((i + 1) % 8 == 0) printf(" ");
    }
    printf("\n");

    printf("\n  [NOTE] Eve's information about this %d-bit key\n", ak->length);
    printf("         is cryptographically negligible (≤ 2^{-40}).\n");
}
