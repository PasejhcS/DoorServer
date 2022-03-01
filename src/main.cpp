#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <ESPAsyncWebServer.h>
#include "htmls.h"
#include <NTPClient.h>
#include <timelib.h>
#include <Wiegand.h>
#include <EEPROM.h>
#include <LittleFS.h>
#include <vector>
#include <string.h>

#define WiegandD0Pin D5
#define WiegandD1Pin D6
#define OpenPin D1
#define WiegandFlushTimeout 100
#define EEPROM_SIZE 512
const char* ssid = "round";//wifi settings
const char* wifi_password = "0939830773";

const char* http_username = "admin";//web page login
const char* http_password = "admin";

const int MAX_PASSWORD_SIZE=20; //maxium password length

struct eepromStruct
{
  char ssid[33];
  char wifi_password[20];
  char http_username[15];
  char http_password[15];
  char ap_ssid[33];
  char ap_password[20];
  char door_password[20];
  char guest_password[20];
  bool guest_enable;
};
eepromStruct es;


String door_password="";//door password for opening door
String guest_password="";
boolean guest_enable=false;

String keypadInput="";//temp for wiegand input.


boolean checkPassword(const String& p);//check if p matchs door_password
void setPassword(const String& p);//write p to EEPROM and set to door_password
void writeLogs(const String& logs1,const String& logs2);//write logs1 and logs2 with current time to littleFS file "/logs"
void wiegandInterput();//for wiegand communication
void receivedData(uint8_t* data, uint8_t bits,const char* m); //for wiegand communication
void receivedDataError(Wiegand::DataError error,uint8_t* data, uint8_t bits,const char* m);//for wiegand communication
void stateChanged(bool plugged,const char* m);//for wiegand communication
boolean openDoor(const String & password);//set open pin to HIGH
boolean checkDigits(const String & s);//check if s only contains numbers.
void setOpen(boolean b);
template <class T> int EEPROM_writeAnything(int address, const T &data);
template <class T> int EEPROM_readAnything(int address, T &data);

Wiegand wiegand;
AsyncWebServer server(80);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

bool isOnline=false;


struct door_admin
{
  int password_size;
  char password_buff[20];
};
struct door_user
{
  int user_no;
  bool enable;
  int password_size;
  char password_buff[20];
  char week;
  u_int32_t hour;
};
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("booting...");



  // Read password from eeprom. 
  //1 byte for size with N byte of password.
  //password is saved as ASCII code
  /*
  uint8_t password_length=EEPROM.read(0);
  Serial.print("current password length:");
  Serial.println(password_length);
  if(password_length!=0)
  {
    door_password.reserve(password_length);
    for(int i=0;i<password_length;i++)
    {
      door_password+=char(EEPROM.read(i+1));
    }
  }

  Serial.println(door_password);
EEPROM.end();
*/
//init eeprom obj
/*
  strcpy(es.ssid,"round");
  strcpy(es.wifi_password,"0939830773");
  strcpy(es.http_username,"admin");
  strcpy(es.http_password,"admin");
  strcpy(es.ap_ssid,"door");
  strcpy(es.ap_password,"0931314123");
  strcpy(es.door_password,"2318132");
  strcpy(es.guest_password,"0");
  es.guest_enable=false;

  EEPROM.begin(EEPROM_SIZE);
  EEPROM_writeAnything(0,es);
  EEPROM.commit();
  EEPROM.end();
*/
  EEPROM.begin(EEPROM_SIZE);
  EEPROM_readAnything(0,es);
  EEPROM.commit();
  EEPROM.end();
  door_password=es.door_password;
  guest_password=es.guest_password;
  guest_enable=es.guest_enable;

  boolean result=WiFi.softAP(es.ap_ssid,es.ap_password);
  Serial.println(result?"AP ON":"AP Error!");

