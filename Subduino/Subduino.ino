#include <SoftwareSerial.h>

// 06 SpecB Rom ID
// 4512783106

// Clutch and Brake Switch Info
// 72  (0x48) [01001000] = Brake On
// 128 (0x80) [10000000] = Clutch On
// 200 (0xC8) [11001000] = Both On

// Can Stuff
#include <Arduino.h>
#include <CAN.h>
#include <SPI.h>
#include <CAN_MCP2515.h>

// Define our CAN speed (bitrate).
#define bitrate CAN_BPS_500K

SoftwareSerial sendSerial = SoftwareSerial(5, 6); //Rx, Tx

int ECUbytes[7] = {0, 0, 0, 0, 0, 0, 0};
//                                      |rpm - 2 bytes| tmp |  spd |  swt | gear   | tps |
//byte ReqRPM[28] = {128,16,240,23,168,0,0,0,15,0,0,14,0,0,8,0,0,16,0,1,33,255,83,5,0,0,21,2}; // add throttle
int RRPM;
int BRAKE;
int CLUTCH;
int TPS;
int MPH;
int TMP;
int GEAR;


byte ReqRPM[13] = {128,16,240,8,168,0,0,0,15,0,0,14,77};
int readDelay = 1;
unsigned long prvTime;
unsigned long curTime;

void setup()
{
  Serial.begin(115200); //for diagnostics
    while (!Serial) {
      // wait
    }
  Serial.println("Connecting to Serial");
  sendSerial.begin(4800); //SSM uses 4800 8N1 baud rate
    while (!sendSerial) {
      //wait
      delay(50);
  }
  Serial.println("Serial Line Established");
  Serial.println("Can Init");
  CAN.begin(bitrate);
  delay(50);
  Serial.println("Setup Complete");
  delay(50);
}

void loop()
{
curTime = millis();
int milli=curTime - prvTime;
prvTime = curTime;

writeSSM(ReqRPM, 13, sendSerial);
  if (readECU(ECUbytes, 7, false))
  { 
  RRPM = (ECUbytes[0] | (ECUbytes[1] << 8)) /4.0;  
  CLUTCH = ((ECUbytes[4] & 0x80) >> 7);
  BRAKE = ((ECUbytes[4] & 0x40) >> 6);
  TPS = ((ECUbytes[6] * 100) / 255);
  MPH = (ECUbytes[3] * 0.621371192);
  TMP = ((32+9*(ECUbytes[2]-40))/5);
  if (CLUTCH == 1) {
    GEAR=0;
  }else{
    GEAR=ECUbytes[5];
  }
  Serial.print("RPM: ");
  Serial.print(RRPM);
  Serial.print("    ");
  Serial.print("SPEED: ");
  Serial.print(MPH);
  Serial.print("    ");
  Serial.print("TEMP: ");
  Serial.print(TMP);
  Serial.print("    ");
  Serial.print("Brake:");
  Serial.print(BRAKE);
  Serial.print("    ");
  Serial.print("Clutch::");
  Serial.print(CLUTCH);
  Serial.print("    ");
  Serial.print("GEAR:");
  Serial.print(GEAR);
  Serial.print("    ");
  Serial.print("Throtle: ");
  Serial.print("    ");
  Serial.print(TPS);
  Serial.print("    ");
  Serial.print("Millis: ");
  Serial.println(milli);
  
  RPMmessage(ECUbytes[0],ECUbytes[1]);
  delay(3);
  SPEEDmesage(MPH);
  delay(3);
  TMPmessage(TMP);
  delay(3);
  SWITCHmessage(BRAKE,CLUTCH);
  delay(3);
  GEARmessage(GEAR);
  delay(3);
  TPSmessage(TPS);
  }

}

/* returns the 8 least significant bits of an input byte*/
byte CheckSum(byte sum) {
  byte counter = 0;
  byte power = 1;
  for (byte n = 0; n < 8; n++) {
    counter += bitRead(sum, n) * power;
    power = power * 2;
  }
  return counter;
}

/*writes data over the software serial port
the &digiSerial passes a reference to the external
object so that we can control it outside of the function*/
void writeSSM(byte data[], byte length, SoftwareSerial &digiSerial) {
  //Serial.println(F("Sending packet... "));
  for (byte x = 0; x < length; x++) {
    digiSerial.write(data[x]);
  }
}

