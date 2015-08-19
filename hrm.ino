#include <Adafruit_RGBLCDShield.h>
#include <Adafruit_MCP23017.h>
#include <WaveUtil.h>
#include <WaveHC.h>
#include <FatReader.h>
#include <FatStructs.h>
#include <SdReader.h>
#include <SdInfo.h>
#include <Average.h>
#include <Wire.h>

/////////////////////////// HRM ISR /////////////////////////////
//Seven variables below are used in the ISR, taken from example at
//github.com/WorldFamousElectronics/PulseSensor_Amped_Arduino/blob/master/PulseSensorAmped_Arduino_1dot4/PulseSensorAmped_Arduino_1dot4.ino

int pulsePin = 0;           // Pulse Sensor purple wire connected to analog pin 0
int blinkPin = 13;            // pin to blink led at each beat

// Volatile Variables, used in the interrupt service routine!
volatile int BPM;                   // int that holds raw Analog in 0. updated every 2mS
volatile int Signal;                // holds the incoming raw data
volatile int IBI = 600;             // int that holds the time interval between beats! Must be seeded! 
volatile boolean Pulse = false;     // "True" when User's live heartbeat is detected. "False" when not a "live beat". 
volatile boolean QS = false;

///////////////////////// WAVE SHIELD ///////////////////////
// The below variables are from an example at
//learn.adafruit.com/adafruit-wave-shield-audio-shield-for-arduino/examples

SdReader card; //holds information for the card
FatVolume vol; //holds information for the partition on the card
FatReader root; //holds the information for the filesystem on card
FatReader f; //holds information for the file being played
WaveHC wave;  

#define DEBOUNCE 100

// debugging wave shield methods, adapted from
//learn.adafruit.com/adafruit-wave-shield-audio-shield-for-arduino/examples
void errorCheck() //error checking the SD card
{
  if(card.errorCode()){
    putstring("\n\rSD I/O error: ");
    Serial.print(card.errorCode(), HEX);
    putstring(", ");
    Serial.println(card.errorData(), HEX);
    while(1);
  }else {
    putstring("it's working.");
  }
}

////////////////////  RGB LCD SHIELD ///////////////////////
// Adapted from examples found here
//learn.adafruit.com/rgb-lcd-shield/using-the-rgb-lcd-shield

Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();

#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7

///////////////// PULSE SENSOR CODE ///////////////////
//Adapted from following website:
//github.com/WorldFamousElectronics/PulseSensor_Amped_Arduino/tree/master/PulseSensorAmped_Arduino_1dot4

int fadeRate = 0;
int heartval[5];
int h = 0;
// other variables for pulse sensor located earlier in code

void setup() {
  // put your setup code here, to run once:
  pinMode(blinkPin, OUTPUT);
  pinMode(2, OUTPUT);
  pinMode(3, OUTPUT);
  pinMode(4, OUTPUT);
  pinMode(5, OUTPUT);
  
  digitalWrite(14, HIGH);
  digitalWrite(15, HIGH);
  digitalWrite(16, HIGH);
  digitalWrite(17, HIGH);
  digitalWrite(18, HIGH);
  digitalWrite(19, HIGH);
  
  Serial.begin(9600);
  
  //////// WAVE SHIELD debugging from wave shield site earlier linked //////////
  if (!card.init()){
    putstring_nl("Card init. failed.");
    errorCheck();
    while(1);
  }
  
  
  uint8_t part;
  for(part = 0; part <= 5; part++){
    if(vol.init(card, part)){
      break;
    }
    if(part==5){
      putstring_nl("no valid FAT partition");
      errorCheck();
      while(1);
    }
  }
  putstring("Using partition ");
  Serial.print(part, DEC);
  putstring(", type is FAT");
  Serial.println(vol.fatType(),DEC);

  if (!root.openRoot(vol)){
    putstring_nl("can't open root dir");
    while(1);
  }
  
  interruptSetup(); //reads pulse sensor signal every 2ms
  
  ///////Seting up the LCD Display ////////////
  lcd.begin(16, 2);
  lcd.setBacklight(TEAL); // background is teal until heartrate is taken
  
  lcd.setCursor(0, 0);
  lcd.print("Welcome to your");
  lcd.setCursor(0, 1);
  lcd.print("Arduino HRM!");
  delay(2000);                //allows the wave shield to start up
  playcomplete("INOONE.wav"); //plays a file that says "welcome"
  playcomplete("INTRONN.wav"); //plays instructions file
  

}

