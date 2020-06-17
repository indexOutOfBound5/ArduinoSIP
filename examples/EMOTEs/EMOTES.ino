/* ====================================================================

   Copyright (c) 2019 Thorsten Godau (dl9sec), Meghan Halton (indexOutOfBound5).
   All rights reserved.


   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the
      distribution.

   3. Neither the name of the author(s) nor the names of any contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) AND CONTRIBUTORS "AS IS" AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE.

   ====================================================================*/

#include <ESP8266WiFi.h>
#include <ArduinoSIP.h>
#include <rBase64.h>

//#define DEBUGLOG

#define LED_ESP12E            2
#define LED_NODEMCU           16
#define EMOTE_SIZE            2
#define EMOTE_SIZE_ENCODED    4
#define CRLF_NULL_LEN         2

//------------------------------------------------
// Configuration with static IP
//------------------------------------------------

// Emotional LED config - Make sure this is correct for your HW configuration!
int BLUE_LED = 0;
int RED_LED = 2;
// Doesn't exist on the original board int GREEN_LED = 3;

// WiFi parameters
const char* WiFiSSID    = "MyWifiName"; // WiFi SSID
const char* WiFiPSK     = "MyWifiPassword"; // WiFi WPA2 preshared key

char *WiFiIP            = "192.168.2.70";     // WiFi IP of this board

// Sip parameters
const char *SipIP       = "192.155.85.35";  // IP of the Remote SIP proxy server
const int   SipPORT     = 5060;             // SIP port of the Remote SIP proxy server
const char *SipDOMAIN   = "myname.com";    // SIP User's domain eg. myname.com
const char *SipUSER     = "me";         // SIP User's username
const char *SipPW       = "password";       // SIP User's password

const char *SipDOMAINFriend   = "friendname.com"; // SIP Friend's domain
const char *SipUSERFriend     = "friend";        // SIP Friend's username.

const char *SipPayload   = "";

char    acSipIn[2048];
char    acSipOut[2048];

Sip   aSip(acSipOut, sizeof(acSipOut));

//EMOTES
struct __attribute__ ((packed)) Emote {
  uint8_t emotion;  // 1 byte   - @0
  uint8_t intensity;  // 1 byte   - @1
};

const int UNKNOWN_EMOTE = 0;
const int HAPPY = 1;
const int SAD = 2;
const int ANGRY = 3;
const int EMOTIONS[] = {HAPPY, SAD, ANGRY, UNKNOWN_EMOTE};

// Global counts for the main loop.
uint32_t count = 0;
uint32_t emotions_count = 0;

void setup()
{
  // Watchdog timer
  ESP.wdtDisable();
  ESP.wdtEnable(WDTO_8S);
  
  //Serial port
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  delay(10);
  
  //Other pins
  pinMode(LED_NODEMCU, OUTPUT);
  pinMode(4, INPUT_PULLUP);
  pinMode(5, INPUT_PULLUP);
  
  /* Begin Setup light pins*/
  pinMode(RED_LED, OUTPUT); // RED
  pinMode(BLUE_LED, OUTPUT); //BLUE
  
  digitalWrite(LED_NODEMCU, HIGH); // LED off
  digitalWrite(RED_LED, HIGH); // LED off
  digitalWrite(BLUE_LED, HIGH); // LED off
  /* END Setup light pins */
  
  /* BEGIN Connect to Wifi */
  Serial.printf("\r\n\r\nConnecting to %s\r\n", WiFiSSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WiFiSSID, WiFiPSK);

  while ( WiFi.status() != WL_CONNECTED )
  {
    delay(500);
    Serial.print(".");
  }

  WiFi.persistent(true);
  Serial.printf("\r\nWiFi connected to: %s\r\n", WiFi.localIP().toString().c_str());

  WiFi.localIP().toString().toCharArray(WiFiIP, WiFi.localIP().toString().length());
  /* END Wifi setup */

  /* BEGIN SIP setup */
  aSip.Init(SipIP, SipPORT, WiFiIP, SipPORT, SipUSER, SipDOMAIN, SipPW, 38);

  aSip.Register(NULL);
  /* END SIP setup */

}


