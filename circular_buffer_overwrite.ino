#include <Arduino.h>
#include <HardwareSerial.h>
#include <SPI.h>
#include <SD.h>
#include <SoftWire.h>
#include <string.h>
#include <ctype.h>
#include <Stream.h>

#define ENTRY_SIZE 64      
#define BUFFER_SIZE 10    
#define DATA_FILENAME "dlogs2.dat"
#define INDEX_FILENAME "DHEADER.dat"

//initialize RTC
#define MCP7940N_ADDRESS 0x6F
#define MFP_PIN PA1
SoftWire rtcWire(PB8, PB9, SOFT_STANDARD);

// RTC Time variables
uint8_t rtcSeconds, rtcMinutes, rtcHours, rtcDay, rtcDate, rtcMonth, rtcYear;
char rtcDateTimeStr[64];
bool rtcInitialized = false;


char rtcLogDate[7];  
char rtcLogTime[7];  

///for GPS
char byteGPS;
char linea[300] = "";
char comandoGNRMC[7] = "$GNRMC";
char comandoGNGGA[7] = "$GNGGA";
char comandoGNGLL[7] = "$GNGLL";

int cont=0;
int bien1=0;
int bien2=0;
int bien3=0;
int conta=0;
int indices[13];
int r;

int gnrmc_fix_flag;
int gngga_fix_flag;
int gngll_fix_flag;

bool exit_gnrmc,exit_gngga,exit_gngll;

char logi[12];
char lati[12];
char latdir[3];
char longdir[3];
char fix[3];

char temp_var[12];
int gps_flag;

volatile int lat_pointer=0;
volatile int latdir_pointer=0;
volatile int long_pointer=0;
volatile int longdir_pointer=0;
volatile int fix_pointer=0;

bool gps_exit_flag=0;


#define SD_CS   PB12    // CS
#define SD_SCK  PB13    // SCK
#define SD_MISO PB14    // MISO
#define SD_MOSI PB15    // MOSI


SPIClass SPI_2(2);  


typedef struct
 {
uint32_t magicData;          
uint32_t currentPosition;     
uint32_t overflowFlag;       
uint32_t totalEntry;   
uint32_t lastEntrytime;  
}BufferIndex;

BufferIndex deviceHeader;
bool bufferInitialized=0;
char *substrings[7];
int total_parsing=0;
char parsed_data[15];

void get_each_data(char* string_2_parse)
{ 
int length;
int temp=0;
int flag=0;
length=strlen(string_2_parse);
  
for(int i=0;i<length;i++)
{
if(flag==1)
{
 parsed_data[temp]=string_2_parse[i];
temp++;
 }
if(string_2_parse[i]=='-')
{
flag=1;
temp=0;
}
}
parsed_data[temp]='\0';
}

void newline_string_parsing(char* string_2_parse)
{
int temp_data;
int looping=0;
for (looping = 0; looping < 5; looping++)
{
substrings[looping] = 0;
}
char* newline_sep_string = (char*)strtok(string_2_parse,"\n");
    
looping = 0;
total_parsing=0;
while (newline_sep_string != 0 && looping < 50)
{
total_parsing=looping;
if(strstr((char *)newline_sep_string,"END")!=NULL)
{
substrings[looping++] = newline_sep_string;
total_parsing=looping;
break;
}
substrings[looping++] = newline_sep_string;
newline_sep_string = strtok(0,"\n");
}
for(looping=0;looping<5;looping++)
{
get_each_data(substrings[looping]);
temp_data = atoi(parsed_data);
switch(looping)
{
case 0:
deviceHeader.magicData=temp_data;
break;
case 1:
deviceHeader.currentPosition=temp_data;
break;
case 2:
deviceHeader.overflowFlag=temp_data;
break;
case 3:
deviceHeader.totalEntry=temp_data;
        
break;
case 4:
deviceHeader.lastEntrytime=temp_data;
break;
}
}
}

