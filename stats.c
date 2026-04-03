/**
 * stats.c — Statistical Analysis Engine Implementation
 * =====================================================
 * Engineering Physics Project — BB84 QKD Simulator
 */

#include "stats.h"
#include "bb84.h"
#include "privacy_amp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>   /* DBL_MAX */

/* =========================================================================
 * stats_run() — Monte-Carlo BB84 simulation
 * =========================================================================
 *
 * Repeats a full BB84 exchange (Alice → Eve? → Bob → Sifting → PA)
 * n_trials times and records per-trial statistics.
 */
StatsReport stats_run(int n_trials, int n_qubits, int eve_active, int sec_param)
{
    StatsReport report;
    memset(&report, 0, sizeof(StatsReport));

    /* Clamp to array bounds. */
    if (n_trials > MAX_TRIALS) n_trials = MAX_TRIALS;

    report.n_trials    = n_trials;
    report.n_qubits    = n_qubits;
    report.eve_active  = eve_active;
    report.qber_min    = DBL_MAX;
    report.qber_max    = 0.0;

    double qber_sum    = 0.0;
    double qber_sq_sum = 0.0;   /* for variance: Σ(x²) */
    double sifted_sum  = 0.0;
    double final_sum   = 0.0;

    for (int t = 0; t < n_trials; t++) {

        /* ---------------------------------------------------------------- *
         *  One complete BB84 exchange.                                      *
         * ---------------------------------------------------------------- */
        AliceData alice;
        BobData   bob;
        EveData   eve;
        eve.active = eve_active;

        alice_generate(&alice, n_qubits);
        eve_intercept(&eve, alice.photons, n_qubits);
        bob_measure(&bob, alice.photons, n_qubits);

        SiftedKey    sifted = sift_key(&alice, &bob);
        AmplifiedKey amp    = privacy_amplify(&sifted, sifted.error_rate,
                                              sec_param);

        /* ---------------------------------------------------------------- *
         *  Record per-trial result.                                         *
         * ---------------------------------------------------------------- */
        TrialResult *r   = &report.trials[t];
        r->n_qubits      = n_qubits;
        r->sifted_length  = sifted.length;
        r->n_errors       = sifted.n_errors;
        r->qber           = sifted.error_rate;
        r->final_key_len  = amp.length;

        /* ---------------------------------------------------------------- *
         *  Accumulate for aggregate statistics.                             *
         * ---------------------------------------------------------------- */
        qber_sum    += r->qber;
        qber_sq_sum += r->qber * r->qber;
        sifted_sum  += r->sifted_length;
        final_sum   += r->final_key_len;

        if (r->qber < report.qber_min) report.qber_min = r->qber;
        if (r->qber > report.qber_max) report.qber_max = r->qber;
    }

    /* ------------------------------------------------------------------ *
     * Compute aggregate statistics from accumulators.                     *
     *                                                                     *
     * Variance formula (Welford / textbook):                              *
     *   Var(X) = E[X²] - (E[X])²                                         *
     *          = (Σx²)/n  -  (Σx/n)²                                     *
     * ------------------------------------------------------------------ */
    double n = (double)n_trials;
    report.qber_mean      = qber_sum    / n;
    report.qber_variance  = (qber_sq_sum / n) - (report.qber_mean * report.qber_mean);
    report.avg_sifted_len = sifted_sum  / n;
    report.avg_final_len  = final_sum   / n;

    if (report.avg_sifted_len > 0.0) {
        report.avg_compression = report.avg_final_len / report.avg_sifted_len;
    }

    return report;
}

/* =========================================================================
 * stats_print_report() — formatted aggregate results table
 * =========================================================================
 */
