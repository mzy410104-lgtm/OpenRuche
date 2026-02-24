// ==========================================
// 🐝 智能蜂箱全传感器整合系统 v1.0 🐝
// ==========================================

// --- 引入所有必需的库 ---
#include <Arduino.h>
#include <Wire.h>
#include <BH1750.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include "HX711.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "DHT.h"

#define RXD2 16
#define TXD2 17
#define DELAY_ENVOI 10000 // 10s pour tester

//Structure de la trame 
struct __attribute__((packed)) PayloadRuche {
  uint16_t weight;      // Octets 0-1 : Poids (x100)
  int16_t  temp_int1;   // Octets 2-3 : Temp Int 1 (x100)
  uint8_t  hum_int;     // Octet 4 : Humid Int
  uint8_t  batt_perc;   // Octet 5 : Batterie (%)
  int16_t  temp_int2;   // Octets 6-7 : Temp Int 2 (x100)
  int16_t  temp_int3;   // Octets 8-9 : Temp Int 3 (x100)
  int16_t  temp_ext;    // Octets 10-11 : Temp Ext (x100)
  uint8_t  hum_ext;     // Octet 12 : Humid Ext
  uint16_t lux;         // Octets 13-14 : Luminosité 
};

PayloadRuche maTrame;

// --- 1. I2C 传感器 (光照 + 姿态) ---
BH1750 lightMeter;
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

// --- 2. HX711 称重 ---
const int LOADCELL_DOUT_PIN = 25;
const int LOADCELL_SCK_PIN = 26;
HX711 scale;
float CALIBRATION_FACTOR =210; // ⚠️ 请务必填入您之前算出的校准系数！

// --- 3. DS18B20 高精度温度 (双探头) ---
const int ONE_WIRE_BUS = 23;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature dsSensors(&oneWire);

// --- 4. DHT22 温湿度 (双探头) ---
#define DHT1_PIN 32
#define DHT2_PIN 33  // ⚠️ 注意：为了避开 HX711，这里改成了 33
#define DHTTYPE DHT22
DHT dht1(DHT1_PIN, DHTTYPE);
DHT dht2(DHT2_PIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2); 
  
  Serial.println("=== SYSTEME RUCHE CONNECTEE : INITIALISATION ===");

  //Configuration
  Serial2.println("AT+MODE=LWOTAA");
  delay(500);

  //remplcer avec l'ID du nouveau projet 
  Serial2.println("AT+ID=DevEUI,70B3D57ED0073415"); 
  delay(500);
  Serial2.println("AT+ID=AppEUI,0000000000000000");
  delay(500);
  Serial2.println("AT+KEY=APPKEY,44B8F00FFE627DC84F204831CF7519C6");
  delay(500);
  Serial2.println("AT+DR=5"); // DR5 = débit rapide (SF7) pour petites trames
  delay(500);
  
  Serial.println("Demande de connexion (JOIN)...");
  Serial2.println("AT+JOIN");

  while (!Serial);
  Serial.println("\nLancement du système de contrôle principal de la ruche...");

  // ---------- A. I2C  ----------
  Wire.begin();
  delay(200);
  
  if (lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE, 0x23, &Wire)) {
    Serial.println("✅ BH1750 est bon");
  } else { Serial.println("BH1750 failed！"); }

  if(accel.begin()) {
    Serial.println("ADXL345 est bon");
    accel.setRange(ADXL345_RANGE_4_G);
  } else { Serial.println("ADXL345 est bon！"); }
  
  // Réduire de force la vitesse du bus à 10 kHz pour protéger les sondes à long câble.
  Wire.setClock(10000); 

  // ---------- B.  HX711  ----------
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  if (scale.is_ready()) {
    scale.set_scale(CALIBRATION_FACTOR);
    scale.tare(); 
    Serial.println("HX711 est bon");
  } else { Serial.println(" HX711 failed！"); }

  // ---------- C.  DS18B20 ----------
  dsSensors.begin();
  int dsCount = dsSensors.getDeviceCount();
  Serial.print("DS18B20 et bon，Nombre de sondes détectées: ");
  Serial.println(dsCount);

  // ---------- D.  DHT22 ----------
  dht1.begin();
  dht2.begin();
  Serial.println(" DHT22 est bon");

  Serial.println("=====================================");
  Serial.println(" Début de la surveillance");
  Serial.println("=====================================\n");
  delay(2000); 
}

unsigned long chrono = 0;