//main page containing a open button
  server.on("/",HTTP_GET,[](AsyncWebServerRequest *request){
    if(!request->authenticate(http_username, http_password))//Needs login to access page
      return request->requestAuthentication();
    request->send_P(200,"text/html",index_html);
  });

  //open door link for web access.
  // used in main page. 
  //this link can open door without doorpassword.
  //***can't prevent repeat attack.***//
  server.on("/openDoor1234",HTTP_GET,[](AsyncWebServerRequest *request){
  if(!request->authenticate(http_username, http_password))
    return request->requestAuthentication();
  openDoor(door_password);
  request->send(200, "text/plain", "OK");
  writeLogs("web","open");//log
  });
  //links to change door password
  server.on("/userHandling",HTTP_GET,[](AsyncWebServerRequest *request){
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    Serial.println("on userHandling get");
    request->send_P(200,"text/html",user_handling_html,processor_userHandling);
    message_userHandling="";
  });
  server.on("/userHandling",HTTP_POST,[](AsyncWebServerRequest *request){
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    String inputMessage;
    Serial.println("on userHandling post");
    // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
    int params = request->params();
    Serial.println(params);
    String password_old="",password_new1="",password_new2="";//typical requirement.old password and new password repeat once.

    for (int i = 0; i < params; i++)
    {
        AsyncWebParameter *p = request->getParam(i);
        if (p->isPost())
        {
            Serial.printf("%s: %s \n", p->name().c_str(), p->value().c_str());
            if(p->name()=="password_old")
              password_old=p->value();
            else if(p->name()=="password_new1")
              password_new1=p->value();
            else if(p->name()=="password_new2")
              password_new2=p->value();
        }
    }
    if(checkPassword(password_old))
    {
      if(password_new1==password_new2)
      {
        if(password_new1.length()<MAX_PASSWORD_SIZE && password_new1.length() !=0 && checkDigits(password_new1))
        {
          setPassword(password_new1);
          message_userHandling="Password has been changed!";
          writeLogs("web","password change");
        }          
        else{message_userHandling="Password criteria incorrect!";}          
      }else{message_userHandling="New password does not match!";}
    }else{message_userHandling="incorrect password!";}
    /*
    if (request->hasParam("password_old")) {
      message_userHandling+= request->getParam("password_old")->value();
    }else if (request->hasParam("password_new1")) {
      message_userHandling+= request->getParam("password_new1")->value();
    }else if (request->hasParam("password_new2")) {
      message_userHandling+= request->getParam("password_new2")->value();
    }*/
    Serial.println(message_userHandling);
    request->send_P(200,"text/html",user_handling_html,processor_userHandling);
    message_userHandling="";
  });
//To logout web Authentication
  server.on("/logout",HTTP_GET,[](AsyncWebServerRequest *request){
    request->send(401);
  });
  server.on("/logged-out", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", logout_html);
  });

  //Display logs on web.
  server.on("/logs",HTTP_GET,[](AsyncWebServerRequest *request){
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    AsyncResponseStream *r=request->beginResponseStream("text/html");

    r->print("<!DOCTYPE HTML><html>\n\
    <head>\n\
      <title>logs</title>\n\
      <link href=\"/css.css\" rel=\"stylesheet\">\n\
      <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n\
    </head>\n\
    <body>\n\
    <br>\n\
    <a class=\"button\" href=\"/\">Control</a>\n\
    <a class=\"button\" href=\"logs\">Logs</a>\n\
    <a class=\"button\" href=\"userHandling\">Manage</a>\n\
    <a class=\"button\" onclick=\"logoutButton()\">Logout</a>\n\
    <br>\n");

    if(LittleFS.begin()==false)
    {
    r->print("littleFS failed!!<br>\n"); 
    }else{
      String fileName ="/logs";

      File f =LittleFS.open(fileName,"r");
      if(!f){
        r->print("No log files!<br>\n");
        f.close();
      }else{
        int lines=0;
        while(f.available())
        {
          f.readStringUntil('\n');
          lines++;
          
        }
        Serial.print("log size:");
        Serial.println(f.size());
        Serial.print(lines);
        Serial.println(" lines Logs");
        f.close();
        f=LittleFS.open(fileName,"r");
        std::vector<String> v;
        v.reserve(lines+1);
        while(f.available())
        {
          v.push_back(f.readStringUntil('\n'));          
        }
        f.close();
        for(int i=v.size()-1;i>=0;i--)
        {
          r->print(v.at(i));
          r->print("<br>\n");
        }
        
      }
    }
    LittleFS.end();
    r->print("</body></html>");
    request->send(r);
    //request->send(200,"text/html",doorLogs);
  });

  //css for good looking
  server.on("/css.css",HTTP_GET,[](AsyncWebServerRequest *request){
    if(!request->authenticate(http_username, http_password))
      return request->requestAuthentication();
    request->send_P(200,"text/css",css_css);
  });
  //set wiegand callback.
  wiegand.onReceive(receivedData, "Card readed: ");
  wiegand.onReceiveError(receivedDataError, "Card read error: ");
  wiegand.onStateChange(stateChanged, "State changed: ");
  wiegand.begin(Wiegand::LENGTH_ANY, true);

  //set wiegand pin
  pinMode(WiegandD0Pin,INPUT);
  pinMode(WiegandD1Pin,INPUT);
  attachInterrupt(digitalPinToInterrupt(WiegandD0Pin),wiegandInterput,CHANGE);
  attachInterrupt(digitalPinToInterrupt(WiegandD1Pin),wiegandInterput,CHANGE);

  //http server start
  server.begin();

  //start wifi
  //TODO: add offline mode!
  WiFi.begin(es.ssid,es.wifi_password);
  for(int i=0;i<100;i++)
  {
    if(WiFi.status() != WL_CONNECTED)
    {
      delay(100);
      Serial.print(".");
    }else
    {
      Serial.print("\nOnline! IP:");
      Serial.println(WiFi.localIP());
      //isOnline=true;
      break;
    }
  }
  
  if(WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Offline mode!");
  }

  Serial.println("start!");

  //Taipei +8
  timeClient.setTimeOffset(3600*8);
  //set NTP refresh time.
  timeClient.setUpdateInterval(600000);//this is NOT WORKING!!
  //start NTP client
  timeClient.begin();
   
}

