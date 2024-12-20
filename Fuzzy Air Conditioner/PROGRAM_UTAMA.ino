#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include <Fuzzy.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Panasonic.h>
#include <ESP8266WiFi.h>
#include <espnow.h>
#define THINGER_SERIAL_DEBUG

#include <ThingerESP8266.h>

#define USERNAME "Danielpasaribu"
#define DEVICE_ID "ESP8266"
#define DEVICE_CREDENTIAL "a%NGa?ijXnGqRUUw"

#define SSID "m"
#define SSID_PASSWORD "11111111"

ThingerESP8266 thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL);

#define DHTPIN D5    // Digital pin yang terhubung ke sensor DHT
#define ONE_WIRE_BUS D6   // Pin digital untuk sensor DS18B20
#define SEND_PANASONIC_AC true // Set true jika ingin mengirimkan sinyal IR
#define DHTTYPE DHT21 // Tipe sensor DHT
const uint16_t kIrLed = D7;  // Pin GPIO ESP8266 yang akan digunakan. Disarankan: 4 (D2).
IRPanasonicAc ac(kIrLed);  // Atur pin GPIO yang digunakan untuk mengirim pesan.

Fuzzy *kalkulator = new Fuzzy();

int AC_rendah[] = {16,18,19,20};
int AC_sedang[] = {21,22,23,24,25};
int AC_tinggi[] = {26,27,28,29,30};

int input_rendah[] = {20,20,24,25};
int input_sedang[] = {26,26,29,30};
int input_tinggi[] = {31,31,34,36};

float suhu_dht;
float suhu_ds18b20;
float output_fuzzy;


int jumlah_orang = 0;
int controlPin = D0; // Misalnya, ganti dengan nomor pin yang sesuai
DHT dht(DHTPIN, DHTTYPE);
OneWire oneWire(ONE_WIRE_BUS);
LiquidCrystal_I2C lcd(0x27,16,2);
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

typedef struct struct_message {
    int count;
} struct_message;
// Create a struct_message called myData
struct_message myData;

// Callback function that will be executed when data is received
void OnDataRecv(uint8_t * mac, uint8_t *incomingData, uint8_t len) {
  memcpy(&myData, incomingData, sizeof(myData));
  Serial.print("Bytes received: ");
  Serial.println(len);
  Serial.print("Char: ");
  Serial.println(myData.count);
  jumlah_orang = myData.count;
}

void printState() {
  // Display the settings.
  Serial.println("Panasonic A/C remote is in the following state:");
  Serial.printf("  %s\n", ac.toString().c_str());
  // Display the encoded IR sequence.
  unsigned char* ir_code = ac.getRaw();
  Serial.print("IR Code: 0x");
  for (uint8_t i = 0; i < kPanasonicAcStateLength; i++)
    Serial.printf("%02X", ir_code[i]);
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  ac.begin();
  sensors.begin();
  lcd.init();
  lcd.backlight();
  thing.add_wifi(SSID, SSID_PASSWORD);

  thing["suhu_dht"] >> outputValue(suhu_dht);
  thing["suhu_ds18b20"] >> outputValue(suhu_ds18b20);
  thing["output_fuzzy"] >> outputValue(output_fuzzy);

  
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != 0) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(OnDataRecv);

  
  FuzzyInput *input_suhu = new FuzzyInput(1);
  FuzzySet *suhu_rendah = new FuzzySet(input_rendah[0],input_rendah[1],input_rendah[2],input_rendah[3]);
  input_suhu-> addFuzzySet(suhu_rendah);
  FuzzySet *suhu_sedang = new FuzzySet(input_sedang[0],input_sedang[1],input_sedang[2],input_sedang[3]);
  input_suhu-> addFuzzySet(suhu_sedang);
  FuzzySet *suhu_tinggi = new FuzzySet(input_tinggi[0],input_tinggi[1],input_tinggi[2],input_tinggi[3]);
  input_suhu-> addFuzzySet(suhu_tinggi);
  kalkulator->addFuzzyInput(input_suhu);
  
  FuzzyOutput *output_ac = new FuzzyOutput(1);
  FuzzySet *ac_rendah = new FuzzySet(AC_rendah[0],AC_rendah[1],AC_rendah[2],AC_rendah[3]);
  output_ac -> addFuzzySet(ac_rendah);
  FuzzySet *ac_sedang = new FuzzySet(AC_sedang[0],AC_sedang[1],AC_sedang[2],AC_sedang[3]);
  output_ac -> addFuzzySet(ac_sedang);
  FuzzySet *ac_tinggi = new FuzzySet(AC_tinggi[0],AC_tinggi[1],AC_tinggi[2],AC_tinggi[3]);
  output_ac -> addFuzzySet(ac_tinggi);
  kalkulator->addFuzzyOutput(output_ac);

  FuzzyRuleAntecedent *jika_suhu_rendah = new FuzzyRuleAntecedent();
  jika_suhu_rendah->joinSingle(suhu_rendah);
  FuzzyRuleConsequent *maka_ac_tinggi = new FuzzyRuleConsequent();
  maka_ac_tinggi->addOutput(ac_tinggi);
  FuzzyRule *aturan1 = new FuzzyRule(1,jika_suhu_rendah,maka_ac_tinggi);
  kalkulator->addFuzzyRule(aturan1);

  FuzzyRuleAntecedent *jika_suhu_sedang = new FuzzyRuleAntecedent();
  jika_suhu_sedang->joinSingle(suhu_sedang);
  FuzzyRuleConsequent *maka_ac_sedang = new FuzzyRuleConsequent();
  maka_ac_sedang->addOutput(ac_sedang);
  FuzzyRule *aturan2 = new FuzzyRule(2,jika_suhu_sedang,maka_ac_sedang);
  kalkulator->addFuzzyRule(aturan2);

  FuzzyRuleAntecedent *jika_suhu_tinggi = new FuzzyRuleAntecedent();
  jika_suhu_tinggi->joinSingle(suhu_tinggi);
  FuzzyRuleConsequent *maka_ac_rendah = new FuzzyRuleConsequent();
  maka_ac_rendah->addOutput(ac_rendah);
  FuzzyRule *aturan3 = new FuzzyRule(3,jika_suhu_tinggi,maka_ac_rendah);
  kalkulator->addFuzzyRule(aturan3);
  
  // Set up pin untuk kontrol
  pinMode(controlPin, OUTPUT);

}

