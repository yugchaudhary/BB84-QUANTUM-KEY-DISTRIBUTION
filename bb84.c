/**
 * bb84.c — BB84 QKD Core Engine Implementation
 * ==============================================
 * Engineering Physics Project
 *
 * This file implements the four logical stages of the BB84 protocol:
 *
 *   Stage 1 — Alice prepares and transmits photons.
 *   Stage 2 — Eve (optionally) intercepts and resends (intercept-resend attack).
 *   Stage 3 — Bob measures the incoming photons.
 *   Stage 4 — Basis reconciliation (sifting) over a public channel.
 *
 * RANDOMNESS NOTE:
 *   A real quantum device uses true quantum randomness (shot noise, etc.).
 *   Here we use rand() seeded with time(). For a real project you could
 *   replace the RAND_BIT() macro with a call to /dev/urandom on Unix.
 */

#include "bb84.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* -------------------------------------------------------------------------
 * Internal Macro: RAND_BIT()
 *
 * Returns a uniformly random bit (0 or 1).
 * Uses the higher-order bits of rand() which are less correlated than the
 * low-order bits in many PRNG implementations.
 * ------------------------------------------------------------------------- */
#define RAND_BIT() ((uint8_t)((rand() >> 8) & 1))

/* =========================================================================
 * SECTION 1 — UTILITY: QUANTUM MEASUREMENT (WAVEFUNCTION COLLAPSE)
 * =========================================================================
 */

/**
 * measure_photon() — simulates the quantum measurement of a single photon.
 *
 * @param photon             Pointer to the QuantumState being measured.
 * @param measurement_basis  The basis chosen by the observer (Bob or Eve).
 * @return                   The classical bit result of the measurement.
 *
 * PHYSICS:
 * --------
 * This is the heart of the BB84 quantum logic.  We model the Born rule:
 *
 * Case A — SAME BASIS (alice_basis == measurement_basis):
 *   The photon is in an eigenstate of the measurement operator.
 *   Probability of getting the correct bit = 1.
 *   The original bit value is returned deterministically.
 *
 *   Example:
 *     Alice sends |1⟩_+  (vertical polarisation in rectilinear basis).
 *     Bob measures in (+) basis → always gets 1.
 *
 * Case B — DIFFERENT BASIS:
 *   The photon is in a SUPERPOSITION relative to the measurement operator.
 *
 *   Mathematically, the eigenstates of one basis can be written as equal
 *   superpositions of the eigenstates of the other basis.  For example:
 *
 *     |H⟩ = (1/√2)|+45°⟩ + (1/√2)|−45°⟩
 *
 *   By the Born rule the probability of each outcome = |amplitude|² = 0.5.
 *   So the measurement result is RANDOM (50% chance of 0, 50% of 1).
 *
 *   Crucially, the original state is DESTROYED — the photon collapses onto
 *   one of the new eigenstates and has no memory of its prior state.
 *   This is what causes detectable errors when Eve eavesdrops.
 */
uint8_t measure_photon(const QuantumState *photon, Basis measurement_basis)
{
    /* ------------------------------------------------------------------ *
     * Case A — Matching bases.  Correct bit recovered with certainty.    *
     * ------------------------------------------------------------------ */
    if (photon->basis == measurement_basis) {
        return photon->bit;
    }

    /* ------------------------------------------------------------------ *
     * Case B — Mismatched bases.                                          *
     *                                                                     *
     * The photon is in a superposition w.r.t. the measurement basis.     *
     * We simulate the probabilistic collapse using a random bit.         *
     *                                                                     *
     * In Dirac notation: ⟨eigenstate|ψ⟩ = 1/√2 for both eigenstates,   *
     * so P(0) = P(1) = |1/√2|² = 0.5.                                   *
     * ------------------------------------------------------------------ */
    return RAND_BIT();
}

/* =========================================================================
 * SECTION 2 — ALICE: PHOTON PREPARATION AND TRANSMISSION
 * =========================================================================
 */

