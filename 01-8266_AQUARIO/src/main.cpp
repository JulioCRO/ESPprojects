#include <ESPbasic.h>
#include <SPI.h>
#include "RTClib.h" //INCLUSÃO DA BIBLIOTECA

int max_azul = 128, fade_azul = 255, max_branca = 128, fade_branca = 255, max_full = 128, fade_full = 255, _currfade = 255;
bool _fadeUp = true, _rtc_found=false, _rtc_lost_pw=false, _check_btn=false;
#define BRANCA 14
#define FULL 12
#define AZUL 13
#define BTN 15


RTC_DS3231 rtc; //OBJETO DO TIPO RTC_DS3231

os_timer_t fadeinout; //timmer for wifimonitor

os_timer_t alarme_t; //timmer for wifimonitor

void changelux(AsyncWebServerRequest * request, bool fade);
void getDateTime(AsyncWebServerRequest * request, bool fade);
void setDateTime(AsyncWebServerRequest * request, bool fade);
void luxfade(void*z);
void alarme(void*z);

int lastalarm=0;

void IRAM_ATTR BTN_FUNC();


void setup() {
  pinMode(BRANCA, OUTPUT);
  pinMode(FULL, OUTPUT);
  pinMode(AZUL, OUTPUT);
  pinMode(BTN, INPUT);
  attachInterrupt(BTN, BTN_FUNC, RISING);

  analogWrite(BRANCA, 255);
  analogWrite(FULL, 255);
  analogWrite(AZUL, 255);

  startBasic();

  
  DynamicJsonDocument json(128);
  if (!parseJSON("luz.json", json)) {
    logger(INFO, "Criando luz padrão.");

    json["luz_branca"] = max_branca;
    json["luz_full"] = max_full;
    json["luz_azul"] = max_azul;

    File jsonFile = LittleFS.open("luz.json", "w");
    serializeJsonPretty(json, jsonFile);
    jsonFile.close();
  }

  max_branca = json["luz_branca"].as<unsigned int>();
  max_full = json["luz_full"].as<unsigned int>();
  max_azul = json["luz_azul"].as<unsigned int>();

  char maxlux[128];

  sprintf(maxlux, "Max luz branca: %d full: %d azul: %d", max_branca, max_full, max_azul);
  logger(OK, maxlux);
  if (! rtc.begin()) { // SE O RTC NÃO FOR INICIALIZADO, FAZ
    logger(ERRO, F("DS3231 não encontrado"));
  } else {
    _rtc_found=true;
    logger(OK, F("DS3231 encontrado!"));
    if (rtc.lostPower()) { //SE RTC FOI LIGADO PELA PRIMEIRA VEZ / FICOU SEM ENERGIA / ESGOTOU A BATERIA, FAZ
      _rtc_lost_pw=true;
      logger(INFO, F("DS3231 sem bateria."));
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); //CAPTURA A DATA E HORA EM QUE O SKETCH É COMPILADO
    }

    DateTime now = rtc.now();
    char dateBuffer[128];

    sprintf(dateBuffer, "Data %02u-%02u-%04u %02u:%02u:%02u", now.day(), now.month(), now.year(), now.hour(), now.minute(), now.second());
    logger(INFO, dateBuffer);
  }
  server.on("/luz", HTTP_ANY, [](AsyncWebServerRequest * request) {
    String luz = request->arg("luz");
    String valor = request->arg("valor");
    DynamicJsonDocument json(128);
    json["message"] = "ERRO - Não ajustou";


    if (luz.indexOf("luz_branca") >= 0) {

      analogWrite(BRANCA, valor.toInt());
      json["message"] = "OK - Ajustado";
    }

    if (luz.indexOf("luz_full") >= 0) {
      analogWrite(FULL, valor.toInt());
      json["message"] = "OK - Ajustado";
    }

    if (luz.indexOf("luz_azul") >= 0) {
      analogWrite(AZUL, valor.toInt());
      json["message"] = "OK - Ajustado";
    }


    json["luz"] = luz;
    json["valor"] = valor;
    Serial.println("Setou luz " + luz + " valor:" + valor);
    endExec(json, request);
  });


  server.on("/luz_save", HTTP_ANY, [](AsyncWebServerRequest * request) {

    DynamicJsonDocument json(128);
    int args = request->args();
    for (int i = 0; i < args; i++) {
      json[request->argName(i).c_str()] = request->arg(i).toInt();
    }

    File jsonFile = LittleFS.open("luz.json", "w");
    if (jsonFile) {
      serializeJsonPretty(json, jsonFile);
      jsonFile.close();
      json["message"] = "OK - Salvo";
      //serializeJsonPretty(json, Serial);
    } else {
      json["message"] = "ERRO - Erro salvando json";
    }
    serializeJsonPretty(json, Serial);
    endExec(json, request);
  });

  server.on("/luzon", HTTP_ANY, [](AsyncWebServerRequest * request) {
    changelux(request, true);
  });

  server.on("/luzoff", HTTP_ANY, [](AsyncWebServerRequest * request) {
    changelux(request, false);
  });

  server.on("/getdatetime", HTTP_ANY, [](AsyncWebServerRequest * request) {
    getDateTime(request, NULL);
  });
  server.on("/setdatetime", HTTP_ANY, [](AsyncWebServerRequest * request) {
    setDateTime(request, NULL);
  });
  os_timer_setfn(&fadeinout, luxfade, NULL);
  os_timer_arm(&fadeinout, 500, false);

    os_timer_setfn(&alarme_t, alarme, NULL);
  os_timer_arm(&alarme_t, 60000, true);

}

