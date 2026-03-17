package com.sfe.dashboard;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.os.Build;
import android.os.IBinder;
import android.os.PowerManager;

/**
 * DashService — foreground service that keeps the process alive and the CPU awake
 * when the app is minimized.  The OBDManager polling thread and BT socket remain
 * in MainActivity; this service simply prevents the OS from killing the process
 * due to memory pressure and holds a PARTIAL_WAKE_LOCK so polling keeps running.
 *
 * Started from MainActivity.onResume(), stopped from MainActivity.onDestroy()
 * (user explicitly exits the app).
 */
public class DashService extends Service {

    private static final String CHANNEL_ID = "sfe_dash_obd";
    private static final int    NOTIF_ID   = 1001;

    private PowerManager.WakeLock wakeLock;

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        ensureNotificationChannel();

        Intent tapIntent = new Intent(this, MainActivity.class);
        tapIntent.setFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
        int piFlags = Build.VERSION.SDK_INT >= 23
            ? PendingIntent.FLAG_IMMUTABLE | PendingIntent.FLAG_UPDATE_CURRENT
            : PendingIntent.FLAG_UPDATE_CURRENT;
        PendingIntent pi = PendingIntent.getActivity(this, 0, tapIntent, piFlags);

        Notification.Builder nb = new Notification.Builder(this, CHANNEL_ID)
            .setContentTitle("SFE Dash")
            .setContentText("OBD logging active")
            .setSmallIcon(android.R.drawable.ic_menu_compass)
            .setContentIntent(pi)
            .setOngoing(true);

        startForeground(NOTIF_ID, nb.build());

        if (wakeLock == null) {
            PowerManager pm = (PowerManager) getSystemService(POWER_SERVICE);
            wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "SFEDash:OBD");
            wakeLock.acquire();
        }

        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        if (wakeLock != null && wakeLock.isHeld()) {
            wakeLock.release();
            wakeLock = null;
        }
        stopForeground(true);
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) { return null; }

    private void ensureNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel ch = new NotificationChannel(
                CHANNEL_ID, "SFE Dash", NotificationManager.IMPORTANCE_LOW);
            ch.setDescription("OBD data logging");
            ch.setShowBadge(false);
            NotificationManager nm = getSystemService(NotificationManager.class);
            if (nm != null) nm.createNotificationChannel(ch);
        }
    }
}