bool writeStruct(const char* filename, const BufferIndex& data)
{
char buffer_var[55];
SPI.setModule(2);
  
 
if (SD.exists(filename)) {
SD.remove(filename);
}
  
File dataFile = SD.open(filename, FILE_WRITE);
if (dataFile) 
  {
      sprintf(buffer_var,"magicData-%u\n",data.magicData);
      dataFile.print(buffer_var);
      sprintf(buffer_var,"currentPosition-%u\n",data.currentPosition);
      dataFile.print(buffer_var);
      sprintf(buffer_var,"overflowFlag-%u\n",data.overflowFlag);
      dataFile.print(buffer_var);
      sprintf(buffer_var,"totalEntry-%u\n",data.totalEntry);
      dataFile.print(buffer_var);
      sprintf(buffer_var,"lastEntrytime-%u\n",data.lastEntrytime);
      dataFile.print(buffer_var);      
      dataFile.flush();
      dataFile.close();
      return true;
  }
  else
  {
    Serial2.println("Error Write");
    return false;
  }
}

bool readStruct(const char* filename, BufferIndex& data)
{ 
  SPI.setModule(2);
  File dataFile = SD.open(filename);
  if (dataFile) 
  {
    long fileSize = dataFile.size();
    char* buffer_var = new char[fileSize + 1];
    dataFile.read(buffer_var, fileSize);
    buffer_var[fileSize] = '\0';
    //Serial2.write(buffer_var);
    dataFile.close();
    newline_string_parsing(buffer_var);
    return true;
  } 
  else
  {
    Serial2.println("Error Read");
    return false;
  }
}

bool saveIndex()
{
  if(writeStruct(INDEX_FILENAME,deviceHeader))
  {
    Serial2.println("updated index");
    return true;
  }
  else
  {
    Serial2.println("failed to save index");
    return false;
  }
}

bool loadIndex()
{
  if (readStruct(INDEX_FILENAME, deviceHeader)) 
  {
    if(deviceHeader.magicData==0xa1b2c3)
    {
      Serial2.print("M: ");
      Serial2.println(deviceHeader.magicData,HEX);
      Serial2.print("C Pos: ");
      Serial2.println(deviceHeader.currentPosition);
      Serial2.print("OF: ");
      Serial2.println(deviceHeader.overflowFlag);
      Serial2.print("TEntry: ");
      Serial2.println(deviceHeader.totalEntry);
      Serial2.print("OF: ");
      Serial2.println(deviceHeader.lastEntrytime);
      return true;
    }
   else
   {
     return false;
   }
  }
  else
  {
    return false;
  }
}

bool deleteIndex()
{
  if (!SD.remove(INDEX_FILENAME)) {
    Serial2.println("Failed to remove file.");
    return false;
  } else {
    Serial2.println("File removed successfully!");
    return true;
  }
}