uint8_t i=0;

/////////////////////////////////////////////////////////////////////

void loop() {
  uint8_t buttons = lcd.readButtons();
  if(buttons){
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.setBacklight(TEAL);  // if any button is pushed, backlight color returns to teal
    
    if(buttons & BUTTON_UP){
      heartRate();
    }
    if(buttons & BUTTON_LEFT){
      lcd.setCursor(0, 0);
      lcd.print("Last 5 readings: ");
      lcd.setCursor(0, 10);
      int i;
      for (i=0; i<5; i++){
        lcd.println(heartval[i]);
      }
    }
    
    if(buttons & BUTTON_RIGHT){
      lcd.print("MOTIVATION");
      playcomplete("DOITTTT.wav"); // "motivational" audio file
      lcd.setCursor(0,0);
     // lcd.print("MOTIVATION");
    }
    if(buttons & BUTTON_DOWN){
      averageH();
    }
    if(buttons & BUTTON_SELECT){
      lcd.print("Select");
      playcomplete("INTRONN.wav"); //plays the instructions
    }
  }


}

//////////////////////////////(end of void loop)////////////////

//////////// WAVE SHIELD playing .wav files///////////////////
//////////// adapted from previous wave shield link //////////

void playcomplete(char *name){
  playfile(name);
  while(wave.isplaying){ //does nothing while a file is playing
  }//file has finished playing
}
void playfile(char *name){
  if(wave.isplaying){
    wave.stop();
  }
  if(!f.open(root, name)){
    putstring("Couldn't open file ");
    Serial.print(name);
    return;
  }
  if(!wave.create(f)){
    putstring_nl("not a valid wav file");
    return;
  }
  wave.play();
}

////////////////////////////////////////////////////
// UP BUTTON method for displaying heart rate //

void heartRate(){
  int x = 10;
  
  while(x != 0){
    if(QS == true){
      fadeRate = 255;
      lcd.setCursor(0, 0);
      lcd.print("Heart rate: ");
      lcd.setCursor(12, 0);
      lcd.println(BPM);
      lcd.setCursor(0, 1);
      lcd.print("HEART BEAT FOUND");
      delay(500);
      QS = false;
      x--;
    }
  }
   if (BPM <= 59){    // color of backlight changing based on BPM value
     lcd.setBacklight(BLUE);
   }else if (BPM >=60 && BPM <= 80) {
     lcd.setBacklight(GREEN);
   }else if (BPM >= 81 && BPM <= 99){
     lcd.setBacklight(YELLOW);
   }else {
     lcd.setBacklight(RED);
   }
   heartval[h] = BPM;
   if(h < 5){
     h++;
   }else if(h == 5){
     h = 0;
     heartval[h] = BPM;
   }
   
   int i;
   for(i=0; i <5; i++){
     Serial.println(heartval[i]);
   }
   playcomplete("IMADONE.wav"); // plays audio saying it is done taking heart rate
}
////// mean function//////
float mean(){
 int i;
 int sum = 0;
 for(i=0; i<5; i++){
   sum += heartval[i];
 }
 return (float)sum / 5.0;
}
   

////////Averaging method ////////////
void averageH(){
  float average = 0;
  if(heartval[4] == 0){
    lcd.setCursor(5, 0);
    lcd.print("error.");
    lcd.setCursor(0,1);
    lcd.print("5 values needed");
  }else {
    average = mean();
    lcd.setCursor(0, 0);
    lcd.print("Average: ");
    lcd.setCursor( 9, 0 );
    lcd.print(average);
  }
}

    
   
