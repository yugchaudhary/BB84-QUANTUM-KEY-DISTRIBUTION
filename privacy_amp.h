/**
 * privacy_amp.h — Privacy Amplification via Universal Hashing
 * =============================================================
 * Engineering Physics Project — BB84 QKD Simulator
 *
 * WHAT IS PRIVACY AMPLIFICATION?
 * --------------------------------
 * After key sifting, Alice and Bob share a sifted key that is largely
 * identical, but Eve may have gained PARTIAL information about it by
 * measuring some qubits.  Privacy amplification "squeezes out" Eve's
 * knowledge by compressing the sifted key into a SHORTER, provably secret
 * final key using a randomised hash function.
 *
 * MATHEMATICAL BASIS — Universal Hashing:
 * ----------------------------------------
 * A family of hash functions {h : {0,1}^n → {0,1}^m} is called
 * 2-universal if for any two distinct inputs x ≠ y:
 *
 *     Pr[ h(x) = h(y) ] ≤ 1 / 2^m
 *
 * Bennett et al. (1995) proved that if Eve has at most `t` bits of
 * Shannon information about the sifted key of length `n`, and we apply
 * a random member of a 2-universal family to produce an output of length:
 *
 *     m = n - t - s                  (s = security parameter, e.g. 40 bits)
 *
 * then Eve's information about the final m-bit key is at most 2^{-s}.
 *
 * UPPER BOUND ON EVE'S INFORMATION:
 *   For the intercept-resend attack at QBER = q:
 *     t_upper ≈ n × h₂(q)     where h₂(p) = -p·log₂p - (1-p)·log₂(1-p)
 *                              is the binary entropy function.
 *
 * IMPLEMENTATION — Toeplitz Matrix Hash:
 *   We construct a random binary Toeplitz matrix T (m × n) and compute:
 *
 *     final_key = T × sifted_key   (matrix-vector product over GF(2))
 *
 * A Toeplitz matrix requires only n + m - 1 random bits (instead of n×m),
 * making it practical for hardware.  It also belongs to a strongly
 * 2-universal family, so it has the desired security properties.
 *
 * REFERENCE:
 *   Bennett, Brassard, Crépeau, Maurer — "Generalized Privacy Amplification"
 *   IEEE Transactions on Information Theory, 1995.
 */

#ifndef PRIVACY_AMP_H
#define PRIVACY_AMP_H

#include <stdint.h>
#include "bb84.h"   /* for SiftedKey, MAX_QUBITS */

/* Maximum length of the final (amplified) key in bits. */
#define MAX_FINAL_KEY MAX_QUBITS

/**
 * AmplifiedKey — the final provably-secure key after privacy amplification.
 */
typedef struct {
    uint8_t key[MAX_FINAL_KEY]; /**< Final key bits                           */
    int     length;              /**< Length in bits (= sifted - leaked - sec) */
    int     sifted_length;       /**< Original sifted key length               */
    double  compression_ratio;   /**< final_length / sifted_length             */
    double  estimated_eve_bits;  /**< t = n × h₂(QBER), Eve's estimated info  */
} AmplifiedKey;

/**
 * binary_entropy() — computes the binary entropy function h₂(p).
 *
 * h₂(p) = -p·log₂(p) - (1-p)·log₂(1-p)
 *
 * This tells us the maximum information (in bits) that a single biased
 * coin-flip with bias p can carry.  At QBER ≈ 0.25, h₂ ≈ 0.81 bits/qubit,
 * meaning Eve could know up to 81% of the sifted key.
 *
 * @param p  Probability in [0, 1].
 * @return   Binary entropy value in [0, 1].
 */
double binary_entropy(double p);

/**
 * privacy_amplify() — applies a random Toeplitz hash to the sifted key.
 *
 * @param sifted    The sifted key to compress.
 * @param qber      Observed Quantum Bit Error Rate (used to bound Eve's info).
 * @param sec_param Security parameter s (typical: 40).
 *                  Final key length = max(0, n - t - s).
 * @return          The final, provably-secure amplified key.
 */
AmplifiedKey privacy_amplify(const SiftedKey *sifted,
                              double           qber,
                              int              sec_param);

/**
 * print_amplified_key() — display the final key and privacy amplification info.
 */
void print_amplified_key(const AmplifiedKey *ak);

#endif /* PRIVACY_AMP_H */