bool initializeBuffer()
{
  File dataFile;
  deviceHeader.magicData=0; 
  deviceHeader.currentPosition=0;  
  deviceHeader.overflowFlag=0;   
  deviceHeader.totalEntry=0; 
  deviceHeader.lastEntrytime=0; 

  if(loadIndex())
  {
    if(deviceHeader.currentPosition>=BUFFER_SIZE)
    {
      Serial2.println("Invalid currentPos");
      deviceHeader.currentPosition=0;
    }
  }
  else
  {
    deviceHeader.magicData=0xa1b2c3; 
    deviceHeader.currentPosition=0;  
    deviceHeader.overflowFlag=0;   
    deviceHeader.totalEntry=0; 
    deviceHeader.lastEntrytime=0; 
    deleteIndex();
    saveIndex();
  }
  

  SPI.setModule(2);
  
  if (!SD.exists(DATA_FILENAME)) {
    Serial2.println("Creating data file with proper size...");
    dataFile = SD.open(DATA_FILENAME, FILE_WRITE);
    if (dataFile) {
      
      char zeroBuffer[ENTRY_SIZE];
      memset(zeroBuffer, 0, ENTRY_SIZE);
      
      for (int i = 0; i < BUFFER_SIZE; i++) {
        dataFile.write((uint8_t*)zeroBuffer, ENTRY_SIZE);
      }
      
      dataFile.flush();
      dataFile.close();
      Serial2.println("Data file created with proper size");
    } else {
      Serial2.println("Failed to create data file!");
      return false;
    }
  } else {
  
    dataFile = SD.open(DATA_FILENAME, FILE_READ);
    if (dataFile) {
      uint32_t fileSize = dataFile.size();
      dataFile.close();
      
      Serial2.print("Verified file size: ");
      Serial2.print(fileSize);
      Serial2.print(" bytes (expected ");
      Serial2.print(BUFFER_SIZE * ENTRY_SIZE);
      Serial2.println(" bytes)");
      
    
      if (fileSize != BUFFER_SIZE * ENTRY_SIZE) {
        Serial2.println("File size incorrect, recreating...");
        SD.remove(DATA_FILENAME);
        
        dataFile = SD.open(DATA_FILENAME, FILE_WRITE);
        if (dataFile) {
          char zeroBuffer[ENTRY_SIZE];
          memset(zeroBuffer, 0, ENTRY_SIZE);
          
          for (int i = 0; i < BUFFER_SIZE; i++) {
            dataFile.write((uint8_t*)zeroBuffer, ENTRY_SIZE);
          }
          
          dataFile.flush();
          dataFile.close();
          Serial2.println("Data file recreated with proper size");
        } else {
          Serial2.println("Failed to recreate data file!");
          return false;
        }
      }
    } else {
      Serial2.println("Failed to open data file for size verification!");
      return false;
    }
  }
  
  bufferInitialized = 1;
  return true;  
}


// bool addBufferEntry(const char* entry, uint32_t timestamp) {
//   if (!bufferInitialized) {
//     Serial2.println("Buffer not initialized - cannot add entry");
//     return false;
//   }

//   File dataFile;
//   SPI.setModule(2);
  
//   for (int retry = 0; retry < 3; retry++) {

//     dataFile = SD.open(DATA_FILENAME, FILE_WRITE);
//     if (dataFile) break;
//     delay(50); 


//     Serial2.print("Retry opening data file: ");
//     Serial2.println(retry + 1);
//   }
  
//   if (!dataFile) {
//     Serial2.println("Failed to open data file after multiple attempts");
//     return false;
//   }
  
//   uint32_t writePos = deviceHeader.currentPosition * ENTRY_SIZE;
  
 
//   if (!dataFile.seek(writePos)) {
//     Serial2.print("Failed to seek to position: ");
//     Serial2.println(writePos);
//     dataFile.close();
//     return false;
//   }
  
//   Serial2.print("Writing at position: ");
//   Serial2.print(deviceHeader.currentPosition);
//   Serial2.print(" (byte offset: ");
//   Serial2.print(writePos);
//   Serial2.println(")");
  
 
//   char entryBuffer[ENTRY_SIZE];
//   memset(entryBuffer, 0, ENTRY_SIZE); 
  
 
//   size_t entryLen = strlen(entry);

//   if (entryLen > ENTRY_SIZE - 1) {

//     entryLen = ENTRY_SIZE - 1; 
//   }

//   memcpy(entryBuffer, entry, entryLen);
//   entryBuffer[entryLen] = '\0'; 
  
//   Serial2.print("Entry: ");
//   Serial2.println(entryBuffer);
  
  
//   size_t bytesWritten = dataFile.write((uint8_t*)entryBuffer, ENTRY_SIZE);

  
//   dataFile.flush();
//   dataFile.close();
  
 
//   uint32_t nextPosition = (deviceHeader.currentPosition + 1) % BUFFER_SIZE;
  
 
//   if (nextPosition == 0) {
//     deviceHeader.overflowFlag = 1;
//     Serial2.println("*** Buffer overflow detected - will start overwriting oldest entries ***");
//   }
  
