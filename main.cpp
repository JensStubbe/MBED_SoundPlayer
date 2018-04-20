#include "mbed.h"
 
#define USE_LCD 0

#if USE_LCD
#include "C12832.h"

// the actual pins are defined in mbed_app.json and can be overridden per target
C12832 lcd(LCD_MOSI, LCD_SCK, LCD_MISO, LCD_A0, LCD_NCS);

#define logMessage lcd.cls();lcd.printf

#else

#define logMessage printf

#endif

#define MQTTCLIENT_QOS2 1

#include "easy-connect.h"
#include "MQTTNetwork.h"
#include "MQTTmbed.h"
#include "MQTTClient.h"
#include "Mail.h"
#include "Thread.h"
#include "mbed.h"
#include "stdlib.h"

int arrivedcount = 0;
PwmOut soundyMcSoundface(p26);
int frequency;
double period;

// Define what the mail is going to look like
typedef struct{
    int frequency;
}mail_t;

Mail<mail_t, 16> mail_box;//Make a reference to the mail block with a queue size of 16

//Send a mail object from messageArrived to generateSound
void messageArrived(MQTT::MessageData& md){
        mail_t *mail = mail_box.alloc();//Allocate the mail memory block
        
        MQTT::Message &message = md.message;
        logMessage("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", message.qos, message.retained, message.dup, message.id);
        logMessage("Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
        ++arrivedcount;//Count the amount of messages
        int freq = atoi((char*)message.payload);//Message to integer conversion
        mail->frequency = freq; //Insert the data in the mail block
        mail_box.put(mail);//Insert the mail block in the array
    }
//Receive a mail object from messageArrived and generate a sound with the frequency in that mail object
void generateSound(){
        while(true){
            osEvent evt = mail_box.get();
            if(evt.status == osEventMail){//If there's an update in the data in the mail
                mail_t *mail = (mail_t*)evt.value.p;//Make a pointer to the mail object, so the contents can be accessed
                
                //Calculate period
                period = (1.0/(mail->frequency));      
                soundyMcSoundface.period(period);//Use the PwmOut period attribute to set the period of the signal
                
                //Generate a sound
                soundyMcSoundface = 0.5;//Duty-cycle 50%
                wait(1);//Interval between sound pulses
                soundyMcSoundface = 0;//Duty-cycle 0%
                wait(1);//Interval between sound pulses
                
                mail_box.free(mail);//Free up space in the mail queue
                }
        }
    }
int main(int argc, char* argv[])
{
    float version = 0.6;
    char* topic = "mbedsound";

    logMessage("HelloMQTT: version is %.2f\r\n", version);

    NetworkInterface* network = easy_connect(true);
    if (!network) {
        return -1;
    }

    MQTTNetwork mqttNetwork(network);

    MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);

    const char* hostname = "mqtt.labict.be";
    int port = 1883;
    logMessage("Connecting to %s:%d\r\n", hostname, port);
    int rc = mqttNetwork.connect(hostname, port);
    if (rc != 0)
        logMessage("rc from TCP connect is %d\r\n", rc);

    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = "mbedsound";
    data.username.cstring = "testuser";
    data.password.cstring = "testpassword";
    if ((rc = client.connect(data)) != 0)
        logMessage("rc from MQTT connect is %d\r\n", rc);

    if ((rc = client.subscribe(topic, MQTT::QOS2, messageArrived)) != 0)
        logMessage("rc from MQTT subscribe is %d\r\n", rc);

    MQTT::Message message;

    Thread thread(generateSound);

    while(true){
        client.yield(100);
        }

    if ((rc = client.unsubscribe(topic)) != 0)
        logMessage("rc from unsubscribe was %d\r\n", rc);

    if ((rc = client.disconnect()) != 0)
        logMessage("rc from disconnect was %d\r\n", rc);

    mqttNetwork.disconnect();

    logMessage("Version %.2f: finish %d msgs\r\n", version, arrivedcount);

    return 0;
}
