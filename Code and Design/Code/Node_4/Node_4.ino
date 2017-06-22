/*
   ///////////connections to Arduino Pins//////
  Light sensor
  SCL -> SCL (A5 on Arduino Uno, Leonardo, etc or 21 on Mega and Due)
  SDA -> SDA (A4 on Arduino Uno, Leonardo, etc or 20 on Mega and Due)
  XBEE IN 12 OUT 13
  Power Level A3
  Moisture A2
  PH A1
  temp y humidity D10
  light D4 D5
*/

#include <XBee.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <BH1750.h>
#include <LCD5110_Graph.h>
#include "DHT.h"
#include "LowPower.h"


XBee xbee = XBee();
BH1750 lightMeter;
DHT dht;

SoftwareSerial xbeeSerial(13, 12); //RX, TX


uint32_t mod1 = 0x41069FCF;
int id = 4;
float temp = 0;
float moisture = 0;
float light = 0;
float humidity = 0;
float ph = 0;
float pwlevel = 0;

////variables for Vernier PH sensor
unsigned long int avgValue;  //Store the average value of the sensor feedback
float b;
int buf[10],tem;
///
int lapse = 10; //minutes between readings

XBeeAddress64 addr64 = XBeeAddress64(0x0013a200, mod1);
ZBTxRequest zbTx ;
ZBTxStatusResponse txStatus = ZBTxStatusResponse();
ZBRxResponse rx = ZBRxResponse();

LCD5110 myGLCD(3,4,5,6,7);
extern unsigned char BigNumbers[];
extern unsigned char SmallFont[];
extern unsigned char TinyFont[];

void setup() {
  //pinMode(2, OUTPUT);
  //digitalWrite(2, HIGH);
  xbeeSerial.begin(9600);
  lightMeter.begin();
  myGLCD.InitLCD();
  Serial.begin(9600);
  dht.setup(10);
  xbee.setSerial(xbeeSerial);
  
}

void loop() {
  //////  reading light
  uint16_t lux = lightMeter.readLightLevel();
  light = float(lux);
  /////// reading PH
   for(int i=0;i<10;i++)       //Get 10 sample value from the sensor for smooth the value
  { 
    buf[i]=analogRead(A1);
    delay(10);
  }
  for(int i=0;i<9;i++)        //sort the analog from small to large
  {
    for(int j=i+1;j<10;j++)
    {
      if(buf[i]>buf[j])
      {
        tem=buf[i];
        buf[i]=buf[j];
        buf[j]=tem;
      }
    }
  }
  avgValue=0;
  for(int i=2;i<8;i++)                      //take the average value of 6 center sample
    avgValue+=buf[i];
  float phValue=(float)avgValue*5.0/1024/6; //convert the analog into millivolt
  phValue=3.5*phValue;   
  ph = phValue;
  ////reading humidity
  float h = dht.getHumidity();
  if (abs(h - humidity) < 10) {
    humidity = h;
  }
  //////reading temperature
  float t = dht.getTemperature();
  if (abs(t - temp) < 10) {
    temp = t;
  }
  /////reading moisture
  float m = analogRead(A2);
  moisture = map(m, 0, 1024, 100, 0);
  /////reading pwlevel
  float pw = analogRead(A3);
  pwlevel = map(pw, 0, 1024, 0, 10);

  String a = String(id) + "," + String(temp) + "," + String(moisture) + "," + String(light) + "," + String(humidity) + "," + String(ph) + "," + String(pwlevel);
  Serial.println(a);
  char payload [a.length()] = "";//the message to send to the Raspberry PI
  a.toCharArray(payload, a.length());

  zbTx = ZBTxRequest(addr64, payload, sizeof(payload));
  xbee.send(zbTx);

  writeLCD();//function that writes to the LCD screen
  
  delay(500); 
  if (xbee.readPacket(500)) {
    if (xbee.getResponse().getApiId() == ZB_TX_STATUS_RESPONSE) {
      xbee.getResponse().getZBTxStatusResponse(txStatus);
      Serial.println(txStatus.getDeliveryStatus());
      // get the delivery status, the fifth byte
      if (txStatus.getDeliveryStatus() == SUCCESS) {
        // success.  time to celebrate
        Serial.println("exito");
        sleepAr();
      } else {
        // the remote XBee did not receive our packet. is it powered on?
        Serial.println("error al recibir mensaje");
      }
    }
  } else if (xbee.getResponse().isError()) {
    //nss.print("Error reading packet.  Error code: ");
    //nss.println(xbee.getResponse().getErrorCode());
  } else {
    // local XBee did not provide a timely TX Status Response -- should not happen
    Serial.println("error al recibir mensaje");
  }

  
  

}

