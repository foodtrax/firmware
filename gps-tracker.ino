/* -----------------------------------------------------------
This checks for a good GPS fix every few minutes, and publishes
data/state information if necessary. If not, it will save data
by staying quiet.
---------------------------------------------------------------*/

// Getting the library
#include "AssetTracker.h"

// Keep track of the last time we published data
long lastPublish = 0;

// Keep track of the last time we checked our environment
long lastPublishCheck = 0;

// How many minutes between checks?
int delayMinutes = 2;

// How many minutes between heartbeats? The backend expects every 10 minutes.
int delayHeartbeatMinutes = 7;

// GPS coordinates from the last publish. Don't send anything new if we haven't moved much
bool previousCoordinatesSet = false;
float previousLon = 0;
float previousLat = 0;

// What did we last send?
String lastPublishType = "F";   // Press F to pay respects

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
    Particle.function("gpsIfMoved", gpsPublishIfMoved);
    
    // For debugging
    Particle.variable("lastPubType", lastPublishType);
}

// loop() runs continuously
void loop() {
    // Must be run to capture the GPS output.
    t.updateGPS();
    // if the current time - the last time we published is greater than your set delay...
    if (millis()-lastPublishCheck > delayMinutes*60*1000) {
        // Remember when we published
        lastPublishCheck = millis();

        gpsPublishIfMoved("");
    }
}

// Actively ask for a GPS reading if you're impatient. Only publishes if there's
// a GPS fix, otherwise returns '0'
int gpsPublish(String command) {
    if (t.gpsFix()) {
        previousLon = t.readLon();
        previousLat = t.readLat();
        previousCoordinatesSet = true;
        
        Particle.publish("G", t.readLatLon(), 60, PRIVATE);
        return 1;
    } else {
      return 0;
    }
}

// Publishes different states if different consitions are met.
// Return codes:
//   0 = We were online, and continue to be. 
//       Sent a heartbeat.
//   1 = We were online, and continue to be, but recently published. 
//       Nothing was done.
//   2 = We were online, then we moved.
//       Sent an offline.
//   3 = We were offline, but stationary.
//       Sent coordinates.
//   4 = We were offline, and moved.
//       Nothing was done.
//   5 = No GPS Fix.
//       Nothing was done.
int gpsPublishIfMoved(String command) {

    // No GPS fix, no good data.
    if (!t.gpsFix()) {
        return 5;
    }

    if (!previousCoordinatesSet) {
        // Our first update! Hopefully the first of many.
        previousLon = t.readLonDeg();
        previousLat = t.readLatDeg();
        Serial.println(String::format("previousLon %f",previousLon));
        Serial.println(String::format("previousLat %f",previousLat));
        previousCoordinatesSet = true;
    }

    // How far have we moved since we last checked?
    // Make the reasonable assumption that the earth is flat.
    float thisLon = t.readLonDeg();
    float thisLat = t.readLatDeg();
    
    float changeLon = f_abs(previousLon - thisLon);
    float changeLat = f_abs(previousLat - thisLat);
    Serial.println(String::format("previousLon %f",previousLon));
    Serial.println(String::format("previousLat %f",previousLat));
    Serial.println(String::format("thisLon %f",thisLon));
    Serial.println(String::format("thisLat %f",thisLat));
    Serial.println(String::format("changeLon %f",changeLon));
    Serial.println(String::format("changeLat %f",changeLat));
    
    if (changeLon > 0.001 || changeLat > 0.001) {
        
        // Update our records.
        previousLon = thisLon;
        previousLat = thisLat;
        
        if (lastPublishType != "F") {
            Particle.publish("F", "", 60, PRIVATE);
            lastPublishType = "F";
            lastPublish = millis();
            return 2;
        }
    
        return 4;
    }

    if (lastPublishType == "F") {
        Particle.publish("G", t.readLatLon(), 60, PRIVATE);
        lastPublishType = "G";
        lastPublish = millis();
        return 3;
    }
    
    if (millis()-lastPublish > delayHeartbeatMinutes*60*1000) {
        Particle.publish("H", "", 60, PRIVATE);
        lastPublishType = "H";
        lastPublish = millis();
        return 0;
    }
    return 1;
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

float f_abs(float f) {
    if (f < 0) {
        return -1 * f;
    }
    return f;
}