void stats_print_report(const StatsReport *report)
{
    const char *eve_str = report->eve_active ? "YES ⚠" : "no";

    printf("\n╔══════════════════════════════════════════════════════╗\n");
    printf("║              STATISTICAL ANALYSIS REPORT            ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n\n");

    printf("  Trials performed    : %d\n",    report->n_trials);
    printf("  Photons per trial   : %d\n",    report->n_qubits);
    printf("  Eve active          : %s\n\n",  eve_str);

    printf("  ── QBER Statistics ─────────────────────────────────\n");
    printf("  Mean QBER           : %6.2f%%\n", report->qber_mean     * 100.0);
    printf("  Std. deviation      : %6.2f%%\n",
           sqrt(report->qber_variance) * 100.0);
    printf("  Variance            : %6.4f\n",   report->qber_variance);
    printf("  Min QBER            : %6.2f%%\n", report->qber_min      * 100.0);
    printf("  Max QBER            : %6.2f%%\n", report->qber_max      * 100.0);

    /* ------------------------------------------------------------------ *
     * Theoretical mean QBER for the intercept-resend attack = 25%.       *
     * Show deviation from theory to validate the simulation.             *
     * ------------------------------------------------------------------ */
    if (report->eve_active) {
        double theory    = 0.25;
        double deviation = fabs(report->qber_mean - theory) * 100.0;
        printf("  Theoretical (Eve)   :  25.00%%  "
               "(deviation: %.2f%%)\n", deviation);
    }

    printf("\n  ── Key Length Statistics ────────────────────────────\n");
    printf("  Avg sifted length   : %6.1f bits\n", report->avg_sifted_len);
    printf("  Avg final key (PA)  : %6.1f bits\n", report->avg_final_len);
    printf("  Avg compression     : %6.2f  "
           "(final / sifted)\n\n",                  report->avg_compression);

    /* ------------------------------------------------------------------ *
     * Security assessment from the mean QBER.                            *
     * ------------------------------------------------------------------ */
    printf("  ── Security Assessment ──────────────────────────────\n");
    if (report->qber_mean < 0.01) {
        printf("  SECURE  — Mean QBER < 1%%. Eve not detected.\n");
    } else if (report->qber_mean < 0.11) {
        printf("  CAUTION — Elevated mean QBER. Possible noise.\n");
    } else if (report->qber_mean <= 0.30) {
        printf("  WARNING — Mean QBER ≈ 25%%. Eve detected with "
               "high probability.\n");
    } else {
        printf("  ABORT   — Mean QBER > 30%%. Key exchange is unsafe.\n");
    }
    printf("\n");
}

/* =========================================================================
 * stats_print_histogram() — ASCII bar chart of QBER frequency distribution
 * =========================================================================
 */
void stats_print_histogram(const StatsReport *report, int bins, int width)
{
    if (report->n_trials == 0 || bins <= 0 || width <= 0) return;

    /* ------------------------------------------------------------------ *
     * Build frequency buckets.                                            *
     * Range: [0, 0.5] covers all realistic QBER values.                  *
     * ------------------------------------------------------------------ */
    double range     = 0.5;
    double bin_width = range / (double)bins;
    int   *counts    = (int *)calloc((size_t)bins, sizeof(int));
    if (!counts) return;

    int max_count = 0;

    for (int t = 0; t < report->n_trials; t++) {
        int bin_idx = (int)(report->trials[t].qber / bin_width);
        if (bin_idx >= bins) bin_idx = bins - 1;
        counts[bin_idx]++;
        if (counts[bin_idx] > max_count) max_count = counts[bin_idx];
    }

    /* ------------------------------------------------------------------ *
     * Print the histogram.                                                *
     * Each bar uses the '█' block character for a solid, clean look.     *
     * ------------------------------------------------------------------ */
    printf("\n  QBER Distribution  (n = %d trials)\n", report->n_trials);
    printf("  ─────────────────────────────────────────\n");

    for (int i = 0; i < bins; i++) {
        double lo = (double)i       * bin_width * 100.0;
        double hi = (double)(i + 1) * bin_width * 100.0;

        /* Scale bar length to 'width' characters. */
        int bar_len = (max_count > 0)
                      ? (counts[i] * width / max_count)
                      : 0;

        printf("  %4.1f-%4.1f%% |", lo, hi);
        for (int b = 0; b < bar_len; b++) printf("█");
        printf(" %d\n", counts[i]);
    }
    printf("  ─────────────────────────────────────────\n\n");

    free(counts);
}