//   deviceHeader.currentPosition = nextPosition;
  
//   // Update metadata
//   deviceHeader.totalEntry++;
//   deviceHeader.lastEntrytime = timestamp;

//   // Save updated index
//   bool indexSaveSuccess = false;
//   for (int retry = 0; retry < 3; retry++) {

//     if (deleteIndex() && saveIndex()) {
//       indexSaveSuccess = true;
//       break;
//     }

//     delay(50); 
//     Serial2.print("Retry saving index: ");
//     Serial2.println(retry + 1);
//   }
  
//   if (!indexSaveSuccess) {
//     Serial2.println("Warning: Failed to update index after multiple attempts");
    
//   }
  
//   return true;
// }

bool addBufferEntry(const char* entry, uint32_t timestamp) 
{
  File dataFile;
  uint32_t temporary_position=0;
  uint32_t max_size_page=0;
  int flag=0;
  if (!bufferInitialized)
  {
    return false;
  }

  SPI.setModule(2);


  dataFile = SD.open(DATA_FILENAME, O_RDWR );
  if (!dataFile)
  {
    return false;
  }
    // Calculate position and seek
  uint32_t writePos = deviceHeader.currentPosition * ENTRY_SIZE;
  dataFile.seek(writePos);

    Serial2.print("POS");
  Serial2.println(writePos);

  char entryBuffer[ENTRY_SIZE+2];
  memset(entryBuffer, ' ', ENTRY_SIZE);
  strncpy(entryBuffer, entry, ENTRY_SIZE - 1);
  Serial2.println(entryBuffer);
  entryBuffer[64]='\0';

  size_t bytesWritten = dataFile.print(entryBuffer);
  //dataFile.flush();
  dataFile.close();


  // Increment position, with wrap-around when needed
  temporary_position = (deviceHeader.currentPosition + 1);
  if(temporary_position>=BUFFER_SIZE)
  {
    deviceHeader.overflowFlag = 1;
    deviceHeader.currentPosition =0;
    flag=9;
  }
  else
  {
      max_size_page = 15*64;
    if(writePos>=max_size_page)
    {
      deviceHeader.currentPosition=0;
    }  
    else
    {
      deviceHeader.currentPosition = temporary_position;
    }  
     flag=9;
  }
  if(flag==9)
  {
    deviceHeader.totalEntry++;
    deviceHeader.lastEntrytime=timestamp;
    flag=0;
    deleteIndex();
    saveIndex();
  }
  return true;
}


bool get_data_logs() {
  File dataFile;
  SPI.setModule(2);
  
  dataFile = SD.open(DATA_FILENAME, FILE_READ);
  if (!dataFile) {
    Serial2.println("Failed to open data file for reading");
    return false;
  }
  
  Serial2.println("Reading buffer entries:");
  
  if (deviceHeader.overflowFlag == 1) {
    
    Serial2.println("Buffer has overflowed - reading in proper order (oldest to newest):");
    
    
    Serial2.println("Oldest entries (from current position to end):");
    for (uint32_t i = deviceHeader.currentPosition; i < BUFFER_SIZE; i++) {
      uint32_t readPos = i * ENTRY_SIZE;
      dataFile.seek(readPos);
      
      char entryBuffer[ENTRY_SIZE + 1];
      dataFile.read((uint8_t*)entryBuffer, ENTRY_SIZE);
      entryBuffer[ENTRY_SIZE] = '\0';
      
      Serial2.print("Entry ");
      Serial2.print(i);
      Serial2.print(": ");
      Serial2.println(entryBuffer);
    }
    
    
    Serial2.println("Newest entries (from beginning to current position):");
    for (uint32_t i = 0; i < deviceHeader.currentPosition; i++) {
      uint32_t readPos = i * ENTRY_SIZE;
      dataFile.seek(readPos);
      
      char entryBuffer[ENTRY_SIZE + 1];
      dataFile.read((uint8_t*)entryBuffer, ENTRY_SIZE);
      entryBuffer[ENTRY_SIZE] = '\0';
      
      Serial2.print("Entry ");
      Serial2.print(i);
      Serial2.print(": ");
      Serial2.println(entryBuffer);
    }
  } 
    

  
  
  dataFile.close();
  return true;
}

