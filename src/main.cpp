#include <Arduino.h>


//habilita o debugging
#define DEBUGGING(...) Serial.println( __VA_ARGS__ )
#define DEBUGGING_L(...) Serial.print( __VA_ARGS__ )

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h> //Permite chamar o seu modulo na sua rede pelo nome ao inves do IP.
#include <ESP8266HTTPUpdateServer.h> //Over The Air - OTA

const char* host      = "aboojur"; //Nome na rede
const char* ssid      = "Sala"; //Nome da rede wifi da sua casa
const char* password  = "minecraft"; //Senha da rede wifi da sua casa

//Habilitando a saída serial com as mensagens de debugging
#ifndef DEBUGGING
#define DEBUGGING(...)
#endif
#ifndef DEBUGGING_L
#define DEBUGGING_L(...)
#endif

#define RELE1 13  //Pino D7 
#define RELE2 12   //Pino D6
#define RELE3 14 //Pino D5
#define RELE4 02       // Pino D4
#define NTC 02       // Pino D4



ESP8266HTTPUpdateServer atualizadorOTA; //(OTA)
ESP8266WebServer servidorWeb(80); //Servidor Web na porta 80


//////////////////////////////////////////////////////////////////////////////////////////////////

//Sensores e acionamento
//Timer

#include "UniversalTimer.h"
UniversalTimer ledTimer(20000, true);

WiFiClientSecure  client;
#define TS_ENABLE_SSL // For HTTPS SSL connection

//DS18B20
#include <OneWire.h>
#include <DallasTemperature.h>
#define dados D8 // Porta sensor DS18B20
OneWire oneWire(dados);  /*Protocolo OneWire*/
DallasTemperature sensors(&oneWire); /*encaminha referências OneWire para o sensor*/
float tempGel;
//AHT10

#include <Adafruit_AHTX0.h>

Adafruit_AHTX0 aht;
float humidade;
float tempSala;

// ThingSpeak Settings

#include "ThingSpeak.h"

unsigned long myChannelNumber = 1578968;
const char * myWriteAPIKey = "WC10T0LJRM3H12NV";
String myStatus = "";

// Fingerprint check, make sure that the certificate has not expired.
const char * fingerprint = NULL; // use SECRET_SHA1_FINGERPRINT for fingerprint check

//NTC10k

int ThermistorPin = A0;
int Vo;
float R1 = 10000;
float logR2, R2, T, Tc, Tf = 0;
float c1 = 1.009249522e-03, c2 = 2.378405444e-04, c3 = 2.019202697e-07;
float t = 0.00;

float tempNTC()
{
  float temp_adc_value;
  float temp_val;
  float temp_sum = 0;
  float returnTemp;
  float returnTempRounded;

  for (int i = 0; i < 100; i++)
  {
    temp_adc_value = analogRead(ThermistorPin);


    // NTC thermistor
    R2 = R1 * (1023.0 / (float)temp_adc_value - 1.0);
    logR2 = log(R2);
    T = (1.0 / (c1 + c2 * logR2 + c3 * logR2 * logR2 * logR2));
    Tc = T - 273.15;

    /*
        //  LM35
        //temp_adc_value  *= resolution;
        //Tc = temp_adc_value * 100;

        temp_adc_value = temp_adc_value * 3.2 * 1000 / 1024;
        Tc = temp_adc_value / 10;
    */

    temp_sum = temp_sum + Tc;

    //Serial.println(temp_sum);
    delay(10);
  }

  //Serial.println(temp_sum / 20);
  returnTemp = (temp_sum / 100) - 1.50;
  returnTempRounded = (((float )((int)(returnTemp * 10))) / 10) - 1;

  //Serial.println(returnTempRounded, 1);

  return (returnTempRounded);
}
void thingS()
{

  // set the fields with the values
  ThingSpeak.setField(1, tempGel);
  ThingSpeak.setField(2, tempSala);
  ThingSpeak.setField(3, humidade);
  //ThingSpeak.setField(4, number4);



  // write to the ThingSpeak channel
  int x = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if (x == 200) {
    Serial.println("Channel update successful.");
  }
  else {
    Serial.println("Problem updating channel. HTTP error code " + String(x));
  }


}

void tempGeladeira () {

  //Serial.println(" Requerimento de temperatura...");
  sensors.requestTemperatures(); /* Envia o comando para leitura da temperatura */
  //Serial.println("Pronto");  /*Printa "Pronto" */
  Serial.print("A temperatura é: "); /* Printa "A temperatura é:" */
  Serial.println(sensors.getTempCByIndex(0)); /* Endereço do sensor */
  tempGel = sensors.getTempCByIndex(0);
}

void lerAHT10 () {
  sensors_event_t humidity, temp;
  aht.getEvent(&humidity, &temp);
  Serial.print("Temperature: "); Serial.print(temp.temperature); Serial.println(" degrees C");
  Serial.print("Humidity: "); Serial.print(humidity.relative_humidity); Serial.println("% rH");
  humidade =  humidity.relative_humidity ;
  tempSala = temp.temperature;

  delay(500);
}




//////////////////////////////////////////////////////////////////////////////////////////////////

void InicializaServicoAtualizacao() {
  atualizadorOTA.setup(&servidorWeb);
  servidorWeb.begin();
  DEBUGGING_L("O servico de atualizacao remota (OTA) Foi iniciado com sucesso! Abra http://");
  DEBUGGING_L(host);
  DEBUGGING(".local/update no seu browser para iniciar a atualizacao\n");
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void InicializaWifi() {
  //WiFi.mode(WIFI_AP_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
  }
   DEBUGGING("Conectado!");
  DEBUGGING(WiFi.localIP());
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void InicializaMDNS() {
  if (!MDNS.begin(host)) {
    DEBUGGING("Erro ao iniciar o servico mDNS!");
    while (1) {
      delay(1000);
    }
  }
  DEBUGGING("O servico mDNS foi iniciado com sucesso!");
  MDNS.addService("http", "tcp", 80);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void InicializaPinos() {
  //Iniciando os pinos como saida e com o valor alto (ligado)
  pinMode(RELE1, OUTPUT);
  pinMode(RELE2, OUTPUT);
  pinMode(RELE3, OUTPUT);
  pinMode(RELE4, OUTPUT);


  delay(1000);

}

//////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
  //Se vc ativou o debugging, devera descomentar esta linha abaixo tambem.
  Serial.begin(115200);
  InicializaPinos();
  InicializaWifi();
  InicializaMDNS();
  InicializaServicoAtualizacao();
  Serial.println("ligando");
  delay(2000);
  if (fingerprint != NULL) {
    client.setFingerprint(fingerprint);
  }
  else {
    client.setInsecure(); // To perform a simple SSL Encryption
  }

  ThingSpeak.begin(client);  // Initialize ThingSpeak

  WiFi.begin(ssid, password);
  sensors.begin(); //inicia sensor DS18B20
  aht.begin();
  ledTimer.start();


}

//////////////////////////////////////////////////////////////////////////////////////////////////

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    InicializaWifi();
    InicializaMDNS();
  }
  else {
      servidorWeb.handleClient(); 
       httpServer.handleClient();
  MDNS.update();
  if (ledTimer.check())
  {

    tempGel = tempNTC();
    Serial.println(tempGel);
    lerAHT10();
    thingS();
  }
      }
}
//////////////////////////////////////////////////////////////////////////////////////////////////