void loop() {
  if (_check_btn){
     delay(20);
     if (digitalRead(BTN)) {
      Serial.println("apertou...");
      _fadeUp = (! _fadeUp);
      os_timer_arm(&fadeinout, 250, false);
  }
    _check_btn=false;
  }
}

void changelux(AsyncWebServerRequest * request, bool fade) {
  _fadeUp = fade;
  DynamicJsonDocument json(256);
  json["command"] = "changelux";
  json["message"] = "OK - Ligando luz.";
  if (fade) {
    json["message"] = "OK - Desligando luz.";
  }
  endExec(json, request);
  os_timer_arm(&fadeinout, 250, false);

}

void luxfade(void*z) {
  bool more_one = false;
  char dateBuffer[128];
  sprintf(dateBuffer, "Luz fade %s branca: %d azul: %d full: %d", (_fadeUp) ? "DOWN" : "UP", fade_branca, fade_azul, fade_full );
//  logger(INFO, dateBuffer);

  if (!_fadeUp) {
    if (fade_branca < 255) {
      fade_branca++;
      analogWrite(BRANCA, fade_branca);
      more_one = true;
    }
    if (fade_full < 255) {
      fade_full++;
      analogWrite(FULL, fade_full);
      more_one = true;
    }
    if (fade_azul < 255) {
      fade_azul++;
      analogWrite(AZUL, fade_azul);
      more_one = true;
    }
  } else {
    if (fade_branca > max_branca) {
      fade_branca--;
      analogWrite(BRANCA, fade_branca);
      more_one = true;
    }
    if (fade_full > max_full) {
      fade_full--;
      analogWrite(FULL, fade_full);
      more_one = true;
    }
    if (fade_azul > max_azul) {
      fade_azul--;
      analogWrite(AZUL, fade_azul);
      more_one = true;
    }

  }
  if (more_one) {
    os_timer_arm(&fadeinout, 250, false);
  }

}

void getDateTime(AsyncWebServerRequest * request, bool fade) {
  DynamicJsonDocument json(256);
  json["command"] = "getdatetime";
  json["datetime"] = "0000-00-00 00:00:00";
  json["message"] = "ERRO - RTC não encontrado";
  json["lostpw"] = _rtc_lost_pw;

if(_rtc_found){
    DateTime now = rtc.now();
    char dateBuffer[128];
   sprintf(dateBuffer, "%02u-%02u-%04u %02u:%02u:%02u", now.day(), now.month(), now.year(), now.hour(), now.minute(), now.second());
  json["datetime"] = dateBuffer;
  json["message"] = "OK - Date e hora";
}

  endExec(json, request);

}


void IRAM_ATTR BTN_FUNC() {
  _check_btn = true;
}

void alarme(void*z){
Serial.println("alarme");

DateTime now = rtc.now();

if (now.hour() >= 7 and  now.hour() < 19){
changelux(NULL, true);
Serial.println("alarme LIGAR");
}

if (now.hour() >= 19 or now.hour() < 7){
changelux(NULL, false);
Serial.println("alarme DESLIGAR");
}


//lastalarm

//Serial.println("alarme");
}

void setDateTime(AsyncWebServerRequest * request, bool fade){
  String date = request->arg("date");
  
rtc.adjust(DateTime(date.c_str()));

DateTime now = rtc.now();

DynamicJsonDocument json(256);
  json["command"] = "setdatetime";
  json["datetime_str"] = date;
  char dateBuffer[128];
   sprintf(dateBuffer, "%02u-%02u-%04u %02u:%02u:%02u", now.day(), now.month(), now.year(), now.hour(), now.minute(), now.second());
  json["datetime_rtc"] = dateBuffer;
  json["message"] = "OK - Date e hora";
  endExec(json, request);


}