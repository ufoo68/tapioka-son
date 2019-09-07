#include <BLEServer.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <M5StickC.h>
#include <math.h>

// Device Name: Maximum 30 bytes
#define DEVICE_NAME "LINE Things Pedometer"

// User service UUID: Change this to your generated service UUID
#define USER_SERVICE_UUID "f1267ff5-bc68-42dd-b694-c824bce6ab7a"
// User service characteristics
#define WRITE_CHARACTERISTIC_UUID "E9062E71-9E62-4BC6-B0D3-35CDCD9B027B"
#define NOTIFY_CHARACTERISTIC_UUID "62FBD229-6EDD-4D1A-B554-5C4E1BB29169"

// PSDI Service UUID: Fixed value for Developer Trial
#define PSDI_SERVICE_UUID "E625601E-9E55-4597-A598-76018A0D293D"
#define PSDI_CHARACTERISTIC_UUID "26E2B12B-85F0-4F3F-9FDD-91D114270E6E"

BLEServer* thingsServer;
BLESecurity *thingsSecurity;
BLEService* userService;
BLEService* psdiService;
BLECharacteristic* psdiCharacteristic;
BLECharacteristic* writeCharacteristic;
BLECharacteristic* notifyCharacteristic;

bool deviceConnected = false;
bool oldDeviceConnected = false;

class serverCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
   deviceConnected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};

float accX = 0;
float accY = 0;
float accZ = 0;

const int numOfSample = 50;
float sample[numOfSample];
float threshold = 0;
int countSample = 0;
float range = 50.0;

uint8_t countStep = 0;

float getDynamicThreshold(float *s) {
    float maxVal = s[0];
    float minVal = s[0];
    for (int i=1; i<sizeof(s); i++) {
        maxVal = max(maxVal, s[i]);
        minVal = min(minVal, s[i]);
    }
    return (maxVal + minVal) / 2.0;
}

float getFilterdAccelData() {
    static float y[2] = {0};
    M5.MPU6886.getAccelData(&accX,&accY,&accZ);
    y[1] = 0.8 * y[0] + 0.2 * (abs(accX) + abs(accY) + abs(accZ)) * 1000.0;
    y[0] = y[1];
    return y[1];
}

void setup() {

  BLEDevice::init("");
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_NO_MITM);

  // Security Settings
  BLESecurity *thingsSecurity = new BLESecurity();
  thingsSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND);
  thingsSecurity->setCapability(ESP_IO_CAP_NONE);
  thingsSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  setupServices();
  startAdvertising();


  M5.begin();
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(0, 0);
  M5.MPU6886.Init();
  sample[countSample] = getFilterdAccelData();
}

void loop() {
  delay(100);
  countSample++;
  sample[countSample] = getFilterdAccelData();
  if (abs(sample[countSample] - sample[countSample-1]) < range) {
    sample[countSample] = sample[countSample-1];
    countSample--;
  }
  if (sample[countSample] < threshold && sample[countSample-1] > threshold) {
    countStep++;
  }
  if (countSample == numOfSample) {
    threshold = getDynamicThreshold(&sample[0]);
    countSample = 0;
    sample[countSample] = getFilterdAccelData();
  }
  if(digitalRead(M5_BUTTON_RST) == LOW){
    countStep=0;
    M5.Lcd.fillScreen(BLACK);
    while(digitalRead(M5_BUTTON_RST) == LOW);
  }

  M5.Lcd.setCursor(0, 30);
  M5.Lcd.println("Walked " + String(countStep) + " steps");

  if (digitalRead(M5_BUTTON_HOME) == LOW) {
    notifyCharacteristic->setValue(&countStep, 1);
    notifyCharacteristic->notify();
    while(digitalRead(M5_BUTTON_HOME) == LOW);
  }

  // Disconnection
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // Wait for BLE Stack to be ready
    thingsServer->startAdvertising(); // Restart advertising
    oldDeviceConnected = deviceConnected;
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 30);
    M5.Lcd.println("Ready to Connect");
    delay(1000);
    M5.Lcd.fillScreen(BLACK);
  }
  // Connection
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(0, 30);
    M5.Lcd.println("Connected");
    delay(1000);
    M5.Lcd.fillScreen(BLACK);
  }
}

void setupServices(void) {

  // Create BLE Server
  thingsServer = BLEDevice::createServer();
  thingsServer->setCallbacks(new serverCallbacks());

  // Setup User Service
  userService = thingsServer->createService(USER_SERVICE_UUID);
  // Create Characteristics for User Service
  notifyCharacteristic = userService->createCharacteristic(NOTIFY_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  notifyCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  BLE2902* ble9202 = new BLE2902();
  ble9202->setNotifications(true);
  ble9202->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  notifyCharacteristic->addDescriptor(ble9202);

  // Setup PSDI Service
  psdiService = thingsServer->createService(PSDI_SERVICE_UUID);
  psdiCharacteristic = psdiService->createCharacteristic(PSDI_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ);
  psdiCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);

  // Set PSDI (Product Specific Device ID) value
  uint64_t macAddress = ESP.getEfuseMac();
  psdiCharacteristic->setValue((uint8_t*) &macAddress, sizeof(macAddress));

  // Start BLE Services
  userService->start();
  psdiService->start();
}

void startAdvertising(void) {
  // Start Advertising
  BLEAdvertisementData scanResponseData = BLEAdvertisementData();
  scanResponseData.setFlags(0x06); // GENERAL_DISC_MODE 0x02 | BR_EDR_NOT_SUPPORTED 0x04
  scanResponseData.setName(DEVICE_NAME);

  thingsServer->getAdvertising()->addServiceUUID(userService->getUUID());
  thingsServer->getAdvertising()->setScanResponseData(scanResponseData);
  thingsServer->getAdvertising()->start();
}
