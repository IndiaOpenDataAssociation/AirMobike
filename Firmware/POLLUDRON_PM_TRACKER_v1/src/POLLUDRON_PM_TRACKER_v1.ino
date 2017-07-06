// This #include statement was automatically added by the Particle IDE.
#include <AssetTracker.h>
#include "HttpClient.h"
#include "SparkJson.h"

STARTUP(cellular_credentials_set("unlimitiot","","",NULL));

#define IBM_SERVER ".messaging.internetofthings.ibmcloud.com"
#define IBM_PORT 1883
#define IBM_ORG "babwc6"
#define DEVICEID "OZ_PARTICLE_018"
#define DEBUG_FLAG 1

// Dust Sensor Pin configuration
const byte Dust_PM25_pin = D2;
const byte Dust_PM10_pin = D3;

float pm25 = 0;
float pm10 = 0;
unsigned int p1count = 0;
unsigned int p2count = 0;

float lat = 0;
float lon = 0;

//Object to work HTTP requests
HttpClient http;

//Payload headers essential to POST sensor data requests
http_header_t payloadHeaders[] = {
    { "Content-Type", "application/json" },
    { "Authorization" , "Basic dXNlLXRva2VuLWF1dGg6aGV0dmlfMTIzNA=="},
    { NULL, NULL } // NOTE: Always terminate headers will NULL
};

// Used to keep track of the last time we published data
long lastPublish = 0;

// How many minutes between publishes? 10+ recommended for long-time continuous publishing!
int delayMinutes = 1;

// Creating an AssetTracker named 't' for us to reference
AssetTracker t = AssetTracker();

// A FuelGauge named 'fuel' for checking on the battery state
FuelGauge fuel;

// setup() and loop() are both required. setup() runs once when the device starts
// and is used for registering functions and variables and initializing things
void setup() {
    // Sets up all the necessary AssetTracker bits
    t.begin();

    // Enable the GPS module. Defaults to off to save power.
    // Takes 1.5s or so because of delays.
    t.gpsOn();

    // Opens up a Serial port so you can listen over USB
    Serial.begin(9600);

    Particle.function("batt", batteryStatus);
    Particle.function("gps", gpsPublish);
}

// loop() runs continuously
void loop() {
    // You'll need to run this every loop to capture the GPS output
    t.updateGPS();

    getDust_nova_pm25();
    getDust_nova_pm10();

    delay(500);

    Particle.process();

    // if the current time - the last time we published is greater than your set delay...
    if (millis()-lastPublish > delayMinutes*60*1000) {

        lastPublish = millis();

        if(DEBUG_FLAG)
          Serial.println(t.preNMEA());

        // GPS requires a "fix" on the satellites to give good data,
        // so we should only publish data if there's a fix
        if (t.gpsFix()) {
          String value = t.readLatLon();
          int commaIndex = value.indexOf(',');
          lat = (value.substring(0, commaIndex)).toFloat();
          lon = (value.substring(commaIndex+1)).toFloat();
        }

        if(sendPayload())
        {
          if(DEBUG_FLAG)
          {
            Serial.println("Send data Done");
          }
          else{
            Serial.println("Send data Failed");
          }
        }
        pm25 = 0;
        pm10 = 0;
        p1count = 0;
        p2count = 0;
    }
}

// Lets you remotely check the battery status by calling the function "batt"
// Triggers a publish with the info (so subscribe or watch the dashboard)
// and also returns a '1' if there's >10% battery left and a '0' if below
int batteryStatus(String command){
    // Publish the battery voltage and percentage of battery remaining
    // if you want to be really efficient, just report one of these
    // the String::format("%f.2") part gives us a string to publish,
    // but with only 2 decimal points to save space
    Particle.publish("B",
          "v:" + String::format("%.2f",fuel.getVCell()) +
          ",c:" + String::format("%.2f",fuel.getSoC()),
          60, PRIVATE
    );
    // if there's more than 10% of the battery left, then return 1
    if (fuel.getSoC()>10){ return 1;}
    // if you're running out of battery, return 0
    else { return 0;}
}

bool sendPayload()
{
  if(Cellular.ready() == false)
  {
    Cellular.connect();
  }

  if(p1count > 0)
  {
    pm25 = pm25 / p1count;
  }

  if(p2count > 0)
  {
    pm10 = pm10 / p2count;
  }

  StaticJsonBuffer<300> jsonBuffer;
  JsonObject& payload = jsonBuffer.createObject();
  JsonObject& dataJson = payload.createNestedObject("d");

  dataJson["t"] = int(Time.now());
  dataJson["p1"] = pm25;
  dataJson["p2"] = pm10;
  dataJson["lat"] = lat;
  dataJson["lon"] = lon;

  char buff[300];
  payload.printTo(buff, sizeof(buff));

  // Variable for http request
  http_request_t request;
  http_response_t response;

  ////Setting up server credentials for POST request
  request.hostname = String(IBM_ORG) + IBM_SERVER;
  request.port = IBM_PORT;
  //POST request for Device according to the Device Type and Device ID
  String path = "/api/v0002/device/types/AIROWLWI/devices/" + String(DEVICEID) + "/events/data";
  request.path = path;
  request.body = String(buff);

  //Inititing POST Request
  http.post(request, response, payloadHeaders);

  //Checking response status to confirm successfully Posting Sensor DATA
  if(response.status == 200)
  {
    return 1;
  }
  else{
    return 0;
  }
}

void getDust_nova_pm25()
{
  int dust25;

  //pulse in function to calculate dust value based on duty cycle of PWM input
  unsigned long duration_2 = pulseIn(Dust_PM25_pin, HIGH);

  //Checking to see if the dust value is in range
  if(duration_2 >= 2000)
  {
    //Calculating the PM value from raw output
    dust25 = (duration_2 - 2000) / 1000;
  }

  if(DEBUG_FLAG)
  {
    Serial.print("DUST Particles : ");
    Serial.print("2.5 micron : ");
    Serial.print(dust25);
  }
  if(dust25 > 0)
  {
    pm25 += dust25;
    p1count++;
  }
}

void getDust_nova_pm10()
{
  int dust10;

  //pulse in function to calculate dust value based on duty cycle of PWM input
  unsigned long duration_1 = pulseIn(Dust_PM10_pin, HIGH);

  //Checking to see if the dust value is in range
  if(duration_1 >= 2000)
  {
    //Calculating the PM value from raw output
    dust10 = (duration_1 - 2000) / 1000;
  }

  if(DEBUG_FLAG)
  {
    Serial.print(" DUST Particles : ");
    Serial.print("10 micron : ");
    Serial.println(dust10);
  }
  if(dust10 > 0)
  {
    pm10 += dust10;
    p2count++;
  }

}

// Actively ask for a GPS reading if you're impatient. Only publishes if there's
// a GPS fix, otherwise returns '0'
int gpsPublish(String command) {
    if (t.gpsFix()) {
        Particle.publish("G", t.readLatLon(), 60, PRIVATE);

        // uncomment next line if you want a manual publish to reset delay counter
        // lastPublish = millis();
        return 1;
    } else {
      return 0;
    }
}
