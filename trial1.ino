#include <FastLED.h>
#include <SPI.h>
#include <DMD2.h>
#include <fonts/SystemFont5x7.h>

const int threshold = 80;  // threshold value to decide when the detected sound is a knock or not
#define LED_PIN     2
#define NUM_LEDS    50
#define BRIGHTNESS  32
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
#define HIT_TIME 1000
#define MILLIS_PER_COLOR 20
#define HIT_ANI_INTERVAL 100
#define CSIZE 6

SoftDMD dmd(1,1);

CRGB leds[NUM_LEDS];

enum State {Off=0,Attacking = 1, Hit=2};

void report(){
    Serial.print(analogRead(A1));
    Serial.print(",");
    Serial.print(analogRead(A2));
    Serial.print(",");
    Serial.print(analogRead(A3));
    Serial.print(",");
    Serial.print(analogRead(A4));
    Serial.print(",");
    Serial.print(analogRead(A5));
    Serial.print(",");
    Serial.println();
}

class Target{
  public:
    Target(int sensorPin, uint8_t firstLed, CRGBPalette16 attackPalette, CRGBPalette16 hitPalette){
       this->sensorPin = sensorPin;
       this->firstLed = firstLed;
       this->attackPalette = attackPalette;
       this->hitPalette = hitPalette;
       state = Off;
    }

    void tick(){
      //Serial.println(String(state));
      switch (state) {
        case Off:
        break;
        case Hit:
          //Serial.println("in the hit " + String(millis()-lastHit));
          if (millis() - lastHit > HIT_TIME) {
            state = Off;
            blank();
          }
          else{
            if (millis() - lastHitAniUpdate > HIT_ANI_INTERVAL){
              for (int i=0; i < CSIZE; i++)  leds[i+firstLed] = ColorFromPalette(hitPalette, random(255));  
              lastHitAniUpdate = millis();
            }
          } 
        break;
        case Attacking:
            int sensorReading = analogRead(sensorPin);
            if (sensorReading >= threshold) {
              ::report();
              //Serial.println("Hit" + String(sensorReading));
              state = Hit;
              //Serial.println(String(state));
              lastHit = millis();
              lastHitAniUpdate = millis();
              animateCircle(firstLed, hitPalette);
            }
            else {
              animateCircle(firstLed, attackPalette);
            }
        break;
      }
    }

  void attack(){
    //Serial.println("called attack");
    state = Attacking;
  }

  void blank(){ 
    state = Off; 
    //Serial.println("turning off");
    for(int i=0; i < CSIZE; i++)
      ::leds[i + firstLed] = CRGB::Black;
  }

  
  State state;

  private:
    int sensorPin;
    int firstLed;
    unsigned long lastHit;
    CRGBPalette16 attackPalette;
    CRGBPalette16 hitPalette;
   
    unsigned long lastHitAniUpdate;
    

    static void animateCircle(uint8_t index, CRGBPalette16 palette){
      for (int i = 0; i <CSIZE; i++){
        uint8_t cur_col = millis()/MILLIS_PER_COLOR;
        leds[i+index] = ColorFromPalette(palette, (cur_col + 43*i)%255);
      }
    }

};
const uint8_t NUM_TARGETS = 5;
Target targets[NUM_TARGETS]{
  Target(A1, 0 , RainbowColors_p, LavaColors_p),
  Target(A2, 11, RainbowColors_p, LavaColors_p),
  Target(A3, 22, RainbowColors_p, LavaColors_p),
  Target(A4, 31, RainbowColors_p, LavaColors_p),
  Target(A5, 42, RainbowColors_p, LavaColors_p)
  };

void blankAll(){
  for (int i; i < ::NUM_TARGETS; i++) ::targets[i].blank();
}

class Game{
  public:
    virtual void tick()=0;
    bool running = true;
  protected:
    unsigned long startTime;

    void startup(){
      blankAll();
      //Serial.println(name + ", starting in...");
      for (int i = 3; i >0; i--) {
        
        //Serial.println(String(i) + "...");
        delay(1000);
      }
      startTime = millis();
    }

    String name;
    
};
Game* currentGame;



class HitFive : public Game{
  public:
    HitFive(){
      name = "Hit Five";
      startup();
      for (int i; i < NUM_TARGETS; i++) ::targets[i].attack();
    }

    virtual void tick(){
        if (running){
          bool over = true;
          for (int i; i < NUM_TARGETS && over; i++)
            over = (::targets[i].state != Attacking);
          if (over) {
            running = false;
            //Serial.println("Game over you hit them all in " + String(millis()-startTime));
          }
        }
    }
};

class Invasion : public Game{
  public:
    Invasion(){
      name = "Invasion";
      startup();
      targets[random(NUM_TARGETS)].attack();
      lastUpdate = millis();
      dmd.drawString(0,0,String(gameLength/1000) + "sec");
      dmd.drawString(0,8,"*   *");
    }

  virtual void tick(){
    if (running){
      if (millis() - startTime > gameLength){
        end();
      }
      else{
        if (millis()-lastUpdate > updateInterval){
          //Serial.println("Game time: " + String(millis()-startTime));
          
          int timeLeftSec =(gameLength - millis() + startTime)/1000;
          if (timeLeftSec >= 10)
            dmd.drawString(0,0,String(timeLeftSec));
          else
            dmd.drawString(0,0," "+ String(timeLeftSec));

          bool atLeastOneNotAttacking = false;
          for (int i; i < NUM_TARGETS; i++){          
            if (::targets[i].state == Attacking) {
              invasions +=1;
            }
            else {
              attackMeter += 1;
              atLeastOneNotAttacking = true;
            }
          }        
          dmd.drawString(6,8,String(invasions));  
          if (atLeastOneNotAttacking && attackMeter > attackMeterFullValue)
          {
            uint8_t nextAttacker;
            do {
              nextAttacker = random(NUM_TARGETS); 
              //Serial.println("picked" + String(nextAttacker));
            } 
            while (::targets[nextAttacker].state == Attacking);
            targets[nextAttacker].attack();
            attackMeter -= attackMeterFullValue;
          }
          lastUpdate = lastUpdate + updateInterval;
        }
      }
    }
  }

  void end(){
    running = false;
    //Serial.println("Game Over. You suffered " + String(invasions) + " damage before repelling the enemy." );
  }

  private:
    unsigned long lastUpdate;
    const unsigned long updateInterval = 200;
    int invasions = 0;
    int attackMeter = 0;
    const int attackMeterFullValue = 50;
    const unsigned long gameLength = 60000;

};

void setup() {
  Serial.begin(9600); 
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness(  BRIGHTNESS );
  randomSeed(analogRead(A0));
  
  dmd.setBrightness(255);
  dmd.selectFont(SystemFont5x7);
  dmd.begin();

  delay(1000);
  //Serial.println("Setup complete");
  currentGame = new Invasion();
}

unsigned long lastReport = 0;
void loop() {

  for (int i = 0; i < ::NUM_TARGETS; i++){
    targets[i].tick();
    currentGame->tick();
  }
  FastLED.show();
  
  //output
  if (millis() - lastReport > 150){
    lastReport = millis();
    report();    
  }
}