void loop() {

  Serial.println("\n--- Données en temps réel sur les ruches ---");
  // =========================================
  // 1. Lire les capteurs I2C (intensité lumineuse + orientation)
  // =========================================
  lightMeter.configure(BH1750::ONE_TIME_HIGH_RES_MODE);
  delay(200); 
  uint16_t lux = lightMeter.readLightLevel();
  
  sensors_event_t event; 
  accel.getEvent(&event);
  float accel_x = event.acceleration.x;
  float accel_y = event.acceleration.y;

  // =========================================
  // 2. Lire le poids à partir du HX711
  // =========================================
  float weight = 0.0;
  if (scale.is_ready()) {
    weight = scale.get_units(5); // Prenez la moyenne de cinq lectures, en équilibrant vitesse et précision.
  }

  // =========================================
  // 3. Lire la température DS18B20
  // =========================================
  dsSensors.requestTemperatures(); // Envoyer la commande de mesure de température
  float ds_temp1 = dsSensors.getTempCByIndex(0);
  float ds_temp2 = dsSensors.getTempCByIndex(1);

  // =========================================
  // 4. Lire la température et l'humidité du DHT22
  // =========================================
  float dht_h1 = dht1.readHumidity();
  float dht_t1 = dht1.readTemperature();
  float dht_h2 = dht2.readHumidity();
  float dht_t2 = dht2.readTemperature();

    // Envoi périodique
  if (millis() - chrono > DELAY_ENVOI) {
    chrono = millis();
    preparerDonnees(weight,dht_t1,dht_h1,94,ds_temp1,ds_temp2,dht_t2,dht_h2,lux); //ZhaoYi
    envoyerTrameLoRa(); //MOI
  }

  // Ecoute des réponses du module (pour le debug)
  if (Serial2.available()) {
    String rep = Serial2.readString();
    Serial.print("Reponse module: ");
    Serial.println(rep);
  }

  // =========================================
  // =========================================
  Serial.print("weight: "); Serial.print(weight, 1); Serial.println(" g");
  
  Serial.print("luminosite: "); Serial.print(lux, 1); Serial.println(" Lux");
  
  Serial.print("Temperature_interieur (DS18B20-1): "); Serial.print(ds_temp1, 1); Serial.println(" °C");
  Serial.print("Temperature_interieur (DS18B20-2): "); Serial.print(ds_temp2, 1); Serial.println(" °C");
  
  Serial.print("Temperature_interieur(DHT22): "); Serial.print(dht_t1, 1); Serial.print(" °C, Humidite: "); Serial.print(dht_h1, 1); Serial.println(" %");
  Serial.print("Temperature_exterieur(DHT22-2): "); Serial.print(dht_t2, 1); Serial.print(" °C, Humidite: "); Serial.print(dht_h2, 1); Serial.println(" %");

  Serial.print("angle de ADXL345: "); Serial.print(accel_x, 2); Serial.print(", "); Serial.println(accel_y, 2);

  // =========================================
  if (lux > 50.0) {
    Serial.println("Le couvercle de la ruche a été ouvert !(Lumière intense détectée)");
  }
  if (abs(accel_x) > 5.0 || abs(accel_y) > 5.0) {
    Serial.println("La ruche s'est renversée ou subit des vibrations anormales !");
  }

  Serial.println("-------------------------");

  delay(2500); 
}

// ZhaoYi : C'est ici que tu mets tes lectures de capteurs
void preparerDonnees(float weight,float dht_t1,float dht_h1,float batterie,float ds_temp1,float ds_temp2,float dht_t2,float dht_h2,uint16_t lux) {
  // --- Exemple à remplacer par les vraies valeurs des capteurs ---
  float poids_lu = weight/1000;       // Valeur réelle en kg
  float temp_int1_lue = dht_t1;  // Valeur réelle en °C
  float hum_int_lue = dht_h1;     // Valeur réelle en %
  float batterie_pourcent = 98; // Valeur réelle en %
  float temp_int2_lue = ds_temp1;
  float temp_int3_lue = ds_temp2;
  float temp_ext_lue = dht_t2;
  float hum_ext_lue = dht_h2;
  uint16_t lux_lu = lux;       // Valeur lux directe
  // ---------------------------------------------------------------
  
  maTrame.weight    = (uint16_t)(poids_lu * 100);    
  maTrame.temp_int1 = (int16_t)(temp_int1_lue * 100); 
  maTrame.hum_int   = (uint8_t)hum_int_lue;          
  maTrame.batt_perc = (uint8_t)batterie_pourcent;    
  maTrame.temp_int2 = (int16_t)(temp_int2_lue * 100);
  maTrame.temp_int3 = (int16_t)(temp_int3_lue * 100);
  maTrame.temp_ext  = (int16_t)(temp_ext_lue * 100); 
  maTrame.hum_ext   = (uint8_t)hum_ext_lue;          
  maTrame.lux       = (uint16_t)lux_lu; 
}

//Léo, envoie hexa
void envoyerTrameLoRa() {
  Serial.println("\n--- Envoi des données Ruche ---");
  
  uint8_t *bytePtr = (uint8_t *)&maTrame; // On pointe sur les octets de la struct
  
  Serial2.print("AT+MSGHEX="); 
  
  for (int i = 0; i < sizeof(maTrame); i++) {
    if (bytePtr[i] < 0x10) Serial2.print("0"); // Padding
    Serial2.print(bytePtr[i], HEX);
  }
  
  Serial2.println(""); // Validation de l'envoi
  Serial.print(maTrame.weight,1);Serial.print('\n');
  Serial.print(maTrame.temp_int1,1);Serial.print('\n');
  Serial.print(maTrame.hum_int,1);Serial.print('\n');
  Serial.print(maTrame.batt_perc,1);Serial.print('\n');
  Serial.print(maTrame.temp_int2,1);Serial.print('\n');
  Serial.print(maTrame.temp_int3,1);Serial.print('\n');
  Serial.print(maTrame.temp_ext,1);Serial.print('\n');
  Serial.print(maTrame.hum_ext,1);Serial.print('\n');
  Serial.print(maTrame.lux,1);Serial.print('\n');
  Serial.println("Trame envoyée au module.");
}