void speak(uint32_t mod, uint8_t payload[]) {
  XBeeAddress64 addr64 = XBeeAddress64(0x0013a200, mod);
  ZBTxRequest zbTx = ZBTxRequest(addr64, payload, sizeof(payload));
  xbee.send(zbTx);
  delay(500);
}

void hear() {
  xbee.readPacket();
  char a = xbeeSerial.read();
  Serial.println(a);

  if (xbee.getResponse().isAvailable()) {
    Serial.println(xbee.getResponse().getApiId(), HEX);
    Serial.println("hola!");

    if (xbee.getResponse().getApiId() == ZB_RX_RESPONSE) {
      Serial.println("ZB_RX_RESPONSE");
      xbee.getResponse().getZBRxResponse(rx);
      char mesg [rx.getDataLength ()] ;
      for (int i = 0; i < rx.getDataLength (); i++) {
        mesg[i] = rx.getData(i);
        Serial.print(mesg[i]);
        // recover(mesg);
      }
      String txt = mesg;
      Serial.println(txt);
    }
    else if ( xbee.getResponse().getApiId() == RX_16_RESPONSE  ) {
      Serial.println("RX_16_RESPONSE");
    }
    else if ( xbee.getResponse().getApiId() == RX_16_IO_RESPONSE  ) {
      Serial.println("RX_16_IO_RESPONSE");
    }
    else if (xbee.getResponse().getApiId() == ZB_IO_SAMPLE_RESPONSE) {
      Serial.println("ZB_IO_SAMPLE_RESPONSE");
    }
    else if ( xbee.getResponse().getApiId() == RX_64_RESPONSE  ) {
      Serial.println("RX_64_RESPONSE");
    }
    if (xbee.getResponse().isError()) {
      Serial.println("error");
      xbee.getResponse().getErrorCode();
    }
  }
  delay(500);
}
void sleepAr() {
  int mins = 15 * lapse;
  for (int i = 0 ;  i  <  mins ; i++) {
    //LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF); 
    LowPower.idle(SLEEP_4S, ADC_OFF, TIMER2_OFF, TIMER1_OFF, TIMER0_OFF, SPI_OFF, USART0_OFF, TWI_OFF);
  }
}

void writeLCD(){
    int linespace = 8; 
    myGLCD.clrScr(); 
    myGLCD.setFont(TinyFont); //seleccionamos la fuente smallfont
    myGLCD.print("Light ",LEFT,0*linespace); //imprimimos el texto en el centro
    myGLCD.printNumF(light,0, RIGHT, 0*linespace);
    myGLCD.print("Temperature ",LEFT, 1*linespace); 
    myGLCD.printNumF(temp,0, RIGHT,  1*linespace);
    myGLCD.print("Humidity ",LEFT, 2*linespace); 
    myGLCD.printNumF(humidity,0, RIGHT,  2*linespace);
    myGLCD.print("Moisture ",LEFT, 3*linespace); 
    myGLCD.printNumF(moisture,0, RIGHT,  3*linespace);
    myGLCD.print("PH ",LEFT, 4*linespace); 
    myGLCD.printNumF(ph,0, RIGHT,  4*linespace);
    myGLCD.print("Power Level ",LEFT, 5*linespace); 
    myGLCD.printNumF(pwlevel,0, RIGHT,  5*linespace);
   
    
    myGLCD.update();
}





