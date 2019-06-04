
/*
Right now it should log all data to the SD card, some things in config.h may need to be cahnged but it should at least work
TODOS: 
Add time logging, properly adhere to TREB file format spec
*/
#include <Wire.h>
#include <SD.h>

#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"
#include "config.h"

String logFileName; // Active logging file
String logFileBuffer; // Buffer for logged data. Max is set in config
const byte MLX90640_address = 0x33; //Default 7-bit unshifted address of the MLX90640

#define TA_SHIFT 8 //Default shift for MLX90640 in open air

static float mlx90640To[768];
paramsMLX90640 mlx90640;

void setup()
{
  initHardware();
  Wire.begin();
  Wire.setClock(400000); //Increase I2C clock speed to 400kHz
  if (isConnected() == false)
  {
    LOG_PORT.println("MLX90640 not detected at default I2C address. Please check wiring. Freezing.");
    while (1);
  }
  LOG_PORT.println("MLX90640 online!");
  if ( initSD() )
  {
    sdCardPresent = true;
    // Get the next, available log file name
    logFileName = nextLogFile(); 
    logFile = SD.open(logFileName, FILE_WRITE);
    startFile(logFile);
    logFile.close();
  }
  //Get device parameters - We only have to do this once
  int status;
  uint16_t eeMLX90640[832];
  status = MLX90640_DumpEE(MLX90640_address, eeMLX90640);
  if (status != 0)
    LOG_PORT.println("Failed to load system parameters");

  status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640);
  if (status != 0)
    LOG_PORT.println("Parameter extraction failed");

  //Once params are extracted, we can release eeMLX90640 array
}

void loop()
{
  for (byte x = 0 ; x < 2 ; x++) //Read both subpages
  {
    uint16_t mlx90640Frame[834];
    int status = MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);
    if (status < 0)
    {
      LOG_PORT.print("GetFrame Error: ");
      LOG_PORT.println(status);
    }

    float vdd = MLX90640_GetVdd(mlx90640Frame, &mlx90640);
    float Ta = MLX90640_GetTa(mlx90640Frame, &mlx90640);

    float tr = Ta - TA_SHIFT; //Reflected temperature based on the sensor ambient temperature
    float emissivity = 0.95;
    byte *a = (byte *)&vdd;
    byte *b = (byte *)&tr;
    byte toStore[846] = {83,77,67,83,a[0],a[1],a[2],a[3],b[0],b[1],b[2],b[3]};
    for(int i=0;i<834;i++){
      byte *c = (byte *)&mlx90640Frame[i];
      toStore[2*i+12] = c[0];
      toStore[2*i+13] = c[1];
    }
    sdLogByteArray(toStore);
    //MLX90640_CalculateTo(mlx90640Frame, &mlx90640, emissivity, tr, mlx90640To);
  }
  blinkLED();
  delay(1000);
}

//Returns true if the MLX90640 is detected on the I2C bus
boolean isConnected()
{
  Wire.beginTransmission((uint8_t)MLX90640_address);
  if (Wire.endTransmission() != 0)
    return (false); //Sensor did not ACK
  return (true);
}

void initHardware(void)
{
  // Set up LED pin (active-high, default to off)
  pinMode(HW_LED_PIN, OUTPUT);
  digitalWrite(HW_LED_PIN, LOW);

  // Set up MPU-9250 interrupt input (active-low)
  pinMode(MPU9250_INT_PIN, INPUT_PULLUP);

  // Set up serial log port
  LOG_PORT.begin(SERIAL_BAUD_RATE);
}
void blinkLED()
{
  static bool ledState = false;
  digitalWrite(HW_LED_PIN, ledState);
  ledState = !ledState;
}
bool initSD(void)
{
  // SD.begin should return true if a valid SD card is present
  if ( !SD.begin(SD_CHIP_SELECT_PIN) )
  {
    return false;
  }

  return true;
}
bool startFile(File f){
  byte header[] = {116,114,101,98};
  file.write(header);
}
bool sdLogByteArray(byte[] toLog)
{
  // Open the current file name:
  File logFile = SD.open(logFileName, FILE_WRITE);
  
  // If the file will get too big with these new bytes, create
  // a new one, and open it.
  if (logFile.size() > (SD_MAX_FILE_SIZE - toLog.length))
  {
    logFileName = nextLogFile();
    logFile = SD.open(logFileName, FILE_WRITE);
    startFile(logFile);
  }

  // If the log file opened properly, add the byte array to it.
  if (logFile)
  {
    logFile.write(toLog, toLog.length);
    logFile.close();

    return true; // Return success
  }

  return false; // Return fail
}

// Find the next available log file. Or return a null string
// if we've reached the maximum file limit.
String nextLogFile(void)
{
  String filename;
  int logIndex = 0;

  for (int i = 0; i < LOG_FILE_INDEX_MAX; i++)
  {
    // Construct a file with PREFIX[Index].SUFFIX
    filename = String(LOG_FILE_PREFIX);
    filename += String(logIndex);
    filename += ".";
    filename += String(LOG_FILE_SUFFIX);
    // If the file name doesn't exist, return it
    if (!SD.exists(filename))
    {
      return filename;
    }
    // Otherwise increment the index, and try again
    logIndex++;
  }

  return "";
}
