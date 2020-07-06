
//#program once //헤더파일의 중복방지
#include "auth.h"
#include "images.h"
#include "display.h"

#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include <stdio.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_Sensor.h>
//#include <Adafruit_TSL2561_U.h>
#include <SSD1306Wire.h> // legacy include: `#include "SSD1306.h"`
#include <OLEDDisplayUi.h>
#include "Pushover.h"
#include <Timer.h>


// time
#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <coredecls.h>                  // settimeofday_cb()


// istanziare la libreria Pushover fornendo: API Token, User Key
Pushover pushover_api = Pushover("az1sgrbpkwd4pov6amjdhmmr1vr8wd","utznc1wxgkp94t6k3kcastr45feb59");

// Adjust according to your language
const String WDAY_NAMES[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const String MONTH_NAMES[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

#define TZ              8       // (utc+) TZ in hours
#define DST_MN          60      // use 60mn for summer time in some countries
#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)
time_t now;

Timer t;
Timer t1;

WiFiClient espClient;
PubSubClient client(espClient);


String TEMPSET = "35°c";
String TEMP =  "initialize";
String HUM =  "initialize";

int temperature = 0;
int tempconfig = 0;
int HumidValue = 0;


//OLED DISPLAY
String DISP_DEBUG = "initialize";
String RIGHT_TOP = "initialize";
String ESTOP_UI = "ON";
String DISP_BUZZER = "B";
String DISP_LED = "L";
//MQTT Configure
char message_buff[20];


//OUTPUT PINS
#define LED_PIN     12 //D6
#define BUZZER_PIN  13 //D7

//INPUT PINS
#define ESTOP_PIN   14 //D5

// Variables will change:
boolean ESTOP_flag = false;
int led_flag = true;
int buzzer_flag = true;           // the current state of the output pin



// the following variables are unsigned long's because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
unsigned long lastDebounceTime = 0;  // the last time the output pin was toggled
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers
unsigned long lastTime = 0;

// Initialize the OLED display using Wire library
SSD1306Wire  display(0x3c, D2, D1);  //SSD1306Wire  display(0x3c, 4, 5);

// DISPLAY SETTING
OLEDDisplayUi ui     ( &display );

//ssd1306 slide demos
void msOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);
void drawFrame1(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawFrame2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawFrame3(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawFrame4(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawFrame5(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);


// This array keeps function pointers to all frames
// frames are the single views that slide in
//FrameCallback frames[] = {drawFrame1, drawFrame2, drawFrame3, drawFrame4, drawFrame5 };
FrameCallback frames[] = {drawFrame1, drawFrame2, drawFrame3 };

// how many frames are there?
int frameCount = 3;

// Overlays are statically drawn on top of a frame eg. a clock
OverlayCallback overlays[] = { msOverlay };
int overlaysCount = 1;


void setup()
{

    Serial.begin(115200);

    // The ESP is capable of rendering 60fps in 80Mhz mode
  	// but that won't give you much time for anything else
  	// run it in 160Mhz mode or just set it to 30 fps
    ui.setTargetFPS(60);

  	// Customize the active and inactive symbol
    ui.setActiveSymbol(activeSymbole);
    ui.setInactiveSymbol(inactiveSymbole);

    // You can change this to
    // TOP, LEFT, BOTTOM, RIGHT
    ui.setIndicatorPosition(BOTTOM);

    // Defines where the first frame is located in the bar.
    ui.setIndicatorDirection(LEFT_RIGHT);

    // You can change the transition that is used
    // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_UP, SLIDE_DOWN
    //ui.setFrameAnimation(SLIDE_LEFT);

    // Add frames
    ui.setFrames(frames, frameCount);

    // Add overlays
    ui.setOverlays(overlays, overlaysCount);

    // Initialising the UI will init the display too.
    ui.init();

    display.flipScreenVertically();

    // OUTPUT initialize
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);

    // INPUT initialize
    pinMode(ESTOP_PIN, INPUT_PULLUP);
  	//attachInterrupt(digitalPinToInterrupt(ESTOP_PIN), ESTOP_interrupt, FALLING);

    WiFiConnect();
    configTime(TZ_SEC, DST_SEC, "pool.ntp.org");
    client.setServer(mqtt_server, 1883);
    client.setCallback(callback);
}


void loop()
{

    	if (!client.connected())
    	{
    		reconnect();
    	}
    	client.loop();

      int remainingTimeBudget = ui.update();
      if (remainingTimeBudget > 0) {
          delay(remainingTimeBudget);
          pushover_send();
      }


      // e-stop SWITCH 시작
      int reading = digitalRead(ESTOP_PIN);

      // If the switch changed, due to noise or pressing:
      if(reading == LOW){
             ESTOP_flag = false;  //switch close
             ESTOP_UI = "OFF";

       }else{
             ESTOP_flag = true;   //switch open
             ESTOP_UI = "ON";
       }//e-stop end

      if(ESTOP_flag == false){      //E-STOP 스위치 열려있을때
           digitalWrite(BUZZER_PIN, 1); //부저강제중지
      }

      DISP_LED = digitalRead(LED_PIN);
      DISP_BUZZER = digitalRead(BUZZER_PIN);



      t.update();

} //loop end



void afterEvent(){
  int afterEvent = t.after(10000, doAfter);
  Serial.print("After event started id=");
  Serial.println(afterEvent);
  digitalWrite(LED_PIN, led_flag);
  digitalWrite(BUZZER_PIN, buzzer_flag);
}


void doAfter(){
  Serial.println("stop the warring event");
  //t.stop(ledEvent);
  //t.oscillate(13, 500, HIGH, 5);
  digitalWrite(LED_PIN, 1); //강제중지
  digitalWrite(BUZZER_PIN, 1); //부저강제중지
}


void Publish(char *Topic, char *Message){
  char TopicBase[80] = TOPICBASE;

  strcat(TopicBase, Topic);
  client.publish(TopicBase, Message);
}

void PublishInt(char *Topic, int Value){
  char TopicBase[80] = TOPICBASE;
  char Message[10] = "NULL";

  if (!isnan(Value))
    itoa(Value, Message, 10);

  strcat(TopicBase, Topic);
  client.publish(TopicBase, Message);
}

void PublishFloat(char *Topic, float Value){
  char TopicBase[80] = TOPICBASE;
  char Message[10] = "NULL";

  if (!isnan(Value))
    dtostrf(Value, 5, 1, Message);

  strcat(TopicBase, Topic);
  client.publish(TopicBase, Message);
}


/*
void ESTOP_interrupt(){
   noInterrupts();

   int reading = digitalRead(ESTOP_PIN);

   // If the switch changed, due to noise or pressing:
   if(reading != lastButtonState){
     lastButtonState =! lastButtonState;
     lastDebounceTime = millis();
   }

   if ((millis() - lastDebounceTime) > debounceDelay) {
      ESTOP_flag =! ESTOP_flag;
      lastDebounceTime = millis();
      ESTOP_UI = "ON";
   }else{
      ESTOP_UI = "OFF";
   }

    //set the BUZZER
    digitalWrite(BUZZER_PIN, buzzer_flag);

    // save the reading.  Next time through the loop,
    // it'll be the lastButtonState:
    lastButtonState = reading;

    interrupts();
}
*/


void reconnect(){

	// Loop until we're reconnected
	while (!client.connected())
	{
    //String clientid = MQTTCLIENTID + String(random(0xffff), HEX);
    String clientid = MQTTCLIENTID + String(WiFi.macAddress());
    const char * CLIENTID = clientid.c_str();

  //connect (clientID, username, password, willTopic, willQoS, willRetain, willMessage, cleanSession)
  if (client.connect(CLIENTID, MQTTUSER, MQTTPASSWORD, (char *)TOPICBASE "State", 1, 0, "DEAD"))

		{
			// Once connected, publish an announcement...
      Publish((char *)"State", (char *)"BOOTUP");

			// Subscribe to enable bi-directional comms.
			client.subscribe(TOPICBASE "config/#");  // Allow bootup config fetching using MQTT persist flag!
      client.subscribe(TOPICBASE "push/#");     // Send commands to this device, use Home/LetterBox/Get/# for responses.
      client.subscribe(TOPICBASE "sensor/temperature");     // Send commands to this device, use Home/LetterBox/Get/# for responses.
      client.subscribe(TOPICBASE "sensor/humidity");     // Send commands to this device, use Home/LetterBox/Get/# for responses.
		}
		else
			delay(5000);
	}
}

void WiFiConnect(){
	//Serial print
	Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  int counter = 0;
  WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(200);
      Serial.print(".");
      display.clear();
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.drawString(64, 1, "Connecting to WiFi");
      display.drawXbm(34, 18, WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);
      display.drawXbm(46, 55, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
      display.drawXbm(60, 55, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
      display.drawXbm(74, 55, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
      display.display();
      counter++;
    }

      Serial.println("");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
      Serial.print("MAC address: ");
      Serial.println(WiFi.macAddress());
      Serial.println("WiFi connected");
      display.clear();
      display.setTextAlignment(TEXT_ALIGN_CENTER);
      display.drawString(64, 30, "WiFi connected");
      //display.drawString(64, 40, WiFi.localIP());
      // Get time from network time service
      display.display();
      delay(3000);
}


void msOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {

    //const long interval = 1000;
    unsigned long loadsec = millis()/1000;
    unsigned long loadmin = loadsec/60;
    unsigned long loadhour = loadsec/3600;
    //unsigned long loadday = loadsec/86400;

    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    display->setFont(ArialMT_Plain_10);
    display->drawString(128, 0, String(loadsec));
    display->drawString(128, 10, String(loadmin));
    display->drawString(128, 20, String(loadhour));
    //if(alram_flag == true)display->drawString(128, 30, SetTimer(0,0,10));
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(0, 0, DISP_DEBUG);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(0, 54, TEMP);
    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    display->drawString(128, 54, TEMPSET);
    //BOTTOM LINE
    display->drawHorizontalLine(0, 53, 128);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(0, 40, ESTOP_UI);
    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    display->drawString(118, 40, DISP_LED);
    display->drawString(128, 40, DISP_BUZZER);
}




void drawFrame1(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  now = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&now);
  char buff[16];
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String date = WDAY_NAMES[timeInfo->tm_wday];
  //sprintf_P(buff, PSTR("%s, %02d/%02d/%04d"), WDAY_NAMES[timeInfo->tm_wday].c_str(), timeInfo->tm_mday, timeInfo->tm_mon+1, timeInfo->tm_year + 1900);
  //display->drawString(64 + x, 15 + y, String(buff));
  display->setFont(ArialMT_Plain_24);
  sprintf_P(buff, PSTR("%02d:%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
  display->drawString(64 + x, 17 + y, String(buff));
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

// 온도값 표시
void drawFrame2(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_24);
  //String TEMP = String(TempValue) + "°c";
  display->drawString(64 + x, 17 + y, TEMP);
}

// 습도값 표시
void drawFrame3(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_24);
  //String HUM = String(HumidValue) + "%";
  display->drawString(64 + x, 17 + y, HUM);
}

/*
void drawFrame4(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_24);
  //String TEMP = String(TempValue) + "°c";
  display->drawString(64 + x, 17 + y, HUM);
}

void drawFrame5(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64 + x, 20 + y, "SSID:");
  display->drawString(64 + x, 30 + y, ssid);
}
*/

/*
void pushover_send(){

    //String Str1 = "온도:" + String(temperature, 1);
    //String Str2 = "습도:" + String(HumidValue, 1);

    //String Str1 = "온도:" + String(TEMP, 1);
    //String Str2 = "습도:" + String(HUM, 1);

    Serial.print("pushover send: ");
    // titolo del messaggio. Compare nel pop-up e nell'elenco
    pushover.setTitle("Temperature Warring");
    // corpo del messaggio
    pushover.setMessage("TEST");
    // suono da emettere, l'elenco completo è sul sito di pushover come spiegato nell'articolo su settorezero
    pushover.setSound("alien");
    // priorità, di default è 0
    pushover.setPriority(1);
    // impostare il device se si vuole inviarlo ad un solo device, altrimenti lasciare commentato per inviare
    // il messaggio a tutti i device associati a pushover
    pushover.setDevice("[PUSHOVER-DEVICE]");
    // parametri obbligatori per notifica di emergenza di tipo 2 (che richiede conferma di lettura)
    //pushover_button.setRetry(60);
    //pushover_button.setExpire(500);
    // il metodo .send invia il messaggio e restituisce un valore

    pushover.send();
    Serial.println(pushover.send());
  }
*/


void pushover_send(){

  // attendo 100mS come antibounce e riverifico la pressione del pulsante
    //delay(2000);

    Serial.println("button pressed");
    // titolo del messaggio. Compare nel pop-up e nell'elenco
    pushover_api.setTitle("Rilevato pulsante");
    // corpo del messaggio
    pushover_api.setMessage("Pulsante premuto!");
    // suono da emettere, l'elenco completo è sul sito di pushover come spiegato nell'articolo su settorezero
    pushover_api.setSound("alien");
    // priorità, di default è 0
    pushover_api.setPriority(1);
    // impostare il device se si vuole inviarlo ad un solo device, altrimenti lasciare commentato per inviare
    // il messaggio a tutti i device associati a pushover
    //pushover_button.setDevice("[PUSHOVER-DEVICE]");
    // parametri obbligatori per notifica di emergenza di tipo 2 (che richiede conferma di lettura)
    //pushover_button.setRetry(60);
    //pushover_button.setExpire(500);
    // il metodo .send invia il messaggio e restituisce un valore
    Serial.println(pushover_api.send());


}















void callback(char* topic, byte* payload, unsigned int length) {
      unsigned int i = 0;
      for (i=0; i < length; i++) {
      message_buff[i] = payload[i];
      }
      message_buff[i] = '\0';
      String msgString = String(message_buff);
      Serial.println("Inbound: " + String(topic) + ":" + msgString);

      if (String(topic) == "/SKMT/SERVER1/sensor/temperature") {
            TEMP =  msgString + "°c";
            temperature = atoi(msgString.c_str());
            tempconfig = atoi(TEMPSET.c_str());

            if (temperature < tempconfig){     // 온도 경고 LIMMIT 설정
                DISP_DEBUG =  "GOOD OK";
                buzzer_flag = 1;              // 부저끄기
                led_flag = 1;
            }else {
                DISP_DEBUG =  "WARRING";
                buzzer_flag = 0;              // 부저켜기
                led_flag = 0;
                afterEvent();
                }
       }

        if (String(topic) == "/SKMT/SERVER1/sensor/humidity") {
              HUM =  msgString + "%";
              HumidValue = atoi(msgString.c_str());
              //int temperature = atoi(msgString.c_str());
        }


        if (String(topic) == "/SKMT/SERVER1/sensor/leak") {
              //LEAK =  msgString + "%";
              int leak = atoi(msgString.c_str());

              if (leak > 1){
                DISP_DEBUG =  "LEAK WARRING";
                afterEvent();
              }
        }


        if (String(topic) == "/SKMT/SERVER1/sensor/shutdown") {
              //SHUTDOWN =  msgString + "%";
              int shutdown = atoi(msgString.c_str());
              if(shutdown > 1){
                DISP_DEBUG =  "SHUTDOWN";
                afterEvent();
              }
        }


        if (String(topic) == "/SKMT/SERVER1/config/tempset") {
            TEMPSET =  msgString + "°c";
            //int temperature = atoi(msgString.c_str());
      }

} //callback end
