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
byte ReqData[28] = {128,16,240,23,168,0,0,0,15,0,0,14,0,0,8,0,0,16,0,1,33,255,83,5,0,0,21,2}; // add throttle
byte ReqDataSize = 28;
int RRPM;
int BRAKE;
int CLUTCH;
int TPS;
int MPH;
int TMP;
int GEAR;
int SerialStatus=0;
int milli;
int ClrToSnd;


//byte ReqData[13] = {128,16,240,8,168,0,0,0,15,0,0,14,77};
//byte ReqDataSize = 13;
int readDelay = 2;
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
  writeSSM(ReqData, ReqDataSize, sendSerial);
  delay (2);
}

void loop()
{
curTime = millis();
milli=curTime - prvTime;  

if (milli > 250) {
  sendSerial.flush();
  //delay(5);
  writeSSM(ReqData, ReqDataSize, sendSerial);
  Serial.println("Timer Popped");
  prvTime=millis();
  }

  if (sendSerial.available()) {  
    readECU(ECUbytes, 7, false);
    
    prvTime = curTime;

    RRPM = (ECUbytes[0] | (ECUbytes[1] << 8)) /4.0;  
    CLUTCH = ((ECUbytes[4] & 0x80) >> 7); 
    BRAKE = ((ECUbytes[4] & 0x40) >> 6);
    TPS = ((ECUbytes[6] * 100) / 255);
    MPH = (ECUbytes[3] * 0.621371192);
    TMP = ((32+9*(ECUbytes[2]-40))/5);
    if (CLUTCH == 1 || MPH == 0) {
      GEAR=0;
    }

    CAN2RCP1(ECUbytes[0],ECUbytes[1], MPH, TPS);
    delay(1);
    CAN2RCP2(BRAKE, CLUTCH, GEAR, TMP);
    
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
    //delay(20);    
    }
    
    if (ClrToSnd == 0) {
      writeSSM(ReqData, ReqDataSize, sendSerial);
      ClrToSnd = 1;
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
  byte loopLength = 20;
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
  //    Serial.print(data); // for debugging: shows in-packet data
  //    Serial.print(" ");

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
        ClrToSnd = 0;
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

void CAN2RCP1(byte Rpm1, byte Rpm2, byte Kph, byte Tps)
{
  CAN_Frame standard_message; // Create message object to use CAN message structure
  standard_message.id = 0x259; // 601
  standard_message.valid = true;
  standard_message.rtr = 0;
  standard_message.extended = CAN_STANDARD_FRAME;
  standard_message.length = 4; // Data length
  standard_message.data[0] = Rpm1;
  standard_message.data[1] = Rpm2;
  standard_message.data[2] = Kph;
  standard_message.data[3] = Tps;
  CAN.write(standard_message); // Load message and send
}

void CAN2RCP2(byte Brk, byte Clu, byte Ger, byte Tmp)
{
  CAN_Frame standard_message; // Create message object to use CAN message structure
  standard_message.id = 0x25A; // 601
  standard_message.valid = true;
  standard_message.rtr = 0;
  standard_message.extended = CAN_STANDARD_FRAME;
  standard_message.length = 4; // Data length
  standard_message.data[0] = Brk;
  standard_message.data[1] = Clu;
  standard_message.data[2] = Ger;
  standard_message.data[3] = Tmp;
  CAN.write(standard_message); // Load message and send
}

