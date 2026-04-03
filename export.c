/**
 * export.c — CSV Data Export Implementation
 * ===========================================
 * Engineering Physics Project — BB84 QKD Simulator
 */

#include "export.h"
#include "stats.h"
#include "bb84.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* =========================================================================
 * export_trials_csv() — per-trial CSV dump
 * =========================================================================
 */
int export_trials_csv(const StatsReport *report, const char *filename)
{
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "[EXPORT] Failed to open '%s' for writing.\n", filename);
        return -1;
    }

    /* CSV header row. */
    fprintf(fp, "trial,n_qubits,eve_active,sifted_length,"
                "errors,qber,final_key_len\n");

    for (int t = 0; t < report->n_trials; t++) {
        const TrialResult *r = &report->trials[t];
        fprintf(fp, "%d,%d,%d,%d,%d,%.6f,%d\n",
                t + 1,
                r->n_qubits,
                report->eve_active,
                r->sifted_length,
                r->n_errors,
                r->qber,
                r->final_key_len);
    }

    fclose(fp);
    printf("[EXPORT] %d trials written to '%s'\n", report->n_trials, filename);
    return 0;
}

/* =========================================================================
 * export_sweep_csv() — sweep n_qubits in [n_min, n_max], two runs per point
 *                      (clean channel + Eve), write summary CSV.
 * =========================================================================
 */
int export_sweep_csv(int n_min, int n_max, int n_steps,
                     int n_trials, int sec_param, const char *filename)
{
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "[EXPORT] Failed to open '%s' for writing.\n", filename);
        return -1;
    }

    /* CSV header. */
    fprintf(fp, "n_qubits,eve_active,qber_mean,qber_stddev,"
                "avg_sifted_len,avg_final_key_len,avg_compression\n");

    /* Compute the step size.  Guard against n_steps <= 1. */
    int step_size = (n_steps > 1) ? (n_max - n_min) / (n_steps - 1) : 1;
    if (step_size < 1) step_size = 1;

    int total_points = 0;

    printf("[EXPORT] Sweep: n = %d…%d (%d steps), %d trials/point\n",
           n_min, n_max, n_steps, n_trials);

    for (int n = n_min; n <= n_max; n += step_size) {

        if (n > MAX_QUBITS) n = MAX_QUBITS;

        /* Run once with no Eve, once with Eve. */
        for (int eve = 0; eve <= 1; eve++) {

            StatsReport rep = stats_run(n_trials, n, eve, sec_param);

            double stddev = sqrt(rep.qber_variance);

            fprintf(fp, "%d,%d,%.6f,%.6f,%.2f,%.2f,%.4f\n",
                    n, eve,
                    rep.qber_mean,
                    stddev,
                    rep.avg_sifted_len,
                    rep.avg_final_len,
                    rep.avg_compression);

            total_points++;
        }

        /* Progress indicator. */
        printf("  n = %4d  done.\r", n);
        fflush(stdout);

        if (n == MAX_QUBITS) break;
    }

    printf("\n[EXPORT] %d data points written to '%s'\n",
           total_points, filename);
    fclose(fp);
    return 0;
}