//wiegand needs to fulsh to call callback.
unsigned long wiegandFlushTimer=0;

//ntp update interval
unsigned long ntpTimer=0;
const unsigned long ntpTimeout=36000000L;//10hour

//for keypad input timeout
bool isPressed=false;
unsigned long lastPressedTime=0;
unsigned long keypadTimeout=10000;

//for reset door open pin
bool isOpened=false;
unsigned long openTimer=0;
unsigned long openTimeout=500;

void loop() {
//flush wiegand to call callback
  if(millis()-wiegandFlushTimer>WiegandFlushTimeout)
  {
    wiegand.flush();
    wiegandFlushTimer=millis();
  }

//if connect to internet update time from ntp
  if( (millis()-ntpTimer>ntpTimeout || ntpTimer==0) && (WiFi.status() == WL_CONNECTED))
  {
    ntpTimer=millis();    
    Serial.print("time update:");
    if(timeClient.forceUpdate())
    {
      Serial.println("Updated!");
      setTime(timeClient.getEpochTime()); 
      Serial.println(now());
    }else{
      Serial.println("Failed!");
    }
  }

  //close opendoor pin.
  if(isOpened)
  {
    if(millis()-openTimer>openTimeout)
    {
      isOpened=false;
      setOpen(false);
    }
  }


//reset key while timeout
  if(isPressed)
  {
    if(millis()-lastPressedTime>keypadTimeout)
    {
      keypadInput.clear();
      isPressed=false;
    }
    
  }


}


boolean checkPassword(const String& p)
{
  if (p==door_password)
    return true;

  return false;
}
boolean checkGuestPassword(const String& p)
{
  if(p==guest_password)
    return true;
  return false;
}
void setPassword(const String& p)
{
  EEPROM.begin(EEPROM_SIZE);
  strcpy(es.door_password,p.c_str());
  EEPROM_writeAnything(0,es);
  EEPROM.commit();
  EEPROM.end();
}
void setGuestPassword(const String& p,bool enable)
{
  EEPROM.begin(EEPROM_SIZE);
  strcpy(es.guest_password,p.c_str());
  es.guest_enable=enable;
  EEPROM_writeAnything(0,es);
  EEPROM.commit();
  EEPROM.end();
}

