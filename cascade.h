/**
 * cascade.h — Cascade Error Correction Protocol
 * ===============================================
 * Engineering Physics Project — BB84 QKD Simulator
 *
 * WHAT IS CASCADE?
 * ----------------
 * After key sifting, Alice and Bob hold *almost* identical bit strings,
 * but residual errors (from Eve or channel noise) mean a few bits differ.
 * Privacy amplification CANNOT be applied safely until these errors are
 * corrected, because XOR-based hashing cannot fix bit flips — it only
 * compresses away Eve's partial information.
 *
 * Cascade (Brassard & Salvail, 1993) is the canonical interactive error
 * correction protocol for QKD.  It works in PASSES over the key, using
 * a divide-and-conquer parity check approach.
 *
 * HOW CASCADE WORKS — PER PASS:
 * --------------------------------
 *   1. Alice and Bob publicly agree on a random PERMUTATION of key positions.
 *   2. They divide the permuted key into BLOCKS of size k (doubles each pass).
 *   3. For each block they compare PARITIES over the public channel:
 *        parity(block) = XOR of all bits in the block (0 or 1).
 *   4. If parities MATCH  → no error (or even number of errors) in block.
 *      If parities DIFFER → odd number of errors detected.  They run
 *        BINARY SEARCH (BISECT) inside that block to isolate and flip the
 *        first error.
 *   5. After each error flip, all *previous* passes' blocks that contain
 *      the corrected position are re-checked (the "cascade" effect) —
 *      this propagates corrections globally.
 *
 * INFORMATION LEAKAGE:
 *   Each parity bit revealed leaks 1 bit of information to Eve.
 *   Privacy amplification must account for this:
 *     total_leaked = n_parity_bits_revealed   (tracked in CascadeResult)
 *
 * NUMBER OF PASSES:
 *   Typically 4 passes are sufficient to reduce uncorrected error
 *   probability below 10^{-6} for QBER ≤ 15%.
 *
 * BLOCK SIZE k1 (first pass):
 *   Optimal k1 ≈ 0.73 / QBER  (derived from expected errors-per-block ≈ 1).
 *
 * REFERENCE:
 *   Brassard & Salvail — "Secret-Key Reconciliation by Public Discussion"
 *   Eurocrypt 1993, LNCS 765, pp. 410-423.
 */

#ifndef CASCADE_H
#define CASCADE_H

#include "bb84.h"    /* for MAX_QUBITS */

/* Number of Cascade passes to run (4 is standard). */
#define CASCADE_PASSES 4

/* Hard cap on iterations of binary search (prevents infinite loops). */
#define CASCADE_BISECT_MAX_ITERS 32

/**
 * CascadeResult — outcome of one Cascade reconciliation run.
 */
typedef struct {
    uint8_t corrected_key[MAX_QUBITS]; /**< Bob's key after correction       */
    int     key_length;                 /**< Number of bits in the key        */
    int     errors_found;               /**< Total single-bit errors fixed    */
    int     parities_revealed;          /**< Total parity bits sent publicly  */
    int     passes_run;                 /**< Number of passes actually run    */
    double  residual_error_rate;        /**< Estimated remaining QBER after   */
} CascadeResult;

/**
 * cascade_reconcile() — run the Cascade protocol to correct Bob's key.
 *
 * @param alice_key    Alice's reference key (ground truth), n bits.
 * @param bob_key      Bob's sifted key (possibly has errors), n bits.
 * @param n            Key length (number of bits).
 * @param qber         Observed QBER, used to compute initial block size.
 * @param n_passes     Number of Cascade passes (typically 4).
 * @return             CascadeResult with the corrected key and statistics.
 *
 * NOTE: In a real QKD deployment Alice and Bob can only compare PARITIES
 * (1-bit summaries) — they never share raw key bits over the public channel.
 * This simulation has access to both keys for validation purposes, but the
 * algorithm uses only parity information for its decisions.
 */
CascadeResult cascade_reconcile(const uint8_t *alice_key,
                                const uint8_t *bob_key,
                                int            n,
                                double         qber,
                                int            n_passes);

/**
 * print_cascade_result() — display reconciliation statistics.
 */
void print_cascade_result(const CascadeResult *result);

#endif /* CASCADE_H */
