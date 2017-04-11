# linkit-one-aws-twilio-demo
We demonstrate sending, receiving, and replying to SMS and MMSes with the LinkIt ONE and Twilio.

## AWS Account Required

We're using AWS IoT as a middleman to improve the security of our IoT setup.  Instead of sending SMSes and MMSes from the board, we're first publishing to MQTT topics on IoT (using TLS) before we forward them to Twilio.  We create certificates in AWS IoT which are then revokable if a board in the field is compromised.

See our article here for the matching Lambda function and the full details on the rule to trigger it:

.....

## Build example:

For sending SMS/MMS messages, use IoT on the 'twilio' channel as a trigger with the SQL of: 
"SELECT * FROM 'twilio' WHERE Type='Outgoing'".  

You must not use SQL version 2016-3-23, it will not work with null-terminated strings!

For receiving messages, use API Gateway and pass through form parameters.  Return the empty response to Twilio with application/xml.  The 'response' will come from a new 'send' originating on the LinkIt.


### LinkIt
<pre>
git clone https://github.com/twilio/linkit-one-aws-twilio-demo.git
</pre>

#### Install the following packages with the Arduino Package Manager:
* ArduinoJSON by Benoit Blanchon
#### Install using the instructions in the Repo:
* [AWS LinkIt Demo](https://github.com/twilio/linkit-one-aws-twilio-demo)

Open this .ino file in Arduino IDE (if you haven't yet, you also need to [set up your LinkIt](https://docs.labs.mediatek.com/resource/linkit-one/en/getting-started) to connect)

### Edit the credentials, change desired settings
Compile and Upload to the board!

## Run example:
(Should send an MMS automatically when uploaded to LinkIt or power is restored.  Busy waits for you to connect to Serial first.)

## Motivations

The show the skeleton for a Twilio starter application using an LinkIt connected to WiFi or GPRS with AWS IoT, Lambda, and API Gateway handling the backend.  With luck, this gets you closer to your goal of an embedded _Twilio Thing_!

## Meta & Licensing

* [MIT License](http://www.opensource.org/licenses/mit-license.html)
* Lovingly crafted by Twilio Developer Education.
