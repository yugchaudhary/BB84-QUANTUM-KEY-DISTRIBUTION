/**
 * export.h — CSV Data Export
 * ===========================
 * Engineering Physics Project — BB84 QKD Simulator
 *
 * Writes simulation results (per-trial statistics) to CSV files that can
 * be loaded into Python/matplotlib, Excel, MATLAB, or any plotting tool.
 *
 * OUTPUT FILES:
 *   bb84_trials.csv    — per-trial rows: n_qubits, qber, sifted_len,
 *                        final_key_len, errors
 *   bb84_sweep.csv     — QBER vs. photon-count sweep summary table
 *
 * EXAMPLE PYTHON USAGE (after running --export):
 *   import pandas as pd, matplotlib.pyplot as plt
 *   df = pd.read_csv("bb84_trials.csv")
 *   df.boxplot(column="qber", by="eve_active")
 *   plt.show()
 */

#ifndef EXPORT_H
#define EXPORT_H

#include "stats.h"    /* StatsReport, TrialResult */

/**
 * export_trials_csv() — write per-trial data to a CSV file.
 *
 * Columns:
 *   trial, n_qubits, eve_active, sifted_length, errors, qber, final_key_len
 *
 * @param report    StatsReport containing populated trials[] array.
 * @param filename  Destination path (e.g. "bb84_trials.csv").
 * @return          0 on success, -1 on file open error.
 */
int export_trials_csv(const StatsReport *report, const char *filename);

/**
 * export_sweep_csv() — run multiple photon-count values and export summary.
 *
 * Sweeps n_qubits from n_min to n_max in steps, running n_trials per point.
 * Outputs one row per (n_qubits, eve_active) combination:
 *   n_qubits, eve_active, qber_mean, qber_stddev, avg_sifted, avg_final_key
 *
 * @param n_min       Minimum photon count.
 * @param n_max       Maximum photon count.
 * @param n_steps     Number of evenly spaced steps between n_min and n_max.
 * @param n_trials    Trials per data point.
 * @param sec_param   Security parameter for privacy amplification.
 * @param filename    Destination path (e.g. "bb84_sweep.csv").
 * @return            0 on success, -1 on error.
 */
int export_sweep_csv(int n_min, int n_max, int n_steps,
                     int n_trials, int sec_param, const char *filename);

#endif /* EXPORT_H */
