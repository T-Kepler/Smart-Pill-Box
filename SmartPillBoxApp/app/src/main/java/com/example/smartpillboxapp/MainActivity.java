package com.example.smartpillboxapp;

import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.widget.TextView;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;
import android.app.TimePickerDialog;
import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.Intent;
import android.content.Context;
import android.content.SharedPreferences;
import android.widget.TimePicker;
import android.util.Log;
import java.util.Calendar;
import com.android.volley.Request;
import com.android.volley.RequestQueue;
import com.android.volley.Response;
import com.android.volley.VolleyError;
import com.android.volley.toolbox.JsonObjectRequest;
import com.android.volley.toolbox.StringRequest;
import com.android.volley.toolbox.Volley;
import com.android.volley.DefaultRetryPolicy;
import org.json.JSONObject;
import org.json.JSONException;
import java.util.Locale;

public class MainActivity extends AppCompatActivity {
    private static final String TAG = "MainActivity";
    private static final String REQUEST_TAG = "pillbox_req";
    private static final String PREFS_NAME = "pillbox_prefs";
    private static final String PREF_DEVICE_IP = "device_ip";
    private static final int REFRESH_INTERVAL_MS = 3000;
    private static final int SYNC_INTERVAL_MS = 30000;
    private static final int VOLLEY_TIMEOUT_MS = 5000;
    private static final int MAX_RETRY_COUNT = 1;

    private static final String DEFAULT_DEVICE_IP = "192.168.43.2";

    private static final String[] SCAN_IPS = {
        "192.168.43.2", "192.168.43.3", "192.168.43.4", "192.168.43.5",
        "172.20.10.2", "172.20.10.3", "172.20.10.4",
        "192.168.1.100", "192.168.0.100", "10.0.0.2"
    };

    private TextView temperatureTextView;
    private TextView humidityTextView;
    private TextView medicineTextView;
    private TextView brightnessTextView;
    private TextView batteryTextView;
    private TextView volumeTextView;
    private TextView fanStatusTextView;
    private TextView heaterStatusTextView;
    private TextView ledStatusTextView;
    private TextView boxStatusTextView;
    private TextView wifiStatusTextView;
    private Button openPillBoxButton;
    private Button refreshButton;
    private Button volumeUpButton;
    private Button volumeDownButton;
    private Button reminderButton1;
    private Button reminderButton2;
    private Button reminderButton3;
    private TextView reminderTextView1;
    private TextView reminderTextView2;
    private TextView reminderTextView3;
    private EditText deviceIpEditText;
    private Button saveIpButton;
    private Button scanButton;
    private EditText tempHighEditText;
    private EditText tempLowEditText;
    private EditText lightOnEditText;
    private EditText lightOffEditText;
    private Button saveThresholdsButton;

    private String deviceIp;
    private RequestQueue requestQueue;
    private Handler refreshHandler;
    private boolean isResumed = false;
    private int consecutiveErrors = 0;
    private boolean isScanning = false;
    private boolean boxIsOpen = false;
    private boolean rtcSynced = false;
    private long lastSyncTime = 0;

    private int currentTempHigh = 30;
    private int currentTempLow = 15;
    private int currentLightOn = 60;
    private int currentLightOff = 65;

    private AlarmManager alarmManager1;
    private PendingIntent alarmIntent1;
    private int reminderHour1 = -1;
    private int reminderMinute1 = -1;

    private AlarmManager alarmManager2;
    private PendingIntent alarmIntent2;
    private int reminderHour2 = -1;
    private int reminderMinute2 = -1;