void read_file(const char* filename) {
  File dataFile = SD.open(filename);
  if (dataFile) {
    Serial2.print("Reading file: ");
    Serial2.println(filename);
    Serial2.print("File size: ");
    Serial2.print(dataFile.size());
    Serial2.println(" bytes");
    
    int bytesRead = 0;
    while (dataFile.available()) {
      Serial2.write(dataFile.read());
      bytesRead++;
      
      
      if (bytesRead % 64 == 0) {
        Serial2.println();
      }
    }
    
    dataFile.close();
    Serial2.println("\nDone reading file");
  } else {
    Serial2.print("Failed to open file: ");
    Serial2.println(filename);
  }
}

//________________________________________________________________________________###########################__________________________________________________________


// --------------------- RTC Functions ---------------------
bool mcp7940n_write_register(uint8_t reg, uint8_t value) {
  rtcWire.beginTransmission(MCP7940N_ADDRESS);
  rtcWire.write(reg);
  rtcWire.write(value);
  return rtcWire.endTransmission() == 0;
}

bool mcp7940n_read_register(uint8_t reg, uint8_t *value) {
  rtcWire.beginTransmission(MCP7940N_ADDRESS);
  rtcWire.write(reg);
  if (rtcWire.endTransmission(false) != 0) return false;
  if (rtcWire.requestFrom(MCP7940N_ADDRESS, 1) != 1) return false;
  *value = rtcWire.read();
  return true;
}

bool mcp7940n_read_time() {
  uint8_t value;
  if (!mcp7940n_read_register(0x00, &value)) return false;
  rtcSeconds = (value & 0x0F) + ((value & 0x70) >> 4) * 10;

  if (!mcp7940n_read_register(0x01, &value)) return false;
  rtcMinutes = (value & 0x0F) + ((value & 0x70) >> 4) * 10;

  if (!mcp7940n_read_register(0x02, &value)) return false;
  rtcHours = (value & 0x0F) + ((value & 0x30) >> 4) * 10;

  if (!mcp7940n_read_register(0x03, &value)) return false;
  rtcDay = value & 0x07;

  if (!mcp7940n_read_register(0x04, &value)) return false;
  rtcDate = (value & 0x0F) + ((value & 0x30) >> 4) * 10;

  if (!mcp7940n_read_register(0x05, &value)) return false;
  rtcMonth = (value & 0x0F) + ((value & 0x10) >> 4) * 10;

  if (!mcp7940n_read_register(0x06, &value)) return false;
  rtcYear = (value & 0x0F) + ((value & 0xF0) >> 4) * 10;

  return true;
}

bool mcp7940n_set_time(uint8_t y, uint8_t m, uint8_t d, uint8_t h, uint8_t min, uint8_t sec) {
  uint8_t sec_bcd = ((sec / 10) << 4) | (sec % 10);
  uint8_t min_bcd = ((min / 10) << 4) | (min % 10);
  uint8_t hour_bcd = ((h / 10) << 4) | (h % 10);
  uint8_t date_bcd = ((d / 10) << 4) | (d % 10);
  uint8_t month_bcd = ((m / 10) << 4) | (m % 10);
  uint8_t year_bcd = ((y / 10) << 4) | (y % 10);
  uint8_t wkday = 1; // Monday (placeholder - would be calculated in full implementation)

  sec_bcd |= 0x80; // Start oscillator

  return
    mcp7940n_write_register(0x00, sec_bcd) &&
    mcp7940n_write_register(0x01, min_bcd) &&
    mcp7940n_write_register(0x02, hour_bcd) &&
    mcp7940n_write_register(0x03, wkday | 0x08) &&
    mcp7940n_write_register(0x04, date_bcd) &&
    mcp7940n_write_register(0x05, month_bcd) &&
    mcp7940n_write_register(0x06, year_bcd);
}