/**
 * alice_generate() — Alice prepares n_qubits photons and "sends" them.
 *
 * @param alice    Output struct to populate.
 * @param n_qubits Number of photons to prepare (must be ≤ MAX_QUBITS).
 *
 * Alice's procedure (BB84 Step 1):
 *   1. Generate a random bit stream  {b₀, b₁, …, b_{n-1}}  in {0, 1}.
 *   2. Generate a random basis stream {β₀, β₁, …, β_{n-1}} in {+, ×}.
 *   3. For each photon i, encode bit bᵢ using basis βᵢ:
 *        (bᵢ=0, βᵢ=+) → horizontal polarisation  |H⟩
 *        (bᵢ=1, βᵢ=+) → vertical polarisation    |V⟩
 *        (bᵢ=0, βᵢ=×) → 45° polarisation         |+45⟩
 *        (bᵢ=1, βᵢ=×) → 135° polarisation        |−45⟩
 *
 * We store only the logical (bit, basis) representation since we are not
 * running real optics.
 */
void alice_generate(AliceData *alice, int n_qubits)
{
    /* Clamp to the hard limit defined in the header. */
    if (n_qubits > MAX_QUBITS) {
        fprintf(stderr,
            "[WARN] n_qubits (%d) exceeds MAX_QUBITS (%d). Clamping.\n",
            n_qubits, MAX_QUBITS);
        n_qubits = MAX_QUBITS;
    }

    alice->n_qubits = n_qubits;

    for (int i = 0; i < n_qubits; i++) {

        /* Step 1: Choose a random bit (0 or 1). */
        alice->bits[i] = RAND_BIT();

        /* Step 2: Choose a random basis (+ or ×) with equal probability. */
        alice->bases[i] = (Basis)RAND_BIT();

        /* Step 3: Encode the photon — store as a QuantumState struct. */
        alice->photons[i].bit   = alice->bits[i];
        alice->photons[i].basis = alice->bases[i];
    }
}

/* =========================================================================
 * SECTION 3 — EVE: INTERCEPT-RESEND ATTACK
 * =========================================================================
 */

/**
 * eve_intercept() — Eve performs a full intercept-resend attack.
 *
 * @param eve      Eve's state struct (must have eve->active == 1 to run).
 * @param photons  The array of in-flight photons from Alice. Eve MODIFIES
 *                 this array in-place to simulate re-emission.
 * @param n_qubits Number of photons to intercept.
 *
 * PHYSICS OF THE ATTACK:
 * ----------------------
 * Eve cannot clone the quantum state without disturbing it (the No-Cloning
 * Theorem prevents her from making a copy to measure separately).  Instead:
 *
 *   1. Eve intercepts each photon.
 *   2. She measures it in her own randomly chosen basis (which may differ
 *      from Alice's).  This causes WAVEFUNCTION COLLAPSE — the photon is
 *      projected onto one of Eve's basis eigenstates.
 *   3. She re-emits a NEW photon in the collapsed state toward Bob.
 *
 * CONSEQUENCE (Expected Error Rate):
 *   Eve guesses the correct basis 50% of the time.
 *     • Correct guess (50%): photon state unchanged → no error at Bob.
 *     • Wrong guess   (50%): collapsed to random state.
 *         – Bob uses same basis as Alice (50% of the time):
 *             Alice's original bit survives only 50% of the time.
 *             Error probability = 50%.
 *
 *   Net QBER introduced by Eve ≈ 25%  (0.5 × 0.5 = 0.25).
 *   A QBER > ~11% is a red flag in a real system.
 *
 * The photons[] array is modified to reflect the *re-emitted* photon states.
 * This is the key: Bob then measures these collapsed states, not Alice's.
 */
void eve_intercept(EveData *eve, QuantumState *photons, int n_qubits)
{
    /* If Eve is dormant, do nothing — photons pass through unmodified. */
    if (!eve->active) {
        return;
    }

    printf("[EVE] Eavesdropping on %d photons...\n", n_qubits);

    for (int i = 0; i < n_qubits; i++) {

        /* Step 1: Eve randomly picks her measurement basis. */
        eve->bases[i] = (Basis)RAND_BIT();

        /* Step 2: Eve MEASURES the photon.
         *
         * measure_photon() implements the Born-rule collapse:
         *   • If eve->bases[i] == photons[i].basis → correct bit obtained.
         *   • Otherwise                             → random bit (50/50).
         *
         * The original quantum state is destroyed by this measurement.
         */
        eve->measured_bits[i] = measure_photon(&photons[i], eve->bases[i]);

        /* Step 3: Eve RE-EMITS a new photon in her collapsed state.
         *
         * She has no choice but to send a photon encoded with the bit and
         * basis she measured — she has lost the original state information
         * when the wrong basis was used.
         *
         * We model this by overwriting the photon in the channel with
         * Eve's (potentially different) (bit, basis) pair.
         */
        photons[i].bit   = eve->measured_bits[i];
        photons[i].basis = eve->bases[i];

        /* The photon in-flight is now in Eve's re-emitted state. */
    }
}

