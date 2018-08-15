/* -----------------------------------------------------------
This checks for a good GPS fix every 10 minutes, and publishes that
data if it is one, and the data is different enough from the
preivously published data. If not, it will save data by staying quiet.

It also registers some Particle.functions for changing whether it
publishes, reading the battery level, and manually requesting GPS
readings.
---------------------------------------------------------------*/

// Getting the library
#include "AssetTracker.h"

// Set whether you want the device to publish data to the internet by default here.
// 1 will Particle.publish AND Serial.print, 0 will just Serial.print
// Extremely useful for saving data while developing close enough to have a cable plugged in.
// You can also change this remotely using the Particle.function "tmode" defined in setup()
int transmittingData = 1;

// Used to keep track of the last time we published data
long lastPublish = 0;

// How many minutes between publishes? 10+ recommended for long-time continuous publishing!
int delayMinutes = 5;

// Creating an AssetTracker named 't' for us to reference
AssetTracker t = AssetTracker();

// A FuelGauge named 'fuel' for checking on the battery state
FuelGauge fuel;

// GPS coordinates from the last publish. Don't send anything new if we haven't moved much
bool previousCoordinatesSet = false;
float previousLon = 0;
float previousLat = 0;

// Threshold to ignore updates (knowing when food trucks are on the move isn't very helpful)
// 9000 is VERY sensitive, 12000 will still detect small bumps.
// We want to know when it's driving.
int accelThreshold = 16000;

// setup() and loop() are both required. setup() runs once when the device starts
// and is used for registering functions and variables and initializing things
void setup() {

    // Sets up all the necessary AssetTracker bits
    t.begin();

    // Enable the GPS module. Defaults to off to save power.
    // Takes 1.5s or so because of delays.
    t.gpsOn();

    // Opens up a Serial port so you can listen over USB
    //Serial.begin(9600);

    // These three functions are useful for remote diagnostics. Read more below.
    Particle.function("tmode", transmitMode);
    Particle.function("batt", batteryStatus);
    Particle.function("gps", gpsPublish);
    Particle.function("gpsIfMoved", gpsPublishIfMoved);
}

// loop() runs continuously
void loop() {
    // Must be run to capture the GPS output.
    t.updateGPS();
    // if the current time - the last time we published is greater than your set delay...
    if (millis()-lastPublish > delayMinutes*60*1000) {
        // Remember when we published
        lastPublish = millis();

        gpsPublishIfMoved("");
    }
}

// Allows you to remotely change whether a device is publishing to the cloud
// or is only reporting data over Serial. Saves data when using only Serial!
// Change the default at the top of the code.
int transmitMode(String command) {
    transmittingData = atoi(command);
    return 1;
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

// Same as gpsPublish, but only publishes if we've moved enough.
int gpsPublishIfMoved(String command) {

    if (t.gpsFix()) {
            
        // If the truck is moving too fast, ignore it
        if (t.readXYZmagnitude() < accelThreshold) {
        
            // See if we've moved enough since last time to report.
            // Make the reasonable assumption that the earth is flat.
            float thisLon = t.readLon();
            float thisLat = t.readLat();
            
            float changeLon = abs(previousLon - thisLon);
            float changeLat = abs(previousLat - thisLat);
            
            // In our testing, drift seems to be about a few ten-thousandths of a degree. If movement
            // Is greater than one thousandth of a degree, update.
            if (!previousCoordinatesSet || (changeLon > 0.0005 || changeLat > 0.0005)) {
                
                // Update our records.
                previousLon = thisLon;
                previousLat = thisLat;
                previousCoordinatesSet = true;
                
                // Only publish if we're in transmittingData mode 1;
                if (transmittingData) {
                    // Short publish names save data!
                    Particle.publish("G", t.readLatLon(), 60, PRIVATE);
                    return 1;
                } else {
                    //Serial.println("Not transmitting. We would have though!");
                    return 3;
                }
            } else {
                //Serial.println("Not enough change, not transmitting.");
                return 2;
            }
        }
    }
    return 0;
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