// Initialize the RTC module
bool mcp7940n_init() {
  rtcWire.begin();
  uint8_t dummy;
  return mcp7940n_read_register(0x03, &dummy);
}

// --------------------- Helper Functions ---------------------
// Calculate timestamp from RTC values
uint32_t calculateRtcTimestamp() {
  // Simple timestamp calculation using RTC values
  return ((2000+rtcYear-1970)*365 + rtcMonth*30 + rtcDate) * 86400 + 
         rtcHours*3600 + rtcMinutes*60 + rtcSeconds;
}

void gps_read()
{
  byteGPS=Serial3.read();  

  if (byteGPS == -1) 
  {           
    // See if the port is empty yet
    delay(10); 
  } 
  else 
  {
     linea[conta]=byteGPS;        // If there is serial port data, it is put in the buffer
     Serial2.print(byteGPS); 
     conta++;                      
     if (byteGPS==13)
     {

       cont=0;
       bien1=0;
       bien2=0;
       bien3=0;
       for (int i=1;i<7;i++)
       {
         // Verifies if the received command starts with $GPR
         if (linea[i]==comandoGNRMC[i-1])
         {
           bien1++;
           
         }
         if(gnrmc_fix_flag!=9)
         {
          if (linea[i]==comandoGNGGA[i-1])
          {
            bien2++;
            
          }
          if (linea[i]==comandoGNGLL[i-1])
          {
            bien3++;
            
          }
         }
       }
       if((bien1==6)||(bien2==6)||(bien3==6))
       {               // If yes, continue and process the dat
       if(bien1==6)
       {
        exit_gnrmc=1;
        lat_pointer     =2;
        latdir_pointer  =3;
        long_pointer    =4;
        longdir_pointer =5;
        fix_pointer     =1;
       }
       else if(bien2==6)
       {
        exit_gngga=1;
        lat_pointer     =1;
        latdir_pointer  =2;
        long_pointer    =3;
        longdir_pointer =4;
        fix_pointer     =5;
       }
       else if(bien3==6)
       {
        exit_gngll=1;
        lat_pointer     =0;
        latdir_pointer  =1;
        long_pointer    =2;
        longdir_pointer =3;
        fix_pointer     =5;
       }
       for (int i=0;i<300;i++){
       if (linea[i]==','){    // check for the position of the  "," separator
       indices[cont]=i;
       cont++;
       }
       if (linea[i]=='*'){    // ... and the "*"
       indices[12]=i;
       cont++;
       }
       }
       
      for (int i=0;i<7;i++)
      {
        r=0;
        for (int j=indices[i];j<(indices[i+1]-1);j++)
        {
        gps_flag=0;
        temp_var[r]=linea[j+1];
        r=r+1;
        }
        temp_var[r]='\0';
        

        if(i==lat_pointer)
        {
          sprintf(lati,"%s",temp_var);
          Serial2.print("lat="); 
          Serial2.print(i); Serial2.print(","); 
          Serial2.println(temp_var); 
        }
        else if(i==latdir_pointer)
        {
          sprintf(latdir,"%s",temp_var);
          Serial2.print("latdir="); Serial2.print(i); Serial2.print(","); 
          Serial2.println(temp_var); 
        }
        else if(i==long_pointer)
        {
          sprintf(logi,"%s",temp_var);
          Serial2.print("long="); Serial2.print(i); Serial2.print(","); 
          Serial2.println(temp_var); 
        }
        else if(i==longdir_pointer)
        {
          sprintf(longdir,"%s",temp_var);
          Serial2.print("longdir="); Serial2.print(i); Serial2.print(","); 
          Serial2.println(temp_var); 
        }
        else if(i==fix_pointer)
        {
          sprintf(fix,"%s",temp_var);
          Serial2.print("fix="); Serial2.print(i); Serial2.print(","); 
          Serial2.println(temp_var); 
          if(bien1==6)
          {
            if(strcmp(fix,"A")==0)
            {
              gnrmc_fix_flag=9;
            }
            else
            {
              gnrmc_fix_flag=0;
            }
          }
          else if(bien2==6)
          {
            if(strcmp(fix,"1")==0)
            {
              gngga_fix_flag=9;
            }
          }
          else if(bien3==6)
          {
            if(strcmp(fix,"A")==0)
            {
              gngll_fix_flag=9;
            }
          }
        }

     
   
      
      }
      }
      conta=0;  
      for (int i=0;i<300;i++)
      {
      linea[i]=' ';             
      }  
      if((exit_gngga==1)&&(exit_gngll==1)&&(exit_gnrmc==1))  
      {
        gps_exit_flag=0;
      }
    }
  }
}


