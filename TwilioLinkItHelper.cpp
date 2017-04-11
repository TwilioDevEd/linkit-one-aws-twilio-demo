#include "TwilioLinkItHelper.hpp"

/* 
 *  Very basic constructor for the LinkIt Helper. The connection is done
 *  elsewhere, so we just initialize the variables we need to
 *  wrap all of the functions as well as perform Twilio-related actions.
 */
TwilioLinkitHelper::TwilioLinkitHelper(
        const int&                              mqtt_port_in,
        const String&                           mqtt_host_in,
        const String&                           mqtt_client_id_in,
        const String&                           mqtt_thing_name_in,
        const String&                           root_ca_name_in,
        const String&                           iot_ca_fname_in,
        const String&                           iot_key_fname_in,
        const bool&                             wifi_used_in,
        void                                    (*disc_in)(void)
)       : mqtt_port(mqtt_port_in)
        , mqtt_host(mqtt_host_in)
        , mqtt_client_id(mqtt_client_id_in)
        , mqtt_thing_name(mqtt_thing_name_in)
        , root_ca_name(root_ca_name_in)
        , iot_ca_fname(iot_ca_fname_in)
        , iot_key_fname(iot_key_fname_in)
        , wifi_used(wifi_used_in)
        , __disc(disc_in)
{}

/* 
 * Send an SMS/MMS from the LinkIt ONE Through AWS IoT and Lambda.
 * 
 * Creates a JSON object set up for our Twilio Lambda function, then converts 
 * to a C String and publishes it to a topic.
 */
void TwilioLinkitHelper::send_twilio_message(
        const String& topic,
        const String& to_number,
        const String& from_number, 
        const String& message_body,
        const String& picture_url
)
{       
        if (message_body.length() + picture_url.length() > \
                (maxMQTTpackageSize/2) ) {
                // Be mindful of your message size on the LinkIt; 
                // break it up into multiple messages.
                // We're being extra safe here with the max size/2, you may
                // need to tweak it for your purposes.
                return;
        }
        
        StaticJsonBuffer<maxMQTTpackageSize> jsonBuffer;
        JsonObject& root = jsonBuffer.createObject();
        root["To"] = to_number.c_str();
        root["From"] = from_number.c_str();
        root["Type"] = "Outgoing";
        root["Body"] = message_body.c_str();
        if (picture_url.length() > 0) {
                root["Image"] = picture_url.c_str();
        }

        // No unique_ptr with this compiler.
        //std::unique_ptr<char []> buffer(new char[maxMQTTpackageSize]());

        char buffer[maxMQTTpackageSize];
        root.printTo(buffer, maxMQTTpackageSize);
        publish_to_topic(topic.c_str(), buffer);
}

/* 
 *  Wrapper to yield to the AWS Watchdog, which checks requests,
 *  pings, and does other functions to keep the connection
 *  alive.
 */
void TwilioLinkitHelper::handle_requests() {
        aws_iot_mqtt_yield(2000);
}

/* Subscribe to a MQTT topic */
void TwilioLinkitHelper::subscribe_to_topic(
        char* topic, 
        int32_t (*callbackHandler)(MQTTCallbackParams) 
) 
{
        MQTTSubscribeParams subParams;
        subParams.mHandler = callbackHandler;
        subParams.pTopic = topic;
        subParams.qos = QOS_0;
        int rc = aws_iot_mqtt_subscribe(&subParams);
        if (NONE_ERROR != rc) {
                Serial.println("Failed to subscribe.");
        } else {
                Serial.println("MQTT subscribed");
        }
        Serial.flush();
}

/* Publish to a MQTT topic */
void TwilioLinkitHelper::publish_to_topic(
        const char* topic, 
        const char* message 
) 
{
        // Message setup
        MQTTMessageParams Msg;
        Msg.qos = QOS_0;
        Msg.isRetained = false;
        Msg.pPayload = (void *)message;
        Msg.PayloadLen = strlen(message);

        // Parameter setup
        MQTTPublishParams Params;
        Params.pTopic = const_cast<char*>(topic);
        Params.MessageParams = Msg;

        int rc = aws_iot_mqtt_publish(&Params);
        if (rc != NONE_ERROR) {
                Serial.print("rc from MQTT subscribe is ");
                Serial.println(rc);
                return;
        }
}


/* Publish to a MQTT topic */
void TwilioLinkitHelper::publish_to_topic(
        const char* topic, 
        const String& message 
) 
{

        char buf[message.length() + 1];
        strcpy(buf, message.c_str());
        
        // Message setup
        MQTTMessageParams Msg;
        Msg.qos = QOS_0;
        Msg.isRetained = false;
        Msg.pPayload = (void *)buf;
        Msg.PayloadLen = strlen(buf);

        // Parameter setup
        MQTTPublishParams Params;
        Params.pTopic = const_cast<char*>(topic);
        Params.MessageParams = Msg;

        int rc = aws_iot_mqtt_publish(&Params);
        if (rc != NONE_ERROR) {
                Serial.print("rc from MQTT subscribe is ");
                Serial.println(rc);
                return;
        }
}

