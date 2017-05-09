/*
 * Twilio send and receive SMS/MMS messages through AWS IoT, Lambda, and API 
 * Gateway with the LinkIt ONE Mediatek development board.
 * 
 * This application demonstrates sending out an SMS or MMS from a LinkIt ONE
 * board through AWS IoT, and receiving messages via a MQTT topic
 * subscribed to by the board.
 * 
 * 
 * This code owes much thanks to MediaTek Labs, who have a skeleton up on
 * how to connect to AWS IoT here: 
 * 
 * https://github.com/MediaTek-Labs/aws_mbedtls_mqtt
 *
 * You'll need to install it to run our code.
 * 
 * License: This code, MIT, AWS Code: Apache (http://aws.amazon.com/apache2.0)
 */
// Handle incoming messages
#include <ArduinoJson.h>

/* Scheduling and connectivity from LinkIt ONE Board */
#include <LTask.h>
#include <LWiFi.h>
#include <LWiFiClient.h>
#include <LGPRS.h>

// Local Includes
#include "TwilioLinkItHelper.hpp"

/* CONFIGURATION:
 *  
 * Start here for configuration variables.  First up, all of the AWS IoT
 * configuration you need.  You'll need to upload the private key,
 * certificate, and root key to the board.
 * 
 */
String           AWS_IOT_MQTT_HOST =             "SOMETHING.iot.REGION.amazonaws.com";
String           AWS_IOT_MQTT_CLIENT_ID =        "LinkItONE_Twilio";
String           AWS_IOT_ROOT_CA_FILENAME =      "G5.pem";
String           AWS_IOT_CERTIFICATE_FILENAME =  "SOMETHING-certificate.pem";
String           AWS_IOT_PRIVATE_KEY_FILENAME =  "SOMETHING-private.pem";
String           AWS_IOT_TOPIC_NAME = "$aws/things/mtk_test_mf/shadow/update";


// Don't edit this one unless you also edit the /aws_iot_config.h file!
// We've left the name for ease of plug and play, but might eventually 
// rework the park of the library.
String           AWS_IOT_MY_THING_NAME =         "mtk_test_mf"; 

/* Should we use WiFi or GPRS?  'true' for WiFi, 'false' for GPRS */
boolean WIFI_USED =                     true;

/* 
 *  Now, the _Twilio specific_ configuration you need to send an outgoing
 *  SMS or MMS. 
 *  
 *  In production, you may want to pass these over MQTT (or a similar 
 *  channel) to change settings on devices in the field.
 */

String your_device_number        = "+18005551212"; // Twilio # you own
String number_to_text            = "+18005551212"; // Perhaps your cellphone?
String your_sms_message          = "Can you draw this owl?";
String optional_image_path       = "https://upload.wikimedia.org/wikipedia/commons/thumb/9/98/GreatHornedOwl-Wiki.jpg/800px-GreatHornedOwl-Wiki.jpg";
String linkit_image_path         = "http://com.twilio.prod.twilio-docs.s3.amazonaws.com/quest/programmable_wireless/code/LinkIt_Teaser.jpg";

/* Optional Settings.  You probably do not need to change these. */
String twilio_topic        = "twilio";
const int mqtt_tls_port = 8883;

/* Friendly WiFi Network details go here.  Auth choices:
 *  * LWIFI_OPEN 
 *  * LWIFI_WPA
 *  * LWIFI_WEP
 */
#define WIFI_AP "FRIENDLY_NETWORK"
#define WIFI_PASSWORD "PASSWORD_TO_IT"
#define WIFI_AUTH LWIFI_WPA

/* 
 *  Twilio GPRS Settings here - you should not have to change these to 
 *  use a Twilio Programmable Wireless SIM Card.  Make sure your card 
 *  is registered, provisioned, and activated if you have issues.
 */
#define GPRS_APN "wireless.twilio.com"
#define GPRS_USERNAME NULL
#define GPRS_PASSWORD NULL


