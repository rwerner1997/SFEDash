package com.sfe.dashboard;

import android.content.Context;
import android.util.Log;

import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/**
 * Writes per-session OBD diagnostic logs to app-specific external storage.
 * No WRITE_EXTERNAL_STORAGE permission required (app-specific path).
 *
 * File location (accessible via any file manager):
 *   Internal Storage / Android / data / com.sfe.dashboard / files / OBDLogs / sfe_YYYYMMDD_HHMMSS.csv
 *
 * CSV columns:
 *   elapsed_s  — seconds since this connection session started
 *   pid        — 6-char Mode 22 PID sent (e.g. "221021")
 *   raw        — exact trimmed response string from ELM327
 *   bytes      — first (and second if present) data byte in hex+decimal,
 *                e.g. "0xC2(194)" or "0xC2(194)+0x00(0)"; "ERR" for errors
 *
 * One file is created per connection.  Flushes every 200 rows; max 100 000 rows (~7 MB).
 */
class OBDLogger {

    private static final String TAG       = "OBDLogger";
    private static final int    MAX_LINES = 100_000;

    private BufferedWriter writer;
    private long           startMs;
    private int            lineCount;

    // ── Lifecycle ────────────────────────────────────────────────

    /** Open a new log file.  Call once per OBD connection, before the poll loop. */
    void open(Context ctx) {
        File dir = new File(ctx.getExternalFilesDir(null), "OBDLogs");
        //noinspection ResultOfMethodCallIgnored
        dir.mkdirs();
        String ts = new SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(new Date());
        File f = new File(dir, "sfe_" + ts + ".csv");
        startMs   = System.currentTimeMillis();
        lineCount = 0;
        try {
            writer = new BufferedWriter(new FileWriter(f), 8192);
            writer.write("elapsed_s,pid,raw,bytes\n");
            Log.i(TAG, "OBD log → " + f.getAbsolutePath());
        } catch (IOException e) {
            Log.e(TAG, "Cannot open log: " + e.getMessage());
            writer = null;
        }
    }

    /** Flush and close the current log file. */
    void close() {
        if (writer == null) return;
        try { writer.flush(); writer.close(); } catch (IOException ignored) {}
        writer = null;
        Log.i(TAG, "OBD log closed (" + lineCount + " lines)");
    }

    // ── Logging ──────────────────────────────────────────────────

    /**
     * Record one Mode 22 exchange.
     *
     * @param pid  6-char PID string, e.g. "221021"
     * @param raw  trimmed ELM327 response string
     * @param b0   first data byte value, or -1 if the response was an error / parse failure
     * @param b1   second data byte value, or -1 if absent (single-byte response)
     */
    void log(String pid, String raw, int b0, int b1) {
        if (writer == null || lineCount >= MAX_LINES) return;
        try {
            float t       = (System.currentTimeMillis() - startMs) / 1000f;
            String safeRaw = (raw == null) ? "" : raw.replace("\"", "'");
            String bytes;
            if (b0 < 0) {
                bytes = "ERR";
            } else if (b1 < 0) {
                bytes = String.format(Locale.US, "0x%02X(%d)", b0, b0);
            } else {
                bytes = String.format(Locale.US, "0x%02X(%d)+0x%02X(%d)", b0, b0, b1, b1);
            }
            writer.write(String.format(Locale.US, "%.3f,%s,\"%s\",%s\n",
                    t, pid, safeRaw, bytes));
            lineCount++;
            if (lineCount % 200 == 0) writer.flush();
        } catch (IOException e) {
            Log.w(TAG, "log write: " + e.getMessage());
        }
    }
}
