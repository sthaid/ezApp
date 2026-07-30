package org.libsdl.app;

import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.util.Log;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;

import android.content.pm.ServiceInfo;

import org.sthaid.ezApp.R;  // needed to access R.drawable

public class ezApp_loc_fgsvc extends Service {

    private static final String TAG = "EZAPP";
    private IBinder             mBinder = new InnerBinder();

    public class InnerBinder extends Binder {
        ezApp_loc_fgsvc getService() {
            return ezApp_loc_fgsvc.this;
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int start_id) {
        String CHANNEL_ID          = "ezApp_channel";
        String CHANNEL_NAME        = "ezApp";
        String CHANNEL_DESCRIPTION = "description";
        int    NOTIFICATION_ID     = 100;

        // print starting msg
        Log.i(TAG, "starting loc_fgsvc");

        // Create a Notification Channel
        NotificationManager notificationManager = 
            (NotificationManager) this.getSystemService(this.NOTIFICATION_SERVICE);
        NotificationChannel channel = 
            new NotificationChannel(CHANNEL_ID, CHANNEL_NAME, NotificationManager.IMPORTANCE_DEFAULT);
        channel.setDescription(CHANNEL_DESCRIPTION);
        channel.enableVibration(true);
        channel.setLockscreenVisibility(Notification.VISIBILITY_PUBLIC); 
        notificationManager.createNotificationChannel(channel);

        // Build the notification
        Notification.Builder builder;
        builder = new Notification.Builder(this, CHANNEL_ID);
        Notification notification = builder
                .setContentTitle("foreground enabled")
                 //.setContentText("more text here if needed")
                .setSmallIcon(R.drawable.ic_notification)
                .setOngoing(true)
                .setPriority(Notification.PRIORITY_DEFAULT)
                .setAutoCancel(true)
                .setVisibility(Notification.VISIBILITY_PUBLIC)
                .build();

        // startForeground
        this.startForeground(NOTIFICATION_ID, notification, 
                  ServiceInfo.FOREGROUND_SERVICE_TYPE_LOCATION);

        // do not restart service if killed by system
        return START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    @Override
    public void onDestroy() {
        Log.i(TAG, "stopping loc fgsvc");
        stopSelf();
    }
}
