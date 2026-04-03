/**
 * bb84.h — BB84 Quantum Key Distribution Simulation
 * ===================================================
 * Engineering Physics Project
 *
 * This header defines the core data types and function prototypes for a
 * software simulation of the BB84 protocol, originally proposed by
 * Charles Bennett and Gilles Brassard in 1984.
 *
 * PHYSICS BACKGROUND:
 * -------------------
 * In real quantum cryptography, the "bit" is encoded as the polarisation
 * of a single photon.  Two conjugate (mutually unbiased) bases are used:
 *
 *   Rectilinear basis (+) : 0° / 90°   (horizontally / vertically polarised)
 *   Diagonal basis    (×) : 45° / 135° (diagonally polarised)
 *
 * KEY QUANTUM PRINCIPLE — Wavefunction Collapse:
 *   When Bob (or Eve) measures a photon, the act of measurement PROJECTS the
 *   photon's quantum state onto one of the eigenstates of the chosen basis.
 *
 *   • If the measurement basis matches the preparation basis → the original
 *     bit value is recovered with probability 1 (perfect correlation).
 *
 *   • If the measurement basis is DIFFERENT → the photon is in a quantum
 *     superposition relative to that basis.  The Born rule gives equal
 *     probability (50%) of collapsing to either eigenstate, producing a
 *     completely RANDOM result and DESTROYING the original information.
 *
 * This file models that collapse statistically using a PRNG.
 */

#ifndef BB84_H
#define BB84_H

#include <stdint.h>
#include <stdlib.h>

/* =========================================================================
 * SECTION 1 — CONSTANTS AND ENUMERATIONS
 * =========================================================================
 */

/** Maximum number of photons (qubits) that can be simulated in one run. */
#define MAX_QUBITS 4096

/**
 * Encoding / measurement basis.
 *
 * Rectilinear (+):  |0⟩ = horizontal, |1⟩ = vertical
 * Diagonal    (×):  |+⟩ = 45°,        |−⟩ = 135°
 *
 * Choosing RECTILINEAR = 0 and DIAGONAL = 1 lets us XOR two bases to check
 * whether they match: (alice_basis XOR bob_basis) == 0 means they are equal.
 */
typedef enum {
    BASIS_RECTILINEAR = 0,  /**< (+) basis — 0° / 90° polarisations  */
    BASIS_DIAGONAL    = 1   /**< (×) basis — 45°/135° polarisations  */
} Basis;

/**
 * Convenience string representations for printing.
 * Index with a Basis value.
 */
static const char *BASIS_NAMES[] = { "+", "×" };

/* =========================================================================
 * SECTION 2 — CORE DATA STRUCTURES
 * =========================================================================
 */

/**
 * QuantumState — models a single photon travelling through the channel.
 *
 * In real physics this would be the full density matrix ρ of the photon.
 * Here we store the CLASSICAL representation of the quantum information:
 *   • 'bit'   — the logical bit value Alice encodes (0 or 1).
 *   • 'basis' — the polarisation basis used to encode that bit.
 *
 * After Eve intercepts and re-emits (or after Bob measures), the photon is
 * in a *collapsed* state, which we represent by updating these fields.
 */
typedef struct {
    uint8_t bit;    /**< Logical bit value: 0 or 1                    */
    Basis   basis;  /**< Preparation / collapsed basis of this photon */
} QuantumState;

/**
 * AliceData — encapsulates all information on Alice's (sender's) side.
 *
 * Alice generates n random bits and n random bases, then prepares a photon
 * in the corresponding polarisation for each (bit, basis) pair.
 */
typedef struct {
    int           n_qubits;               /**< Number of photons sent          */
    uint8_t       bits[MAX_QUBITS];       /**< Alice's original random bits     */
    Basis         bases[MAX_QUBITS];      /**< Alice's randomly chosen bases    */
    QuantumState  photons[MAX_QUBITS];    /**< Photons prepared and transmitted */
} AliceData;

/**
 * BobData — encapsulates all information on Bob's (receiver's) side.
 *
 * Bob independently chooses random measurement bases.  He then measures each
 * incoming photon and records the collapsed bit value.
 */
typedef struct {
    int     n_qubits;                  /**< Number of photons received          */
    Basis   bases[MAX_QUBITS];         /**< Bob's randomly chosen bases         */
    uint8_t measured_bits[MAX_QUBITS]; /**< Bits obtained after measurement     */
} BobData;

/**
 * EveData — encapsulates the eavesdropper's state.
 *
 * Eve sits on the quantum channel between Alice and Bob.  She measures each
 * photon with her own random basis and then re-emits a NEW photon in the
 * collapsed state toward Bob.  This is the "intercept-resend" attack.
 *
 * The 'active' flag lets us toggle Eve's presence without recompiling.
 */
typedef struct {
    int     active;                      /**< 1 = Eve is eavesdropping, 0 = off */
    Basis   bases[MAX_QUBITS];           /**< Eve's randomly chosen bases        */
    uint8_t measured_bits[MAX_QUBITS];   /**< Bits Eve measured                  */
} EveData;

/**
 * SiftedKey — the final shared secret extracted after basis reconciliation.
 *
 * Only the subset of positions where Alice and Bob chose the SAME basis
 * contribute to the sifted key.  The error_rate field tells us what fraction
 * of matched positions have mismatched bit values (non-zero → eavesdropping
 * or noise detected).
 */
typedef struct {
    uint8_t key[MAX_QUBITS];  /**< Sifted key bits (only the matching positions) */
    int     length;            /**< Number of bits in the sifted key              */
    int     n_errors;          /**< Number of bit errors found in matched positions*/
    double  error_rate;        /**< Quantum Bit Error Rate (QBER) = n_errors/length*/
} SiftedKey;

/* =========================================================================
 * SECTION 3 — FUNCTION PROTOTYPES
 * =========================================================================
 */

/* --- Alice --- */
void alice_generate(AliceData *alice, int n_qubits);

/* --- Eve (intercept-resend attack) --- */
void eve_intercept(EveData *eve, QuantumState *photons, int n_qubits);

/* --- Bob --- */
void bob_measure(BobData *bob, const QuantumState *photons, int n_qubits);

/* --- Key sifting --- */
SiftedKey sift_key(const AliceData *alice, const BobData *bob);

/* --- Utility --- */
uint8_t measure_photon(const QuantumState *photon, Basis measurement_basis);
void    print_alice(const AliceData *alice);
void    print_eve(const EveData *eve);
void    print_bob(const BobData *bob);
void    print_sifted_key(const SiftedKey *key);

#endif /* BB84_H */
