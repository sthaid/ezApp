package org.libsdl.app;

import android.app.Activity;
import android.app.Service;
import android.content.Intent;
import android.os.Binder;
import android.os.IBinder;
import android.util.Log;
import android.content.Context;
import android.os.SystemClock;
import android.content.pm.ServiceInfo;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;

import org.sthaid.ezApp.R;  // needed to access R.drawable.ic_notification

import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioPlaybackCaptureConfiguration;
import android.media.AudioRecord;
import android.media.MediaRecorder;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;

public class ezApp_media_fgsvc extends Service {

    private static final String    TAG = "EZAPP";
    private static final int       STATE_INITIALIZING = 1;
    private static final int       STATE_RECORDING    = 2;
    private static final int       STATE_FAILED       = 3;
    private static final int       STATE_STOPPED      = 4;
    private static final int       MAX_SAMPLES        = 2048;
    private static int             state;
    private static AudioRecord     audioRecord;
    private static MediaProjection mediaProjection;
    private static short[]         audio_data = new short[MAX_SAMPLES];
    private static short[]         error_data = new short[0];
    private IBinder                mBinder = new InnerBinder();

    public class InnerBinder extends Binder {
        ezApp_media_fgsvc getService() {
            return ezApp_media_fgsvc.this;
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int start_id) {
        String CHANNEL_ID          = "ezApp_channel";
        String CHANNEL_NAME        = "ezApp";
        String CHANNEL_DESCRIPTION = "description";
        int    NOTIFICATION_ID     = 101;

        int SAMPLE_RATE    = 48000;
        int CHANNEL_CONFIG = AudioFormat.CHANNEL_IN_STEREO;
        int AUDIO_FORMAT   = AudioFormat.ENCODING_PCM_16BIT;
        int bufferSizeInBytes;
        MediaProjectionManager projectionManager;

        int    resultCode = intent.getIntExtra("resultCode", Activity.RESULT_CANCELED);
        Intent data       = intent.getParcelableExtra("data");

        // print starting msg< and init variables
        Log.i(TAG, "starting media fgsvc");
        state           = STATE_INITIALIZING;
        audioRecord     = null;
        mediaProjection = null;

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
                  ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION);

        // access mediaProjection
        projectionManager = (MediaProjectionManager) getSystemService(Context.MEDIA_PROJECTION_SERVICE);
        mediaProjection = projectionManager.getMediaProjection(resultCode, data);
        if (mediaProjection == null) {
            Log.e(TAG, "getMediaProjection failed\n");
            state = STATE_FAILED;;
            return START_NOT_STICKY;
        }

        // get minimum buffer size
        bufferSizeInBytes = AudioRecord.getMinBufferSize(SAMPLE_RATE, CHANNEL_CONFIG, AUDIO_FORMAT);

        // Build the AudioPlaybackCaptureConfiguration
        AudioPlaybackCaptureConfiguration captureConfig =
            new AudioPlaybackCaptureConfiguration.Builder(mediaProjection)
                .addMatchingUsage(AudioAttributes.USAGE_MEDIA) // Capture media playback audio
                .addMatchingUsage(AudioAttributes.USAGE_GAME)  // Capture game audio 
                .build();

        // Configure the AudioFormat
        AudioFormat format = new AudioFormat.Builder()
            .setSampleRate(SAMPLE_RATE)
            .setChannelMask(CHANNEL_CONFIG)
            .setEncoding(AUDIO_FORMAT)
            .build();

        // Initialize AudioRecord with the configuration
        audioRecord = new AudioRecord.Builder()
            // "Cannot both set audio source and set playback capture config"
            //.setAudioSource(MediaRecorder.AudioSource.DEFAULT)
            .setAudioFormat(format)
            .setBufferSizeInBytes(bufferSizeInBytes)
            .setAudioPlaybackCaptureConfig(captureConfig) // key step
            .build();

        // Start recording
        Log.i(TAG, "start recording");
        audioRecord.startRecording();
        state = STATE_RECORDING;

        // do not restart service if killed by system
        return START_NOT_STICKY;
    }

    // question: why does this have to be static
    public static short[] get_playbackcapture_audio() {
        int shorts_read = 0;
        int millisecs = 0;

        // wait for up to 5 seconds for STATE_RECORDING
        while (true) {
            if (state == STATE_RECORDING) {
                break;
            }
            if (millisecs > 5000 || state == STATE_FAILED) {
                if (millisecs > 5000) {
                    Log.e(TAG, "get_playbackcapture_audio timedout");
                } else {
                    Log.e(TAG, "get_playbackcapture_audio STATE_FAILED");
                }
                return error_data;
            }
            SystemClock.sleep(100);
            millisecs += 100;
        }
            
        // read audio data
        shorts_read = audioRecord.read(audio_data, 0, MAX_SAMPLES);
        if (shorts_read != MAX_SAMPLES ) {
            Log.e(TAG, "get_playbackcapture_audio short_read = " + shorts_read);
            return error_data;
        }

        // return audio data
        return audio_data;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return mBinder;
    }

    @Override
    public void onDestroy() {
        Log.i(TAG, "stopping media fgsvc");

        if (audioRecord != null) {
            audioRecord.stop();
            audioRecord.release();
            audioRecord = null;
        }

        if (mediaProjection != null) {
            mediaProjection.stop();
            mediaProjection = null;
        }

        state = STATE_STOPPED;

        stopSelf();
    }
}