/* Workaround to remove a Macro from mbedtls */
#ifdef connect
#undef connect
#endif

typedef int32_t mqtt_callback(MQTTCallbackParams);

// Global Twilio Lambda helper
void disconnect_function();
TwilioLinkitHelper helper(
        mqtt_tls_port,
        AWS_IOT_MQTT_HOST,
        AWS_IOT_MQTT_CLIENT_ID,
        AWS_IOT_MY_THING_NAME,
        AWS_IOT_ROOT_CA_FILENAME,
        AWS_IOT_CERTIFICATE_FILENAME,
        AWS_IOT_PRIVATE_KEY_FILENAME,
        WIFI_USED,
        disconnect_function
);

struct MQTTSub {
        char* topic;
        mqtt_callback* callback; 
};

struct MQTTPub {
        
};


/* 
 * Our Twilio message handling callback.  This is passed as a callback function
 * when we subscribe to the Twilio topic, and will handle any incoming messages
 * on that topic.
 * 
 * You'll want to add your own application logic inside of here.  For this 
 * demo, we'll take the first 160 characters of the message body and send it 
 * back in reverse and optionally write to a serial connection.
 * 
 * No, this doesn't handle unicode - prepare for weird results if you send
 * any non-ASCII characters!
 */
int32_t handle_incoming_message_twilio(MQTTCallbackParams params)
{     
        MQTTMessageParams message = params.MessageParams;
      
        // We don't have std::unique_ptr
        //std::unique_ptr<char []> msg(new char[message.payloadlen+1]());

        char msg[message.PayloadLen+1];
        
        memcpy (msg,message.pPayload,message.PayloadLen);
        StaticJsonBuffer<maxMQTTpackageSize> jsonBuffer;
        JsonObject& root = jsonBuffer.parseObject(msg);
        
        String to_number           = root["To"];
        String from_number         = root["From"];
        String message_body        = root["Body"];
        String message_type        = root["Type"];

        // Only handle messages to the ESP's number
        if (strcmp(to_number.c_str(), your_device_number.c_str()) != 0) {
                return 0;
        }
        // Only handle incoming messages
        if (!message_type.equals("Incoming")) {
                return 0;
        }

        // Basic demonstration of rejecting a message based on which 'device'
        // it is sent to, if devices get one Twilio number each.
        Serial.print("\n\rNew Message from Twilio!");
        Serial.print("\r\nTo: ");
        Serial.print(to_number);
        Serial.print("\n\rFrom: ");
        Serial.print(from_number);
        Serial.print("\n\r");
        Serial.print(message_body);
        Serial.print("\n\r");

        // Now reverse the body and send it back.

        
        // std::unique_ptr<char []> return_body(new char[161]());
        
        char return_body[161];
        int16_t r = message_body.length()-1, i = 0;

        // Lambda will limit body size, but we should be defensive anyway.
        // uint16_t is fine because 'maxMQTTpackageSize' limits the total 
        // incoming message size.
        
        // 160 characters is _index_ 159.
        r = (r < 160) ? r : 159; 
        return_body[r+1] = '\0';
        while (r >= 0) {
                return_body[i++] = message_body[r--];
        }
        
        Serial.print(return_body);
        Serial.print("\n\r");
        
        // Send a message, reversing the to and from number
        helper.send_twilio_message(
                twilio_topic,
                from_number,
                to_number, 
                String(return_body),
                String(linkit_image_path)
        );

        return 0;
}


/* 
 *  Main setup function.
 *  
 *  Here we connect to either GPRS or WiFi, then AWS IoT.  We then subscribe
 *  to some channels and send out a SMS (or MMS) via MQTT and our 
 *  Lambda backend.
 */