void writeLogs(const String &log1,const String &log2)
{
  String logs="";
  char tmp[10];
  String y=String(year());
  sprintf(tmp,"%02d",month());
  String mo=tmp;
  sprintf(tmp,"%02d",day());
  String d=tmp;
  sprintf(tmp,"%02d",hour());
  String h=tmp;
  sprintf(tmp,"%02d",minute());
  String m=tmp;
  sprintf(tmp,"%02d",second());
  String s=tmp;
  logs+=y+"/"+mo+"/"+d+" ";
  logs+=String(h+":"+m+":"+s+" ");
  logs+=log1+" ";
  logs+=log2;
  Serial.println(logs);
  
  if(LittleFS.begin())
  {
    String fileName="/logs";
    File f=LittleFS.open(fileName,"r");   
    if((f.size()+logs.length())>4000)//swap file
    {
      Serial.print(f.size());
      Serial.println("byte;logs file too large. Cleanning...");
      int lines=0;
      while(f.available())
      {        
        f.readStringUntil('\n');
        lines++;
      }
      f.close();
      std::vector<String> v;
      v.reserve(lines+1);
      f=LittleFS.open(fileName,"r");
      while(f.available())
      {
        v.push_back(f.readStringUntil('\n'));
      }
      f.close();
      LittleFS.remove(fileName);
      f=LittleFS.open(fileName,"w");
      for(unsigned int i=v.size()/2;i<v.size();i++)
      {
        f.println(v.at(i));
      }
    }    
    f.close();
    f=LittleFS.open(fileName,"a");
    f.println(logs);
  }else{Serial.println("LittleFS failed!");}
  LittleFS.end();

}

IRAM_ATTR void wiegandInterput()
{
  wiegand.setPin0State(digitalRead(WiegandD0Pin));
  wiegand.setPin1State(digitalRead(WiegandD1Pin));
}

void receivedData(uint8_t* data, uint8_t bits,const char* m)
{


  lastPressedTime=millis();
  
  Serial.print("Wiegand data:");
  Serial.print(bits);
  Serial.println("/");
  Serial.println(data[0]);
  data[0]=data[0] & 0b00001111;
  Serial.println("/");
  Serial.println(data[0]);
  
  if( bits != 4 && bits != 8 ) //not keypad button data
    return;



  if(data[0]==0x0A)//'*',clean current input
  {
    keypadInput.clear();
    isPressed=false;

    return;
  }
  if(data[0]==0x0B)//'#',open door
  {
    keypadInput.clear();
    isPressed=false;

    openDoor(keypadInput);
    return;
  }
  if(data[0]>=0x00 && data[0]<=0x09)
  {
    keypadInput+=char(data[0]);
    lastPressedTime=millis();
    isPressed=true;
  }
}

void receivedDataError(Wiegand::DataError error,uint8_t* data, uint8_t bits,const char* m)
{
  Serial.print("Wiegand error:");
  Serial.println(Wiegand::DataErrorStr(error));

}

void stateChanged(bool plugged,const char* m)
{

}

boolean openDoor(const String & input)
{ 
  if( checkPassword(input) )
  {
    //open door
    isOpened=true;
    openTimer=millis();
    
    setOpen(true);
    writeLogs("Admin","Open");
    return true;
  }
  if(guest_enable && checkGuestPassword(input) )
  {
    //open door
    isOpened=true;
    openTimer=millis();
    
    setOpen(true);
    writeLogs("Guest","Open");
    return true;
  }




  writeLogs("Keypad","Wrong!");

  return false;
}

boolean checkDigits(const String & s)
{
  for(auto &i : s)
  {
    if(i<'0' || i>'9')
      return false;
  }
  return true;
}

void setOpen(boolean b)
{
  if(b==true)
  {
    pinMode(OpenPin,OUTPUT);
    digitalWrite(OpenPin,LOW);
  }else
  {
    pinMode(OpenPin,INPUT);
    digitalWrite(OpenPin,HIGH);
  }

}



template <class T> int EEPROM_writeAnything(int address, const T &data)
{
  const byte *p = (const byte *)(const void *)&data;
  int i, n;
  for(i = 0, n = sizeof(data); i < n; i++)
    EEPROM.write(address++, *p++);
  return i;
}
template <class T> int EEPROM_readAnything(int address, T &data)
{
  byte *p = (byte *)(void *)&data;
  int i, n;
  for(i = 0, n = sizeof(data); i < n; i++)
    *p++ = EEPROM.read(address++);
  return i;
}