//________________________________________________________________________________###########################__________________________________________________________

// --------------------- Setup ---------------------
void setup() 
{
  
  Serial.begin(115200);
  delay(100); 
 
  Serial2.begin(115200);
 
  Serial3.begin(115200);
  delay(100);

  Serial2.println("\nEnhanced GPS Logger with RTC and Circular Buffer");
  Serial2.println("Ready.");

 
  Serial2.println("Initializing RTC (primary time source)...");
  rtcWire.begin();
  delay(100);  
  
  if (mcp7940n_init()) {
    Serial2.println("RTC initialized successfully");
    rtcInitialized = true;
    
    // Read current time from RTC
    if (mcp7940n_read_time()) {
      sprintf(rtcDateTimeStr, "20%02d/%02d/%02d %02d:%02d:%02d",
              rtcYear, rtcMonth, rtcDate, rtcHours, rtcMinutes, rtcSeconds);
      Serial2.print("RTC time: ");
      Serial2.println(rtcDateTimeStr);
      
      // Format RTC time for logging
      sprintf(rtcLogDate, "%02d%02d%02d", rtcDate, rtcMonth, rtcYear);
      sprintf(rtcLogTime, "%02d%02d%02d", rtcHours, rtcMinutes, rtcSeconds);
    } else {
      Serial2.println("Failed to read time from RTC");
      // Set default RTC time if it wasn't set before
      if (mcp7940n_set_time(25, 5, 14, 12, 12, 0)) {
        Serial2.println("Set default RTC time: 2025-05-14 12:00:00");
      }
      
      // Read the time we just set
      if (mcp7940n_read_time()) {
        // Format RTC time for logging
        sprintf(rtcLogDate, "%02d%02d%02d", rtcDate, rtcMonth, rtcYear);
        sprintf(rtcLogTime, "%02d%02d%02d", rtcHours, rtcMinutes, rtcSeconds);
      }
    }
  } else {
    Serial2.println("Failed RTC - CRITICAL ERROR!");
  }

  // Configure SPI pins for SD card
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH); // Deselect SD card

  // Initialize SPI2 with conservative settings
  SPI.setModule(2);
  SPI_2.begin();
  SPI_2.setClockDivider(SPI_CLOCK_DIV128); 
  SPI_2.setDataMode(SPI_MODE0);
  SPI_2.setBitOrder(MSBFIRST);
  
  digitalWrite(SD_CS, HIGH);
  delay(100);

  // Send dummy clock cycles with CS high
  for(int i = 0; i < 10; i++) {
    SPI_2.transfer(0xFF);
    Serial2.print(".");
  }
  Serial2.println(" Done");

  delay(100);

  Serial2.println("\nInitializing SD card on SPI2...");
  
  // Select SPI2 for SD card operations
  SPI.setModule(2);
  
 
  bool sdInitialized = false;
  for (int retry = 0; retry < 3; retry++) 
  {
    if (SD.begin(SD_CS)) 
    {
      Serial2.println("SD card initialization successful!");
      
      // Test file operations
      File dataFile = SD.open("test.txt", FILE_WRITE);
      if (dataFile) {
        Serial2.println("\nWriting to test.txt...");
        dataFile.println("Testing SD card with level shifter");
        dataFile.println("Module is working properly!");
        dataFile.close();
        Serial2.println("Write successful!");
        
        // Verify we can read the file back
        dataFile = SD.open("test.txt", FILE_READ);
        if (dataFile) {
          Serial2.println("Reading from test.txt:");
          while (dataFile.available()) {
            Serial2.write(dataFile.read());
          }
          dataFile.close();
          Serial2.println("\nRead test successful!");
          sdInitialized = true;
          break;
        } else {
          Serial2.println("Failed to open test.txt for reading!");
          continue;
        }
      } else {
        Serial2.println("Failed to open test.txt for writing!");
        continue;
      }
    }
    Serial2.print("SD initialization attempt ");
    Serial2.print(retry + 1);
    Serial2.println(" failed. Retrying...");
    delay(500);
  }

  if (!sdInitialized) {
    Serial2.println("Failed to initialize SD card after multiple attempts!");
    while(1); 
  }

  // Initialize circular buffer
  Serial2.println("\nInitializing circular buffer...");
  if (initializeBuffer()) {
    Serial2.println("Circular buffer initialized successfully");
  } else {
    Serial2.println("Failed to initialize circular buffer!");
    while(1); 
  }

 
  Serial2.println("\nWriting test entries to buffer...");
  char databuff[ENTRY_SIZE];
  int numTestEntries = 44; 
  
  for(int i = 0; i < numTestEntries; i++)
  {
    if (mcp7940n_read_time()) {
      // Format RTC time for logging
      sprintf(rtcLogDate, "%02d%02d%02d", rtcDate, rtcMonth, rtcYear);
      sprintf(rtcLogTime, "%02d%02d%02d", rtcHours, rtcMinutes, rtcSeconds);
      
      // Prepare entry data
      sprintf(databuff, "test=%d Date:%s Time:%s", i, rtcLogDate, rtcLogTime);
      
      // Log the entry
      uint32_t timestamp = calculateRtcTimestamp();
      bool success = addBufferEntry(databuff, timestamp);
      
      Serial2.print("Entry ");
      Serial2.print(i);
      Serial2.print(": ");
      Serial2.println(success ? "Success" : "Failed");
      
     
      delay(100);
    }
  }

  // Display buffer stats
  Serial2.println("\nBuffer statistics:");
  Serial2.print("Current position: ");
  Serial2.println(deviceHeader.currentPosition);
  Serial2.print("Overflow flag: ");
  Serial2.println(deviceHeader.overflowFlag);
  Serial2.print("Total entries written: ");
  Serial2.println(deviceHeader.totalEntry);
  
  // Read all entries from the buffer
  Serial2.println("\nReading all entries from buffer:");
  get_data_logs();
  gps_exit_flag=1;
}



void loop()
{
  
  if (mcp7940n_read_time()) {
    // Format RTC time for display
    sprintf(rtcLogDate, "%02d%02d%02d", rtcDate, rtcMonth, rtcYear);
    sprintf(rtcLogTime, "%02d%02d%02d", rtcHours, rtcMinutes, rtcSeconds);

    Serial2.print("\n\nCurrent time: ");
    Serial2.print(rtcLogDate);
    Serial2.print(" ");
    Serial2.println(rtcLogTime);
    delay(1000);
  }
  gps_exit_flag=1;
  exit_gnrmc=exit_gngga=exit_gngll=0;
  while(gps_exit_flag==1)
  {
    if(Serial3.available()>0)
    {
      gps_read();
      //delayMicroseconds(10);
    }
  }
 
  
 // delay(100);
}