void setup()
{
        LTask.begin();
        
        Serial.begin(115200);
        while(!Serial) {
                // Busy wait on Serial Monitor connection.
                delay(100);
        }

  
        if (WIFI_USED){
                LWiFi.begin();
                Serial.println("Connecting to AP");
                Serial.flush();
                while (!LWiFi.connect(
                                WIFI_AP, 
                                LWiFiLoginInfo(WIFI_AUTH, WIFI_PASSWORD)
                      )
                ) {
                        Serial.print(".");
                        Serial.flush();
                        delay(500);
                }
        } else {  
                Serial.println("Connecting to GPRS");
                Serial.flush();
                while (!LGPRS.attachGPRS(
                                GPRS_APN, 
                                GPRS_USERNAME, 
                                GPRS_PASSWORD
                      )
                ) {
                        Serial.println(".");
                        Serial.flush();
                        delay(500);
                }
        }

        Serial.println("Connected!");

        //char HostAddress[255] = AWS_IOT_MQTT_HOST;
        VMINT port = mqtt_tls_port;
        CONNECT_PORT = port;
        
        LTask.remoteCall(&__wifi_dns, (void*)AWS_IOT_MQTT_HOST.c_str());
        LTask.remoteCall(&__bearer_open, NULL);
        LTask.remoteCall(&__mqtt_start, NULL);

        MQTTSub sub_struct;
        
        sub_struct.topic = const_cast<char*>(twilio_topic.c_str());
                
        sub_struct.callback = handle_incoming_message_twilio;
        
        LTask.remoteCall(&__sub_mqtt, (void*)&sub_struct);
        
        //char mqtt_message[30];
        //sprintf(mqtt_message, "Hello World - Love, Twilio");
        Serial.println("publish_MQTT go");
        //linkitaws::publish_MQTT("twilio", mqtt_message);
}

/*  
 *   Disconnect callback function - what do you want to do when it goes down?
 *   
*/
void disconnect_function() {

        Serial.println("Oh no, we disconnected!");
        delay(10000);
        
        // We could just reconnect, but in this case it's probably better to 
        // power cycle in a place with better signal.
        
        // helper.start_mqtt()
        
        while(1);
}

/*  
 *   Every time through the main loop we will spin up a new thread on the 
 *   LinkIt to perform our watchdog tasks in the background.  Insert everything
 *   you want to perform over and over again until infinity (or power 
 *   loss!) here.
*/
boolean main_thread(void* user_data) {
        static bool sent_intro = false;
        
        Serial.println("Thread...");
        Serial.flush();

        if (!sent_intro) {
                helper.send_twilio_message(
                                twilio_topic,
                                number_to_text,
                                your_device_number, 
                                String(your_sms_message),
                                String(optional_image_path)
                );
                sent_intro = true;
        }

        // We need to handle any incoming messages from AWS.
        // Don't remove this call from the loop.
        helper.handle_requests();
        delay(1000);
}

/*
 * The standard Arduino loop.  You don't want to put anything here; just
 * spin up new threads since the MediaTek can handle these async.
 * 
 * This example just creates a new thread which calls main_thread()
 * every 3 seconds.
 */
void loop() {
        Serial.flush();
        /* We can do our main tasks in a new thread with LTask */
        LTask.remoteCall(main_thread, NULL);
        delay(3000);
}

/* 
 *  Trampolines/thunks are the easiest way to pass these class methods.  
 *  We don't have std::bind() or boost::bind()!
 */
inline boolean __bearer_open(void* ctx) 
{ return helper.bearer_open(ctx); }

inline boolean __mqtt_start(void* ctx)  
{ return helper.start_mqtt(ctx); }

inline boolean __wifi_dns(void* ctx)    
{ return helper.wifiResolveDomainName(ctx); }

inline boolean __sub_mqtt(void *ctx)   
{ 
        MQTTSub* sub_struct = (MQTTSub*)ctx; char* topic = sub_struct->topic;
        mqtt_callback* callback = sub_struct->callback;
        helper.subscribe_to_topic(topic, callback);
        return true;
}
