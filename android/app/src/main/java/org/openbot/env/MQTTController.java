package org.openbot.env;

import android.content.Context;
import android.util.Log;
import android.widget.Toast;

import org.eclipse.paho.android.service.MqttAndroidClient;
import org.eclipse.paho.client.mqttv3.IMqttActionListener;
import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken;
import org.eclipse.paho.client.mqttv3.IMqttToken;
import org.eclipse.paho.client.mqttv3.MqttCallback;
import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.eclipse.paho.client.mqttv3.MqttMessage;
import org.json.JSONObject;

import java.io.UnsupportedEncodingException;

public class MQTTController {

    {
        handleBotEvents();
    }

    private static final String TAG = "MQTTController";

    private static final String SUB_MQTT_TOPIC = "openbot_cmd";

    private static final String PUB_MQTT_TOPIC = "openbot_state";

    private static final String clientId = MqttClient.generateClientId();

    private static MqttAndroidClient client;

    private static Context context;


    public void connect(Context context) {
        this.context = context;
        if(client != null && client.isConnected()) return;
        client = new MqttAndroidClient(context, "tcp://192.168.1.121:1883", clientId);
        try {
            IMqttToken token = client.connect();
            token.setActionCallback(new IMqttActionListener() {
                @Override
                public void onSuccess(IMqttToken asyncActionToken) {
                    Log.d(TAG, "MQTT链接成功");
                    Toast.makeText(context, "MQTT链接成功", Toast.LENGTH_SHORT).show();
                    try {
                        IMqttToken subClient = client.subscribe(SUB_MQTT_TOPIC, 1);
                        subClient.setActionCallback(new IMqttActionListener() {
                            @Override
                            public void onSuccess(IMqttToken asyncActionToken) {
                                client.setCallback(payloadCallback);
                                Log.d(TAG, "MQTT主题订阅成功");
                                Toast.makeText(context, "MQTT主题订阅成功", Toast.LENGTH_SHORT).show();
                            }

                            @Override
                            public void onFailure(IMqttToken asyncActionToken, Throwable exception) {
                                Log.d("mqtt", "sub onFailure");
                            }
                        });
                    } catch (MqttException e) {
                        e.printStackTrace();
                    }
                }

                @Override
                public void onFailure(IMqttToken asyncActionToken, Throwable exception) {
                    // Something went wrong e.g. connection timeout or firewall problems
                    Log.d("mqtt", "onFailure");
                }
            });
        } catch (MqttException e) {
            e.printStackTrace();
        }
    }

    public void disconnect() {
        try {
            if (client != null && client.isConnected()){
                client.disconnect();
            }
        } catch (MqttException e) {
            e.printStackTrace();
        }
    }

    public void send(JSONObject info) {
        if(client == null) return;
        String payload = info.toString();
        try {
            Log.d(TAG, "发送信息: " + payload);
            IMqttDeliveryToken publish = client.publish(PUB_MQTT_TOPIC, new MqttMessage(payload.getBytes("UTF-8")));
            Toast.makeText(context, "发送信息: " + publish.getMessage(), Toast.LENGTH_SHORT).show();
        } catch (MqttException e) {
            e.printStackTrace();
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
        }
    }

    public static void publishToServer(String data){
        if(client == null) return;
        try {
            client.publish(PUB_MQTT_TOPIC, new MqttMessage(data.getBytes()));
        } catch (MqttException e) {
            e.printStackTrace();
        }
    }

    private final MqttCallback payloadCallback = new MqttCallback() {
        @Override
        public void connectionLost(Throwable cause) {

        }

        @Override
        public void messageArrived(String topic, MqttMessage message) throws Exception {
            Log.d(TAG, "收到消息: " + message.toString());
            ControllerToBotEventBus.emitEvent(new JSONObject(message.toString()));
        }

        @Override
        public void deliveryComplete(IMqttDeliveryToken token) {

        }
    };

    public boolean isConnected() {
        return client!=null && client.isConnected();
    }

    private void handleBotEvents() {
        BotToControllerEventBus.getProcessor().subscribe(event -> send(event));
    }
}