//this will change the values in dataArray, populating them with values respective of the poll array address calls
boolean readECU(int* dataArray, byte dataArrayLength, boolean nonZeroes)
{
  byte data = 0;
  boolean isPacket = false;
  byte sumBytes = 0;
  byte checkSumByte = 0;
  byte dataSize = 0;
  byte bytePlace = 0;
  byte zeroesLoopSpot = 0;
  byte loopLength = 10;
  for (byte j = 0; j < loopLength; j++)
  {
    data = sendSerial.read();
    delay(readDelay);

    if (data == 128 && dataSize == 0) { //0x80 or 128 marks the beginning of a packet
      isPacket = true;
      j = 0;
      //Serial.println("Begin Packet");
    }

    //terminate function and return false if no response is detected
    if (j == (loopLength - 1) && isPacket != true)
    {
      return false;
    }

    if (isPacket == true && data != -1) {
      Serial.print(data); // for debugging: shows in-packet data
      Serial.print(" ");

      if (bytePlace == 3) { // how much data is coming
        dataSize = data;
        loopLength = data + 6;
      }

      if (bytePlace > 4 && bytePlace - 5 < dataArrayLength && nonZeroes == false)
      {
        dataArray[bytePlace - 5] = data;
      }
      else if (bytePlace > 4 && zeroesLoopSpot < dataArrayLength / 2 && nonZeroes == true && data != 0 && bytePlace < dataSize + 4)
      {
        dataArray[zeroesLoopSpot] = data;
        dataArray[zeroesLoopSpot + (dataArrayLength / 2)] = bytePlace;
        zeroesLoopSpot++;
      }

      bytePlace += 1; //increment bytePlace

      //once the data is all recieved, checksum and re-set counters
     // Serial.print("byte place: ");
     // Serial.println(bytePlace);
      if (bytePlace == dataSize + 5) {
        checkSumByte = CheckSum(sumBytes);  //the 8 least significant bits of sumBytes

        if (data != checkSumByte) {
          Serial.println(F("checksum error"));
          return false;
        }
//        Serial.println("Checksum is good");
        isPacket = false;
        sumBytes = 0;
        bytePlace = 0;
        checkSumByte = 0;
        dataSize = 0;
        return true;
      }
      else {
        sumBytes += data; // this is to compare with the checksum byte
        //Serial.print(F("sum: "));
        //Serial.println(sumBytes);
      }
    }
  }
}

void RPMmessage(byte Rpm1, byte Rpm2)
{
  CAN_Frame standard_message; // Create message object to use CAN message structure
  standard_message.id = 0x259; // 601
  standard_message.valid = true;
  standard_message.rtr = 0;
  standard_message.extended = CAN_STANDARD_FRAME;
  standard_message.length = 5; // Data length
  standard_message.data[1] = Rpm1;
  standard_message.data[2] = Rpm2;
  CAN.write(standard_message); // Load message and send
}

void SPEEDmesage(byte Kph)
{
  CAN_Frame standard_message; // Create message object to use CAN message structure
  standard_message.id = 0x25A; // 602
  standard_message.valid = true;
  standard_message.rtr = 0;
  standard_message.extended = CAN_STANDARD_FRAME;
  standard_message.length = 5; // Data length
  standard_message.data[1] = Kph;
  CAN.write(standard_message); // Load message and send
}

void TMPmessage(byte Tmp)
{
  CAN_Frame standard_message; // Create message object to use CAN message structure

  standard_message.id = 0x25B; // 603
  standard_message.valid = true;
  standard_message.rtr = 0;
  standard_message.extended = CAN_STANDARD_FRAME;
  standard_message.length = 5; // Data length
  standard_message.data[1] = Tmp;
  CAN.write(standard_message); // Load message and send
}
void SWITCHmessage(byte Brk, byte Clu)
{
  CAN_Frame standard_message; // Create message object to use CAN message structure
  standard_message.id = 0x25C; // 604
  standard_message.valid = true;
  standard_message.rtr = 0;
  standard_message.extended = CAN_STANDARD_FRAME;
  standard_message.length = 5; // Data length
  standard_message.data[1] = Brk; // Brake
  standard_message.data[2] = Clu; // Clutch
  CAN.write(standard_message); // Load message and send
}
void GEARmessage(byte Ger)
{
  CAN_Frame standard_message; // Create message object to use CAN message structure
  standard_message.id = 0x25D; // 605
  standard_message.valid = true;
  standard_message.rtr = 0;
  standard_message.extended = CAN_STANDARD_FRAME;
  standard_message.length = 5; // Data length
  standard_message.data[1] = Ger; 
  CAN.write(standard_message); // Load message and send
}

void TPSmessage(byte Tps)
{
  CAN_Frame standard_message; // Create message object to use CAN message structure
  standard_message.id = 0x25E; // 606
  standard_message.valid = true;
  standard_message.rtr = 0;
  standard_message.extended = CAN_STANDARD_FRAME;
  standard_message.length = 5; // Data length
  standard_message.data[1] = Tps; 
  CAN.write(standard_message); // Load message and send
}