/* =========================================================================
 * SECTION 4 — BOB: MEASUREMENT
 * =========================================================================
 */

/**
 * bob_measure() — Bob randomly chooses bases and measures incoming photons.
 *
 * @param bob      Bob's state struct to populate.
 * @param photons  Array of photons (possibly altered by Eve) to measure.
 * @param n_qubits Number of photons.
 *
 * Bob's procedure (BB84 Step 2):
 *   1. Choose a random measurement basis for each photon.
 *   2. Measure the photon → wavefunction collapses → record the bit.
 *
 * Note: Bob does NOT know Alice's bases yet (that comes at the sifting
 * step via the public classical channel).
 */
void bob_measure(BobData *bob, const QuantumState *photons, int n_qubits)
{
    bob->n_qubits = n_qubits;

    for (int i = 0; i < n_qubits; i++) {

        /* Step 1: Bob independently picks a random basis. */
        bob->bases[i] = (Basis)RAND_BIT();

        /* Step 2: Measure — Born-rule collapse is handled inside this call.
         *
         * If bob->bases[i] == photons[i].basis → correct bit with P=1.
         * Otherwise                            → random bit with P=0.5.
         *
         * When Eve is present, photons[i] is Eve's re-emission, so even a
         * basis match between Bob and Alice does NOT guarantee correctness
         * (Eve may have collapsed to a different bit already).
         */
        bob->measured_bits[i] = measure_photon(&photons[i], bob->bases[i]);
    }
}

/* =========================================================================
 * SECTION 5 — KEY SIFTING (BASIS RECONCILIATION)
 * =========================================================================
 */

/**
 * sift_key() — extracts the shared sifted key via public basis comparison.
 *
 * @param alice  Alice's complete data.
 * @param bob    Bob's complete data.
 * @return       A SiftedKey struct containing the key and its QBER.
 *
 * BASIS RECONCILIATION (BB84 Step 3):
 * ------------------------------------
 * After all photons have been sent and measured, Alice and Bob communicate
 * their BASIS CHOICES over a public (but authenticated) classical channel.
 * They discard all positions where their bases differ (~50% on average).
 *
 * The remaining positions form the SIFTED KEY.  Because both used the same
 * basis, quantum mechanics guarantees they should have identical bits —
 * UNLESS noise or an eavesdropper introduced errors.
 *
 * ERROR DETECTION (QBER):
 *   In practice Alice and Bob sacrifice a random subset of the sifted key
 *   to estimate the Quantum Bit Error Rate (QBER).  A QBER of:
 *     ~0%   → no eavesdropping detected, channel is clean.
 *     ~25%  → Eve is almost certainly present (intercept-resend).
 *     >11%  → abort key exchange (security threshold in many protocols).
 *
 * This simulation compares ALL matched bits to compute the error rate,
 * which is equivalent to sacrificing the entire sifted key for estimation.
 * In a real implementation you would reveal only a sample.
 */
SiftedKey sift_key(const AliceData *alice, const BobData *bob)
{
    SiftedKey result;
    memset(&result, 0, sizeof(SiftedKey));

    int n = alice->n_qubits;                /* Iterate over all photons. */

    for (int i = 0; i < n; i++) {

        /* ---------------------------------------------------------------- *
         * Check whether Alice and Bob used the SAME basis for position i. *
         *                                                                   *
         * XOR trick: (alice->bases[i] ^ bob->bases[i]) == 0 iff equal.   *
         * ---------------------------------------------------------------- */
        if (alice->bases[i] == bob->bases[i]) {

            /* Bases match → this position contributes to the sifted key. */
            result.key[result.length] = alice->bits[i];
            result.length++;

            /* ------------------------------------------------------------ *
             * ERROR DETECTION                                               *
             *                                                               *
             * If the bits disagree on a matched basis, it means either:    *
             *   (a) Eve measured in the wrong basis and re-emitted a       *
             *       photon in her basis → Bob measured Eve's photon, not   *
             *       Alice's.                                                *
             *   (b) Channel noise (in a more realistic model).             *
             * ------------------------------------------------------------ */
            if (alice->bits[i] != bob->measured_bits[i]) {
                result.n_errors++;
            }
        }
        /* Mismatched bases → this position is DISCARDED. */
    }

    /* Compute the Quantum Bit Error Rate (QBER). */
    if (result.length > 0) {
        result.error_rate = (double)result.n_errors / (double)result.length;
    } else {
        result.error_rate = 0.0;
    }

    return result;
}