    private AlarmManager alarmManager3;
    private PendingIntent alarmIntent3;
    private int reminderHour3 = -1;
    private int reminderMinute3 = -1;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        loadDeviceIp();
        initViews();
        initVolley();
        initListeners();
        requestNotificationPermission();
        refreshHandler = new Handler(Looper.getMainLooper());
    }

    private void loadDeviceIp() {
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        deviceIp = prefs.getString(PREF_DEVICE_IP, DEFAULT_DEVICE_IP);
    }

    private void saveDeviceIp(String ip) {
        deviceIp = ip;
        SharedPreferences prefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        prefs.edit().putString(PREF_DEVICE_IP, ip).apply();
        Log.i(TAG, "Device IP saved: " + ip);
    }

    private void requestNotificationPermission() {
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.TIRAMISU) {
            if (checkSelfPermission(android.Manifest.permission.POST_NOTIFICATIONS) != android.content.pm.PackageManager.PERMISSION_GRANTED) {
                requestPermissions(new String[]{android.Manifest.permission.POST_NOTIFICATIONS}, 1);
            }
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        isResumed = true;
        startPeriodicRefresh();
    }

    @Override
    protected void onPause() {
        super.onPause();
        isResumed = false;
        stopPeriodicRefresh();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        stopPeriodicRefresh();
        if (requestQueue != null) {
            requestQueue.cancelAll(REQUEST_TAG);
        }
    }

    private void initViews() {
        temperatureTextView = findViewById(R.id.temperatureTextView);
        humidityTextView = findViewById(R.id.humidityTextView);
        medicineTextView = findViewById(R.id.medicineLevelTextView);
        brightnessTextView = findViewById(R.id.brightnessTextView);
        batteryTextView = findViewById(R.id.batteryTextView);
        volumeTextView = findViewById(R.id.volumeTextView);
        fanStatusTextView = findViewById(R.id.fanStatusTextView);
        heaterStatusTextView = findViewById(R.id.heaterStatusTextView);
        ledStatusTextView = findViewById(R.id.ledStatusTextView);
        boxStatusTextView = findViewById(R.id.boxStatusTextView);
        wifiStatusTextView = findViewById(R.id.wifiStatusTextView);
        openPillBoxButton = findViewById(R.id.openPillBoxButton);
        refreshButton = findViewById(R.id.refreshButton);
        volumeUpButton = findViewById(R.id.volumeUpButton);
        volumeDownButton = findViewById(R.id.volumeDownButton);
        reminderButton1 = findViewById(R.id.reminderButton1);
        reminderButton2 = findViewById(R.id.reminderButton2);
        reminderButton3 = findViewById(R.id.reminderButton3);
        reminderTextView1 = findViewById(R.id.reminderTextView1);
        reminderTextView2 = findViewById(R.id.reminderTextView2);
        reminderTextView3 = findViewById(R.id.reminderTextView3);
        deviceIpEditText = findViewById(R.id.deviceIpEditText);
        saveIpButton = findViewById(R.id.saveIpButton);
        scanButton = findViewById(R.id.scanButton);
        tempHighEditText = findViewById(R.id.tempHighEditText);
        tempLowEditText = findViewById(R.id.tempLowEditText);
        lightOnEditText = findViewById(R.id.lightOnEditText);
        lightOffEditText = findViewById(R.id.lightOffEditText);
        saveThresholdsButton = findViewById(R.id.saveThresholdsButton);

        if (deviceIpEditText != null) {
            deviceIpEditText.setText(deviceIp);
        }
    }

    private void initVolley() {
        requestQueue = Volley.newRequestQueue(this);
    }

    private void initListeners() {
        openPillBoxButton.setOnClickListener(v -> togglePillBox());
        refreshButton.setOnClickListener(v -> fetchDeviceStatus());
        volumeUpButton.setOnClickListener(v -> controlVolume("up"));
        volumeDownButton.setOnClickListener(v -> controlVolume("down"));
        reminderButton1.setOnClickListener(v -> showTimePicker(1));
        reminderButton2.setOnClickListener(v -> showTimePicker(2));
        reminderButton3.setOnClickListener(v -> showTimePicker(3));

        if (saveIpButton != null) {
            saveIpButton.setOnClickListener(v -> {
                String ip = deviceIpEditText.getText().toString().trim();
                if (!ip.isEmpty()) {
                    saveDeviceIp(ip);
                    Toast.makeText(this, "IP saved: " + ip, Toast.LENGTH_SHORT).show();
                    consecutiveErrors = 0;
                    fetchDeviceStatus();
                }
            });
        }

        if (scanButton != null) {
            scanButton.setOnClickListener(v -> scanForDevice());
        }

        if (saveThresholdsButton != null) {
            saveThresholdsButton.setOnClickListener(v -> saveThresholds());
        }
    }

    private void togglePillBox() {
        String action = boxIsOpen ? "close" : "open";
        String url = "http://" + deviceIp + "/api/" + action;

        StringRequest stringRequest = new StringRequest(
                Request.Method.POST,
                url,
                response -> {
                    Toast.makeText(MainActivity.this,
                            boxIsOpen ? "药盒已关闭" : "药盒已开启",
                            Toast.LENGTH_SHORT).show();
                    fetchDeviceStatus();
                },
                error -> Toast.makeText(MainActivity.this,
                        boxIsOpen ? "关闭药盒失败" : "开启药盒失败",
                        Toast.LENGTH_SHORT).show()
        );
        stringRequest.setTag(REQUEST_TAG);
        stringRequest.setRetryPolicy(new DefaultRetryPolicy(
                VOLLEY_TIMEOUT_MS, MAX_RETRY_COUNT, DefaultRetryPolicy.DEFAULT_BACKOFF_MULT));
        requestQueue.add(stringRequest);
    }

    private void updatePillBoxButton() {
        if (boxIsOpen) {
            openPillBoxButton.setText("关闭药盒");
            openPillBoxButton.setBackgroundColor(0xFFFF5722);
        } else {
            openPillBoxButton.setText("开启药盒");
            openPillBoxButton.setBackgroundColor(0xFF4CAF50);
        }
    }

    private void saveThresholds() {
        String thStr = tempHighEditText.getText().toString().trim();
        String tlStr = tempLowEditText.getText().toString().trim();
        String loStr = lightOnEditText.getText().toString().trim();
        String lfStr = lightOffEditText.getText().toString().trim();

        if (thStr.isEmpty() || tlStr.isEmpty() || loStr.isEmpty() || lfStr.isEmpty()) {
            Toast.makeText(this, "请填写所有阈值", Toast.LENGTH_SHORT).show();
            return;
        }

        String url = "http://" + deviceIp + "/api/threshold?temp_high=" + thStr
                + "&temp_low=" + tlStr + "&light_on=" + loStr + "&light_off=" + lfStr;

        StringRequest stringRequest = new StringRequest(
                Request.Method.GET,
                url,
                response -> {
                    Toast.makeText(MainActivity.this, "阈值已保存", Toast.LENGTH_SHORT).show();
                    Log.i(TAG, "Thresholds saved: " + response);
                },
                error -> {
                    Toast.makeText(MainActivity.this, "保存阈值失败", Toast.LENGTH_SHORT).show();
                    Log.e(TAG, "Save thresholds failed");
                }
        );
        stringRequest.setTag(REQUEST_TAG);
        stringRequest.setRetryPolicy(new DefaultRetryPolicy(
                VOLLEY_TIMEOUT_MS, MAX_RETRY_COUNT, DefaultRetryPolicy.DEFAULT_BACKOFF_MULT));
        requestQueue.add(stringRequest);
    }

    private void scanForDevice() {
        if (isScanning) return;
        isScanning = true;
        wifiStatusTextView.setText("Scanning...");
        Log.i(TAG, "Starting device scan...");

        new Thread(() -> {
            for (String ip : SCAN_IPS) {
                try {
                    Log.i(TAG, "Trying " + ip + "...");
                    String url = "http://" + ip + "/api/status";

                    JsonObjectRequest request = new JsonObjectRequest(
                            Request.Method.GET, url, null,
                            response -> {
                                try {
                                    if (response.getInt("code") == 0) {
                                        Log.i(TAG, "Device found at " + ip);
                                        runOnUiThread(() -> {
                                            saveDeviceIp(ip);
                                            if (deviceIpEditText != null) {
                                                deviceIpEditText.setText(ip);
                                            }
                                            wifiStatusTextView.setText("Found: " + ip);
                                            Toast.makeText(MainActivity.this,
                                                    "Device found: " + ip, Toast.LENGTH_LONG).show();
                                            isScanning = false;
                                            syncTimeToDevice();
                                            fetchDeviceStatus();
                                        });
                                    }
                                } catch (JSONException e) {
                                    Log.w(TAG, "Parse error from " + ip);
                                }
                            },
                            error -> Log.d(TAG, "No device at " + ip)
                    );
                    request.setTag("scan_" + ip);
                    request.setRetryPolicy(new DefaultRetryPolicy(2000, 0, 0f));
                    runOnUiThread(() -> requestQueue.add(request));

                    Thread.sleep(2500);
                } catch (InterruptedException e) {
                    break;
                }
            }

            runOnUiThread(() -> {
                if (isScanning) {
                    wifiStatusTextView.setText("Scan complete - not found");
                    Toast.makeText(MainActivity.this,
                            "Device not found. Check hotspot & set IP manually.",
                            Toast.LENGTH_LONG).show();
                }
                isScanning = false;
            });
        }).start();
    }

    private final Runnable refreshRunnable = new Runnable() {
        @Override
        public void run() {
            if (isResumed) {
                fetchDeviceStatus();
                long now = System.currentTimeMillis();
                if (!rtcSynced || (now - lastSyncTime) > SYNC_INTERVAL_MS) {
                    syncTimeToDevice();
                }
                refreshHandler.postDelayed(this, REFRESH_INTERVAL_MS);
            }
        }
    };

    private void startPeriodicRefresh() {
        refreshHandler.removeCallbacks(refreshRunnable);
        fetchDeviceStatus();
        refreshHandler.postDelayed(refreshRunnable, REFRESH_INTERVAL_MS);
    }

    private void stopPeriodicRefresh() {
        refreshHandler.removeCallbacks(refreshRunnable);
    }

    private void fetchDeviceStatus() {
        String url = "http://" + deviceIp + "/api/status";

        JsonObjectRequest jsonObjectRequest = new JsonObjectRequest(
                Request.Method.GET,
                url,
                null,
                response -> {
                    boolean wasDisconnected = (consecutiveErrors > 0);
                    consecutiveErrors = 0;
                    try {
                        if (response.getInt("code") == 0) {
                            JSONObject data = response.getJSONObject("data");

                            int temp = data.getInt("temp");
                            int hum = data.getInt("hum");
                            int medicine = data.getInt("medicine");
                            int light = data.getInt("light");
                            int volume = data.getInt("volume");
                            int battery = data.getInt("battery");
                            int voltage = data.getInt("voltage");
                            int led = data.getInt("led");
                            int fan = data.getInt("fan");
                            int heater = data.getInt("heater");
                            int boxOpen = data.getInt("box_open");
                            if (data.has("rtc_synced")) {
                                rtcSynced = (data.getInt("rtc_synced") == 1);
                            }

                            boxIsOpen = (boxOpen == 1);
                            updatePillBoxButton();

                            temperatureTextView.setText(getString(R.string.temperature_format, temp));
                            humidityTextView.setText(getString(R.string.humidity_format, hum));
                            medicineTextView.setText(getString(R.string.medicine_format, medicine));
                            brightnessTextView.setText(getString(R.string.brightness_format, light));
                            batteryTextView.setText(getString(R.string.battery_format, battery, voltage));
                            volumeTextView.setText(getString(R.string.volume_format, volume));

                            fanStatusTextView.setText(fan == 1 ? R.string.status_on : R.string.status_off);
                            heaterStatusTextView.setText(heater == 1 ? R.string.status_on : R.string.status_off);
                            ledStatusTextView.setText(led == 1 ? R.string.led_on : R.string.led_off);
                            boxStatusTextView.setText(boxOpen == 1 ? R.string.box_open : R.string.box_closed);
                            wifiStatusTextView.setText("Connected: " + deviceIp);

                            if (data.has("temp_high")) {
                                currentTempHigh = data.getInt("temp_high");
                                currentTempLow = data.getInt("temp_low");
                                currentLightOn = data.getInt("light_on");
                                currentLightOff = data.getInt("light_off");

                                if (tempHighEditText != null) tempHighEditText.setText(String.valueOf(currentTempHigh));
                                if (tempLowEditText != null) tempLowEditText.setText(String.valueOf(currentTempLow));
                                if (lightOnEditText != null) lightOnEditText.setText(String.valueOf(currentLightOn));
                                if (lightOffEditText != null) lightOffEditText.setText(String.valueOf(currentLightOff));
                            }

                        } else {
                            Log.w(TAG, "Server returned error: " + response.optString("msg", "unknown"));
                        }

                        if (wasDisconnected) {
                            syncTimeToDevice();
                        }
                    } catch (JSONException e) {
                        Log.e(TAG, "JSON parse error", e);
                    }
                },
                error -> {
                    consecutiveErrors++;
                    wifiStatusTextView.setText(R.string.wifi_disconnected);
                    if (error.networkResponse != null) {
                        Log.e(TAG, "HTTP error " + error.networkResponse.statusCode);
                    } else if (error.getCause() != null) {
                        Log.e(TAG, "Network error: " + error.getCause().getMessage());
                    } else {
                        Log.e(TAG, "Network error: timeout or no connection");
                    }
                    if (consecutiveErrors <= 1) {
                        Toast.makeText(MainActivity.this, R.string.connection_failed, Toast.LENGTH_SHORT).show();
                    }
                }
        );

        jsonObjectRequest.setTag(REQUEST_TAG);
        jsonObjectRequest.setRetryPolicy(new DefaultRetryPolicy(
                VOLLEY_TIMEOUT_MS,
                MAX_RETRY_COUNT,
                DefaultRetryPolicy.DEFAULT_BACKOFF_MULT
        ));
        requestQueue.add(jsonObjectRequest);
    }

    private void controlVolume(String direction) {
        String url = "http://" + deviceIp + "/api/volume/" + direction;

        StringRequest stringRequest = new StringRequest(
                Request.Method.POST,
                url,
                response -> {
                    Toast.makeText(MainActivity.this, R.string.volume_adjusted, Toast.LENGTH_SHORT).show();
                    fetchDeviceStatus();
                },
                error -> Toast.makeText(MainActivity.this, R.string.volume_adjust_failed, Toast.LENGTH_SHORT).show()
        );
        stringRequest.setTag(REQUEST_TAG);
        stringRequest.setRetryPolicy(new DefaultRetryPolicy(
                VOLLEY_TIMEOUT_MS, MAX_RETRY_COUNT, DefaultRetryPolicy.DEFAULT_BACKOFF_MULT));
        requestQueue.add(stringRequest);
    }

    private void syncTimeToDevice() {
        Calendar now = Calendar.getInstance();
        int y = now.get(Calendar.YEAR) % 100;
        int mo = now.get(Calendar.MONTH) + 1;
        int d = now.get(Calendar.DAY_OF_MONTH);
        int h = now.get(Calendar.HOUR_OF_DAY);
        int m = now.get(Calendar.MINUTE);
        int s = now.get(Calendar.SECOND);

        String url = "http://" + deviceIp + "/api/settime?y=" + y
                + "&mo=" + mo + "&d=" + d + "&h=" + h + "&m=" + m + "&s=" + s;

        StringRequest stringRequest = new StringRequest(
                Request.Method.GET,
                url,
                response -> {
                    Log.i(TAG, "Time synced to device: " + h + ":" + m + ":" + s);
                    rtcSynced = true;
                    lastSyncTime = System.currentTimeMillis();
                },
                error -> Log.w(TAG, "Failed to sync time to device")
        );
        stringRequest.setTag(REQUEST_TAG);
        stringRequest.setRetryPolicy(new DefaultRetryPolicy(
                VOLLEY_TIMEOUT_MS, MAX_RETRY_COUNT, DefaultRetryPolicy.DEFAULT_BACKOFF_MULT));
        requestQueue.add(stringRequest);
    }

    private void showTimePicker(final int reminderId) {
        final Calendar c = Calendar.getInstance();
        int hour = c.get(Calendar.HOUR_OF_DAY);
        int minute = c.get(Calendar.MINUTE);

        TimePickerDialog timePickerDialog = new TimePickerDialog(this,
                (view, hourOfDay, minuteOfDay) -> {
                    switch (reminderId) {
                        case 1:
                            reminderHour1 = hourOfDay;
                            reminderMinute1 = minuteOfDay;
                            setAlarm(1, hourOfDay, minuteOfDay);
                            updateReminderText(1);
                            break;
                        case 2:
                            reminderHour2 = hourOfDay;
                            reminderMinute2 = minuteOfDay;
                            setAlarm(2, hourOfDay, minuteOfDay);
                            updateReminderText(2);
                            break;
                        case 3:
                            reminderHour3 = hourOfDay;
                            reminderMinute3 = minuteOfDay;
                            setAlarm(3, hourOfDay, minuteOfDay);
                            updateReminderText(3);
                            break;
                    }
                    syncReminderToDevice(reminderId - 1, hourOfDay, minuteOfDay);
                }, hour, minute, true);
        timePickerDialog.show();
    }

    private void syncReminderToDevice(int index, int hour, int minute) {
        String url = "http://" + deviceIp + "/api/reminder?set=" + index + "&h=" + hour + "&m=" + minute;

        StringRequest stringRequest = new StringRequest(
                Request.Method.GET,
                url,
                response -> Log.i(TAG, "Reminder " + index + " synced to device"),
                error -> Log.w(TAG, "Failed to sync reminder " + index + " to device")
        );
        stringRequest.setTag(REQUEST_TAG);
        stringRequest.setRetryPolicy(new DefaultRetryPolicy(
                VOLLEY_TIMEOUT_MS, MAX_RETRY_COUNT, DefaultRetryPolicy.DEFAULT_BACKOFF_MULT));
        requestQueue.add(stringRequest);
    }

    private void setAlarm(int reminderId, int hour, int minute) {
        Calendar calendar = Calendar.getInstance();
        calendar.set(Calendar.HOUR_OF_DAY, hour);
        calendar.set(Calendar.MINUTE, minute);
        calendar.set(Calendar.SECOND, 0);

        if (calendar.getTimeInMillis() < System.currentTimeMillis()) {
            calendar.add(Calendar.DAY_OF_YEAR, 1);
        }

        switch (reminderId) {
            case 1:
                alarmManager1 = (AlarmManager) getSystemService(Context.ALARM_SERVICE);
                Intent intent1 = new Intent(this, ReminderReceiver.class);
                intent1.putExtra("reminderId", 1);
                alarmIntent1 = PendingIntent.getBroadcast(this, 1, intent1, PendingIntent.FLAG_IMMUTABLE);
                alarmManager1.setRepeating(AlarmManager.RTC_WAKEUP, calendar.getTimeInMillis(),
                        AlarmManager.INTERVAL_DAY, alarmIntent1);
                break;
            case 2:
                alarmManager2 = (AlarmManager) getSystemService(Context.ALARM_SERVICE);
                Intent intent2 = new Intent(this, ReminderReceiver.class);
                intent2.putExtra("reminderId", 2);
                alarmIntent2 = PendingIntent.getBroadcast(this, 2, intent2, PendingIntent.FLAG_IMMUTABLE);
                alarmManager2.setRepeating(AlarmManager.RTC_WAKEUP, calendar.getTimeInMillis(),
                        AlarmManager.INTERVAL_DAY, alarmIntent2);
                break;
            case 3:
                alarmManager3 = (AlarmManager) getSystemService(Context.ALARM_SERVICE);
                Intent intent3 = new Intent(this, ReminderReceiver.class);
                intent3.putExtra("reminderId", 3);
                alarmIntent3 = PendingIntent.getBroadcast(this, 3, intent3, PendingIntent.FLAG_IMMUTABLE);
                alarmManager3.setRepeating(AlarmManager.RTC_WAKEUP, calendar.getTimeInMillis(),
                        AlarmManager.INTERVAL_DAY, alarmIntent3);
                break;
        }

        Toast.makeText(this, getString(R.string.alarm_set, reminderId), Toast.LENGTH_SHORT).show();
    }

    private void updateReminderText(int reminderId) {
        switch (reminderId) {
            case 1:
                if (reminderHour1 != -1 && reminderMinute1 != -1) {
                    reminderTextView1.setText(getString(R.string.reminder_time, 1, String.format(Locale.getDefault(), "%02d:%02d", reminderHour1, reminderMinute1)));
                } else {
                    reminderTextView1.setText(getString(R.string.reminder_none, 1));
                }
                break;
            case 2:
                if (reminderHour2 != -1 && reminderMinute2 != -1) {
                    reminderTextView2.setText(getString(R.string.reminder_time, 2, String.format(Locale.getDefault(), "%02d:%02d", reminderHour2, reminderMinute2)));
                } else {
                    reminderTextView2.setText(getString(R.string.reminder_none, 2));
                }
                break;
            case 3:
                if (reminderHour3 != -1 && reminderMinute3 != -1) {
                    reminderTextView3.setText(getString(R.string.reminder_time, 3, String.format(Locale.getDefault(), "%02d:%02d", reminderHour3, reminderMinute3)));
                } else {
                    reminderTextView3.setText(getString(R.string.reminder_none, 3));
                }
                break;
        }
    }

    public static class ReminderReceiver extends android.content.BroadcastReceiver {
        @Override
        public void onReceive(Context context, Intent intent) {
            int reminderId = intent.getIntExtra("reminderId", 1);
            Toast.makeText(context, context.getString(R.string.reminder_notify, reminderId), Toast.LENGTH_LONG).show();
        }
    }
}