float dht21() {
  float t = dht.readTemperature();
  if (isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
  }

  Serial.print(F("  SUHU RUANGAN: "));
  Serial.print(t);
  Serial.print(F("°C "));
  return t;
}

float ds18b20() {
  sensors.requestTemperatures(); // Kirim perintah untuk mendapatkan suhu
  float tempC = sensors.getTempCByIndex(0);

  if(tempC != DEVICE_DISCONNECTED_C) {
    Serial.print("SUHU LUAR RUANGAN: ");
    Serial.println(tempC);
  } else {
    Serial.println("Error: Could not read temperature data");
  }
  return tempC;
}

void send_ir(int val) {
  // Set up pengiriman sinyal IR
  ac.setModel(kPanasonicRkr);
  ac.on();
  ac.setFan(kPanasonicAcFanAuto);
  ac.setMode(kPanasonicAcCool);
  ac.setTemp(val);
  ac.setSwingVertical(kPanasonicAcSwingVAuto);
  ac.setSwingHorizontal(kPanasonicAcSwingHAuto);

  #if SEND_PANASONIC_AC
  ac.send();
  #endif  // SEND_PANASONIC_AC
}

void lcdshow(byte r, byte c, String m) {
  lcd.setCursor(r, c);
  lcd.print(m);
}

void loop() {
  int numberOfPeople = jumlah_orang;

  if (numberOfPeople <= 1) {
     Serial.println("Jumlah orang masih 0 atau 1. Mematikan AC.");
    // Set up pengiriman sinyal IR untuk mematikan AC
    ac.setModel(kPanasonicRkr);
    ac.off(); // Matikan AC

    #if SEND_PANASONIC_AC
    ac.send(); // Kirim perintah untuk mematikan AC
    printState(); // Tampilkan kode HEX yang dikirim
    #endif  // SEND_PANASONIC_AC
    delay(5000);
    
    digitalWrite(controlPin, LOW); // Menonaktifkan semua pin
  } else {
    float temperature = dht.readTemperature();
    float humidity = dht.readHumidity();

    Serial.print("SUHU RUANGAN: ");
    Serial.print(temperature);
    Serial.println(" °C");
    Serial.print("KELEMBABAN: ");
    Serial.print(humidity);
    Serial.println(" %");

    lcdshow(0, 0, String(temperature, 1));
    lcd.print((char) 223);
    lcdshow(5, 0, "C");
    lcdshow(0, 1, String(humidity, 1));
    lcd.print((char) 223);
    lcdshow(5, 1, "%");

    float hasil = temperature + ds18b20();
    hasil = hasil / 2;

    kalkulator->setInput(1, hasil);
    kalkulator->fuzzify();
    float output = kalkulator->defuzzify(1);
    Serial.println(output);
    send_ir(output);
    lcdshow(9, 0, String(output));
    lcd.print((char) 223);
    lcdshow(14, 0, "C");
    lcdshow(9, 1, String(jumlah_orang));

suhu_dht = temperature;
suhu_ds18b20 =ds18b20() ;
output_fuzzy = output;

    printState();
    thing.handle();
    delay(3000);
  }
}