/* =========================================================================
 * SECTION 6 — UTILITY / DEBUG PRINT FUNCTIONS
 * =========================================================================
 */

/**
 * print_alice() — pretty-print Alice's raw bits, bases, and prepared photons.
 * Intended for debugging with small n_qubits (e.g., ≤ 64).
 */
void print_alice(const AliceData *alice)
{
    printf("\n========== ALICE ===========\n");
    printf("Photons prepared: %d\n\n", alice->n_qubits);
    printf("  Idx | Bit | Basis | Photon state\n");
    printf("  ----|-----|-------|-------------\n");

    for (int i = 0; i < alice->n_qubits; i++) {
        /* Map (bit, basis) to a human-readable polarisation label. */
        const char *state;
        if (alice->bases[i] == BASIS_RECTILINEAR) {
            state = (alice->bits[i] == 0) ? "|H⟩ (0°)"  : "|V⟩ (90°)";
        } else {
            state = (alice->bits[i] == 0) ? "|+⟩ (45°)" : "|−⟩ (135°)";
        }
        printf("  %3d |  %d  |   %s   | %s\n",
               i, alice->bits[i], BASIS_NAMES[alice->bases[i]], state);
    }
}

/**
 * print_eve() — pretty-print Eve's interception data.
 */
void print_eve(const EveData *eve)
{
    printf("\n========== EVE ===========\n");
    if (!eve->active) {
        printf("  (Eve is NOT eavesdropping this run.)\n");
        return;
    }
    printf("  Idx | Eve basis | Eve measured\n");
    printf("  ----|-----------|-------------\n");
    /* Note: n_qubits is not stored in EveData; caller should know it. */
}

/**
 * print_bob() — pretty-print Bob's chosen bases and measured bits.
 */
void print_bob(const BobData *bob)
{
    printf("\n========== BOB ===========\n");
    printf("Photons measured: %d\n\n", bob->n_qubits);
    printf("  Idx | Bob basis | Measured bit\n");
    printf("  ----|-----------|-------------\n");

    for (int i = 0; i < bob->n_qubits; i++) {
        printf("  %3d |     %s     |      %d\n",
               i, BASIS_NAMES[bob->bases[i]], bob->measured_bits[i]);
    }
}

/**
 * print_sifted_key() — display the final sifted key and QBER analysis.
 */
void print_sifted_key(const SiftedKey *key)
{
    printf("\n========== SIFTED KEY ===========\n");
    printf("  Key length : %d bits\n",    key->length);
    printf("  Bit errors : %d\n",          key->n_errors);
    printf("  QBER       : %.2f%%\n",      key->error_rate * 100.0);

    /* Security assessment based on QBER thresholds. */
    printf("  Assessment : ");
    if (key->error_rate < 0.01) {
        printf("SECURE — Channel appears clean, no Eve detected.\n");
    } else if (key->error_rate < 0.11) {
        printf("CAUTION — Elevated QBER; possible noise or weak attack.\n");
    } else if (key->error_rate < 0.30) {
        printf("WARNING — High QBER (~25%% expected with Eve). "
               "Eavesdropper likely detected!\n");
    } else {
        printf("ABORT — QBER too high. Key exchange is compromised.\n");
    }

    printf("\n  Key bits: ");
    for (int i = 0; i < key->length; i++) {
        printf("%d", key->key[i]);
        if ((i + 1) % 8 == 0) printf(" ");   /* Group into bytes for readability. */
    }
    printf("\n");
}