void loop(void)
{

  // SIP processing
  bool gotEmote = false;
  aSip.Processing(acSipIn, sizeof(acSipIn), &gotEmote);

  if (gotEmote) {
    //initialise temporary buffers
    char emote_msg_data[EMOTE_SIZE_ENCODED];
    char emote_msg_decoded[EMOTE_SIZE + 1];
    byte emote_decoded[EMOTE_SIZE + 1];
    struct Emote emote = {0};
    
    // Get any waiting SIP messages
    aSip.getMessage(emote_msg_data, EMOTE_SIZE_ENCODED, acSipIn, EMOTE_SIZE_ENCODED);

    // Messages are encoded in base 64 (so that they don't accidentally contain breaking symbols), decode them
    int decoded_bytes = rbase64_decode(emote_msg_decoded, emote_msg_data, EMOTE_SIZE_ENCODED);
    memcpy(emote_decoded, emote_msg_decoded, EMOTE_SIZE + 1);

    // Unpack our binary data into a more readable emote struct
    unpack_emotion(&emote, emote_decoded);

    Serial.println("*********");
    Serial.printf("Got emotion %i, of intensity %i", emote.emotion, emote.intensity);
    Serial.println("*********");

    // If you had the LED on a PWM/analog pin it would be nice to pass these functions intensity (right now they're digital, so just on or off).
    switch (emote.emotion) {
      case HAPPY:
        react_happy();
        break;
      case SAD:
        react_sad();
        break;
      case ANGRY:
        react_angry();
        break;
      default:
        react_unknown();
        break;
    }
  }
  
  // Debug print, are you recieving data, are you connect to SIP?
  Serial.print(acSipIn);
  
  // Don't spam the server, only send things every thirteen loops.
  if (!(count % 13)) {
    
    byte emote_data[EMOTE_SIZE] = {0};
    char emote_encoded[EMOTE_SIZE_ENCODED];
    
    //Rotate through the EMOTIONS array.
    pack_emotion(EMOTIONS[emotions_count%4], 99, emote_data);

    //Base64 encode it so it doesn't interfere with the SIP protocol
    rbase64_encode(emote_encoded, (char *)emote_data, 2);

    // Print it so we know what we're sending, should be nonsense with an = sign at the end.
    Serial.println("*********");
    Serial.println(emote_encoded);
    Serial.println("*********");

    // Send it!
    aSip.Message(SipUSERFriend, SipDOMAINFriend, emote_encoded, (4 + CRLF_NULL_LEN));

    // This is so that it rotates through the array. What happens if you delete it?
    emotions_count++;
  }

  //This is so that we don't spam the server.
  count++;

  // Feed your watch dog.
  ESP.wdtFeed();

  // Don't spam.
  delay(1000);
}

void pack_emotion(uint8_t emotion, uint8_t intensity, byte emote_data[]) {
  struct Emote emote;
  emote.emotion = emotion;
  emote.intensity = intensity;
  memcpy(emote_data, &emote, EMOTE_SIZE);
}


void unpack_emotion(struct Emote *emote, byte emote_data[]) {
  memcpy(emote, emote_data, EMOTE_SIZE);
}

int get_emotion(char *source) {
  char buf[2];
  strncpy(buf, source, 2);
  return atoi(buf);
}

int get_intensity(char *source) {
  char buf[2];
  strncpy(buf, source+2, 2);
  return atoi(buf);
}

void react_happy() {
  Serial.println("We recieved a happy message!");
  digitalWrite(RED_LED, LOW); // LED off
  digitalWrite(BLUE_LED, LOW); // LED off
  delay(500);
  digitalWrite(RED_LED, HIGH); // LED off
  digitalWrite(BLUE_LED, HIGH); // LED off
  delay(100);
  digitalWrite(RED_LED, LOW); // LED off
  digitalWrite(BLUE_LED, LOW); // LED off
  delay(500);
  digitalWrite(RED_LED, HIGH); // LED off
  digitalWrite(BLUE_LED, HIGH); // LED off
}

void react_sad() {
  Serial.println("We recieved a sad message!");
  digitalWrite(BLUE_LED, LOW); // LED off
  delay(500);
  digitalWrite(BLUE_LED, HIGH); // LED off
  delay(100);
  digitalWrite(BLUE_LED, LOW); // LED off
  delay(500);
  digitalWrite(BLUE_LED, HIGH); // LED off
}

void react_angry() {
  Serial.println("We recieved an angry message!");
  digitalWrite(RED_LED, LOW); // LED off
  delay(500);
  digitalWrite(RED_LED, HIGH); // LED off
  delay(100);
  digitalWrite(RED_LED, LOW); // LED off
  delay(500);
  digitalWrite(RED_LED, HIGH); // LED off
}


void react_unknown() {
  Serial.println("We recieved an unknown message!");
  digitalWrite(RED_LED, LOW); // LED off
  digitalWrite(BLUE_LED, LOW); // LED off
  delay(1000);
  digitalWrite(RED_LED, HIGH); // LED off
  digitalWrite(BLUE_LED, HIGH); // LED off

}