/*
 * Populate the MQTTConnectParams structure and then call
 * aws_iot_mqtt_connect to connect to AWS IoT
 */
boolean  TwilioLinkitHelper::start_mqtt(void* ctx)
{
        MQTTConnectParams connectParams;
        VMINT port = mqtt_port;
        connectParams = MQTTConnectParamsDefault;
        connectParams.KeepAliveInterval_sec = 10;
        connectParams.isCleansession = true;
        connectParams.MQTTVersion = MQTT_3_1_1;
        connectParams.port = port;
        connectParams.isWillMsgPresent = false;

        // We avoided C-strings as long as we could...
        connectParams.pRootCALocation =                \
                const_cast<char*>(root_ca_name.c_str());
        connectParams.pDeviceCertLocation =            \
                const_cast<char*>(iot_ca_fname.c_str());
        connectParams.pDevicePrivateKeyLocation =      \
                const_cast<char*>(iot_key_fname.c_str());
        connectParams.pClientID =                      \
                const_cast<char*>(mqtt_thing_name.c_str());
        connectParams.pHostURL =                       \
                const_cast<char*>(mqtt_host.c_str());
        
        connectParams.mqttCommandTimeout_ms = 20000;
        connectParams.tlsHandshakeTimeout_ms = 10000;
        connectParams.isSSLHostnameVerify = true;
        connectParams.disconnectHandler = __disc;

        int rc = aws_iot_mqtt_connect(&connectParams);
        if (rc != NONE_ERROR) {
                Serial.println("Error in connecting...");
        }
        return true;
}


/*
 * DNS function to lookup the domain name based upon whether we are on WiFi
 * or GPRS with the Twilio service.
 */
boolean TwilioLinkitHelper::wifiResolveDomainName(void *userData)
{
        VMCHAR *domainName = (VMCHAR *)userData;
        vm_soc_dns_result dns;
        IN_ADDR addr;
        VMINT resolveState;
        if (wifi_used){
                resolveState = vm_soc_get_host_by_name(
                                        VM_TCP_APN_WIFI,
                                        domainName,
                                        &dns,
                                        &linkitaws::__wifiResolveCallback
                               );
                Serial.flush();
        } else {
                Serial.flush();
                resolveState = vm_soc_get_host_by_name(
                                        6,
                                        domainName,
                                        &dns,
                                        &linkitaws::__wifiResolveCallback
                               );
        Serial.flush();
        }
                           
        if (resolveState > 0) {
                return false;
        }

        switch (resolveState) {
                case VM_E_SOC_SUCCESS:
                        addr.S_un.s_addr = dns.address[0];
                        CONNECT_IP_ADDRESS = inet_ntoa(addr);
                        Serial.print("IP address is ");
                        Serial.println(CONNECT_IP_ADDRESS);
                        return true;
                case VM_E_SOC_WOULDBLOCK:
                        // Asked to wait, but return so we don't
                        // block on the network.
                        return false;
                case VM_E_SOC_INVAL:  
                        // invalid arguments: null domain_name, etc.
                case VM_E_SOC_ERROR:  
                        // unspecified error
                case VM_E_SOC_LIMIT_RESOURCE:  
                        // socket resources not available
                case VM_E_SOC_INVALID_ACCOUNT:  
                        // invalid data account id
                        return true;
        }
}

/* Bearer open wrapper. */
boolean TwilioLinkitHelper::bearer_open(void* ctx)
{
        if (wifi_used) {
                g_bearer_hdl = vm_bearer_open(
                                        VM_BEARER_DATA_ACCOUNT_TYPE_WLAN,  
                                        NULL,
                                        linkitaws::__bearer_callback
                               );
         } else {
                g_bearer_hdl = vm_bearer_open(
                                        VM_APN_USER_DEFINE,  
                                        NULL,
                                        linkitaws::__bearer_callback
                               );
        }
    
        if(g_bearer_hdl >= 0) {
                return true;
        }
        return false;
}

namespace linkitaws
{

        
VMINT __wifiResolveCallback(vm_soc_dns_result *pDNS)
{
        IN_ADDR addr;
        addr.S_un.s_addr = pDNS->address[0];
        CONNECT_IP_ADDRESS = inet_ntoa(addr);
        LTask.post_signal();
        return 0;
}

void __bearer_callback(
    VMINT handle,
    VMINT event,
    VMUINT data_account_id,
    void *user_data
)
{
        if (VM_BEARER_WOULDBLOCK == g_bearer_hdl) {
                g_bearer_hdl = handle;
        }
    
        switch (event) {
                case VM_BEARER_DEACTIVATED:
                        break;
                case VM_BEARER_ACTIVATING:
                        break;
                case VM_BEARER_ACTIVATED:
                        LTask.post_signal();
                        break;
                case VM_BEARER_DEACTIVATING:
                        break;
                default:
                        break;
    }
    
}


}

