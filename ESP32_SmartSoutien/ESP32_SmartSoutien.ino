// ================================================================
//  ESP32_SmartSoutien.ino — Sketch principal
//
//  FLUX :
//  1. Démarrage → WiFi → Firebase Auth
//  2. Boucle : lire badge NFC
//  3. Vérifier badge dans Firestore (badges/{UID})
//  4. Si valide → enregistrer présence → LCD OK + bip vert
//  5. Si inconnu → LCD refusé + bip rouge
//  6. Toutes les 30s → envoyer heartbeat (statut device)
//
//  SCHÉMA DE CÂBLAGE :
//  ┌─────────────┬──────────┬──────────────┐
//  │ Composant   │ ESP32    │ Note         │
//  ├─────────────┼──────────┼──────────────┤
//  │ MFRC522 SDA │ GPIO 5   │ Chip Select  │
//  │ MFRC522 SCK │ GPIO 18  │ SPI Clock    │
//  │ MFRC522 MOSI│ GPIO 23  │ SPI MOSI     │
//  │ MFRC522 MISO│ GPIO 19  │ SPI MISO     │
//  │ MFRC522 RST │ GPIO 27  │ Reset        │
//  │ MFRC522 3.3V│ 3.3V     │ Alimentation │
//  │ MFRC522 GND │ GND      │              │
//  ├─────────────┼──────────┼──────────────┤
//  │ LCD SDA     │ GPIO 21  │ I2C Data     │
//  │ LCD SCL     │ GPIO 22  │ I2C Clock    │
//  │ LCD VCC     │ 5V       │ Alimentation │
//  │ LCD GND     │ GND      │              │
//  ├─────────────┼──────────┼──────────────┤
//  │ Buzzer +    │ GPIO 26  │ Via résistance│
//  │ LED R       │ GPIO 25  │ Via 220Ω     │
//  │ LED G       │ GPIO 33  │ Via 220Ω     │
//  │ LED B       │ GPIO 32  │ Via 220Ω     │
//  └─────────────┴──────────┴──────────────┘
//
//  LIBRARIES REQUISES (Tools → Manage Libraries) :
//  - MFRC522 by GithubCommunity
//  - LiquidCrystal I2C by Frank de Brabander
//  - ArduinoJson by Benoit Blanchon (v6.x)
//  - WiFi (inclus ESP32 board)
//
//  BOARD : ESP32 Dev Module
//  Upload Speed : 115200
// ================================================================

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <time.h>

#include "config.h"
#include "hardware.h"
#include "firebase.h"
#include "wifi_manager.h"
#include "nfc.h"

// ── Objets globaux (définis ici, extern dans les .h) ─────────
MFRC522          nfc(NFC_SS_PIN, NFC_RST_PIN);
LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
Preferences      prefs;

// ── Variables Firebase globales (utilisées dans firebase.h) ──
String       g_firebase_token = "";
unsigned long g_token_expiry  = 0;
int g_firestore_last_code = 0;

// ── État interne ─────────────────────────────────────────────
String        lastUID         = "";
unsigned long lastScanTime    = 0;
unsigned long lastHeartbeat   = 0;
int           totalScans      = 0;
bool          systemReady     = false;
String firestoreTimestampNow();
void processOnlineScan(String uid);
bool replayOfflineScans();

// ================================================================
//  SETUP
// ================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=============================");
  Serial.println("  SmartSoutien ESP32 v2.1");
  Serial.println("  Device: " + String(DEVICE_ID));
  Serial.println("  Room:   " + String(DEVICE_ROOM));
  Serial.println("=============================\n");

  // ── Init broches ────────────────────────────────────────
  pinMode(PIN_BUZZER,    OUTPUT);
  pinMode(PIN_LED_RED,   OUTPUT);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_BLUE,  OUTPUT);
  ledOff();
  digitalWrite(PIN_BUZZER, LOW);

  // ── Init LCD ─────────────────────────────────────────────
  Wire.begin();
  lcd.init();
  lcd.backlight();
  lcdShow("SmartSoutien", "Demarrage v2.1");
  buzzerStartup();
  delay(800);

  // ── Init NFC ─────────────────────────────────────────────
  SPI.begin();
  nfc.PCD_Init();
  delay(100);

  if (!testNFC()) {
    lcdShow("ERREUR NFC", "Verif cablage!");
    ledRed();
    // Continuer quand même pour tester le reste
  } else {
    Serial.println("[NFC] Module OK");
  }

  // ── NVS : charger compteur local ─────────────────────────
  prefs.begin("soutien", false);
  totalScans = prefs.getInt("scans", 0);
  Serial.println("[NVS] Scans stockés: " + String(totalScans));

  // ── Connexion WiFi ───────────────────────────────────────
  if (!wifiConnect()) {
    // Mode hors-ligne : continuer sans Firebase
    lcdShow("Mode hors-ligne", "WiFi requis");
    delay(2000);
    systemReady = false;
  } else {
    if (firestoreTimestampNow() == "") {
      Serial.println("[Clock] En attente de la synchronisation NTP");
      systemReady = false;
    } else {
    // ── Authentification Firebase ───────────────────────
    lcdShow("Firebase Auth", "Connexion...");
    ledBlue();

    bool firebaseReady = firebaseLogin();
    bool deviceReady = firebaseReady && updateDeviceOnline();
    if (!deviceReady) {
      lcdShow("Firebase ERREUR", "Verif config.h");
      ledRed();
      buzzerError();
      delay(2000);
      systemReady = false;
    } else {
      systemReady = true;
      lcdShow("Systeme pret!", DEVICE_ROOM);
      ledGreen();
      buzzerOK();
      delay(1500);
      ledOff();
    }
    }
  }

  // Afficher l'écran d'attente
  showStandby();
}

// ================================================================
//  LOOP
// ================================================================
void loop() {
  String uid = readUID();
  if (uid != "" && isDoubleScan(uid, lastUID, lastScanTime)) {
    Serial.println("[NFC] Double scan ignoré");
    showAlreadyScanned("...");
    showStandby();
    return;
  }

  // Capture scans before attempting a possibly slow WiFi reconnect.
  if (uid != "" && (!systemReady || WiFi.status() != WL_CONNECTED)) {
    handleOfflineScan(uid);
    uid = "";
  }

  // ── Vérifier WiFi ────────────────────────────────────────
  if (!wifiCheck()) {
    systemReady = false;
    if (uid != "") handleOfflineScan(uid);
    lcdShow("WiFi perdu...", "Reconnexion");
    delay(50);
    return;
  }

  if (firestoreTimestampNow() == "") {
    if (uid != "") handleOfflineScan(uid);
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    lcdShow("Synchronisation", "heure reseau...");
    delay(1000);
    return;
  }

  // Recover Firebase as well as WiFi after a startup/network outage.
  if (!systemReady) {
    lcdShow("Firebase Auth", "Reconnexion...");
    if (firebaseLogin() && updateDeviceOnline()) {
      systemReady = true;
      lcdShow("Systeme pret!", DEVICE_ROOM);
      delay(1000);
      showStandby();
    } else {
      if (uid != "") handleOfflineScan(uid);
      delay(3000);
      return;
    }
  }

  // Retry any buffered scans once Firebase is reachable.
  replayOfflineScans();

  // ── Heartbeat (statut device toutes les 30s) ─────────────
  if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    sendHeartbeat();
    lastHeartbeat = millis();
  }

  // ── Lecture badge NFC ────────────────────────────────────
  if (uid == "") {
    // Pas de badge → animation attente
    static unsigned long lastAnim = 0;
    if (millis() - lastAnim > 800) {
      animateScanning();
      lastAnim = millis();
    }
    return;
  }

  // ── Badge détecté ────────────────────────────────────────
  Serial.println("\n[NFC] Badge détecté: " + uid);

  processOnlineScan(uid);
  showStandby();
}

void processOnlineScan(String uid) {
  Serial.println("\n[NFC] Badge détecté: " + uid);
  lcdShow("Verification...", uid.substring(0, 16));
  ledBlue();

  String studentName   = "";
  String studentStatus = "";
  String studentId     = "";
  bool   badgeValid    = checkBadge(uid, studentName, studentStatus, studentId);

  if (!badgeValid) {
    if (studentStatus == "retry") {
      Serial.println("[Badge] Vérification temporairement indisponible; scan mis en attente");
      handleOfflineScan(uid);
      return;
    }
    Serial.println("[Badge] Inconnu: " + uid);
    showWelcome("Inconnu", false);
    logScan(uid, "unknown", false);
  } else if (studentStatus == "inactive") {
    // Étudiant inactif
    Serial.println("[Badge] Inactif: " + studentName);
    lcdShow("Compte inactif", studentName.substring(0, 16));
    ledRed();
    buzzerDeny();
    delay(MSG_DISPLAY_TIME);
    ledOff();
  } else if (studentStatus == "payment_pending") {
    // Paiement en attente — accès autorisé mais avertissement
    Serial.println("[Badge] Paiement en attente: " + studentName);
    showPaymentWarning(studentName);
    if (recordAttendance(uid, studentId, studentName, "present_unpaid")) {
      totalScans++;
      prefs.putInt("scans", totalScans);
    }
  } else {
    // Accès autorisé
    Serial.println("[Badge] Accès autorisé: " + studentName);
    showWelcome(studentName, true);
    if (recordAttendance(uid, studentId, studentName, "present")) {
      totalScans++;
      prefs.putInt("scans", totalScans);
    }
  }

}

// ================================================================
//  FONCTIONS MÉTIER
// ================================================================

// ── Vérifier badge dans Firestore ────────────────────────────
bool checkBadge(String uid, String &studentName, String &studentStatus, String &studentId) {
  studentStatus = "retry";
  // GET badges/{UID} dans Firestore
  String response = firestoreGet("badges", uid);
  if (response == "") {
    if (g_firestore_last_code == 404) studentStatus = "unknown";
    return false;
  }

  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, response);
  if (err) {
    Serial.println("[Badge] JSON erreur: " + String(err.c_str()));
    return false;
  }

  // Vérifier que le document existe et est actif
  if (!doc["fields"].containsKey("studentId")) {
    studentStatus = "unknown";
    return false;
  }

  bool isActive = doc["fields"]["isActive"]["booleanValue"] | false;
  if (!isActive) {
    studentStatus = "inactive";
    return true;
  }

  // Récupérer les infos de l'étudiant
  studentId = doc["fields"]["studentId"]["stringValue"].as<String>();

  String studentResponse = firestoreGet("students", studentId);
  if (studentResponse != "") {
    DynamicJsonDocument sDoc(2048);
    if (deserializeJson(sDoc, studentResponse)) return false;
    if (!sDoc["fields"].containsKey("name")) return false;
    studentName   = sDoc["fields"]["name"]["stringValue"]          | "Etudiant";
    if (!(sDoc["fields"]["isActive"]["booleanValue"] | true)) {
      studentStatus = "inactive";
      return true;
    }
    String payStatus = sDoc["fields"]["paymentStatus"]["stringValue"] | "paid";

    if (payStatus == "pending" || payStatus == "overdue") {
      studentStatus = "payment_pending";
    } else {
      studentStatus = "active";
    }
  } else {
    if (g_firestore_last_code == 404) studentStatus = "unknown";
    return false;
  }
  return true;
}

// ── Enregistrer présence dans Firestore ──────────────────────
bool recordAttendance(String uid, String studentId, String studentName, String status, String scannedAt = "") {
  // POST vers courses/{courseId}/sessions/{sessionId}/attendances
  String collection = "courses/" + String(COURSE_ID) +
                      "/sessions/" + String(SESSION_ID) +
                      "/attendances";

  String timestamp = scannedAt == "" ? firestoreTimestampNow() : scannedAt;
  if (timestamp == "") {
    Serial.println("[Attendance] Horloge non synchronisee, scan non envoye");
    return false;
  }

  String fields =
    fsStr("studentName",    studentName)  + "," +
    fsStr("studentId",      studentId)    + "," +
    fsStr("badgeUID",       uid)          + "," +
    fsStr("status",         status)       + "," +
    fsStr("deviceId",       DEVICE_ID)   + "," +
    fsStr("room",           DEVICE_ROOM)  + "," +
    fsStr("source",         "nfc")        + "," +
    fsTimestamp("scannedAt", timestamp);

  String body = firestoreBuildDoc(fields);
  bool ok = firestorePost(collection, body);

  if (ok) {
    Serial.println("[Attendance] Enregistré: " + studentName + " → " + status);
  }
  return ok;
}

String firestoreTimestampNow() {
  time_t now = time(nullptr);
  if (now < 1700000000) return "";
  struct tm utc;
  gmtime_r(&now, &utc);
  char timestampBuffer[25];
  strftime(timestampBuffer, sizeof(timestampBuffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
  return String(timestampBuffer);
}

// ── Log scan non reconnu ──────────────────────────────────────
void logScan(String uid, String type, bool allowed) {
  String timestamp = firestoreTimestampNow();
  if (timestamp == "") return;
  String collection = "scan_logs";
  String fields =
    fsStr("uid",       uid)            + "," +
    fsStr("type",      type)           + "," +
    fsBool("allowed",  allowed)        + "," +
    fsStr("deviceId",  DEVICE_ID)     + "," +
    fsTimestamp("scannedAt", timestamp);

  firestorePost(collection, firestoreBuildDoc(fields));
}

// ── Mettre à jour statut device dans Firestore ───────────────
// void updateDeviceOnline() {
//   String fields =
//     fsStr("status",     "online")         + "," +
//     fsStr("deviceId",   DEVICE_ID)       + "," +
//     fsStr("room",       DEVICE_ROOM)      + "," +
//     fsStr("subject",    DEVICE_SUBJECT)   + "," +
//     fsStr("ip",         WiFi.localIP().toString()) + "," +
//     fsStr("firmware",   "v2.1.0")         + "," +
//     fsInt("rssi",       WiFi.RSSI())      + "," +
//     fsStr("lastSeen",   String(millis()));

//   firestorePatch("devices", String(DEVICE_ID),
//                  firestoreBuildDoc(fields));
//   Serial.println("[Device] Statut online envoyé");
// }
bool updateDeviceOnline() {
  String fields =
    fsStr("status",     "online")               + "," +
    fsStr("deviceId",   DEVICE_ID)              + "," +
    fsStr("room",       DEVICE_ROOM)            + "," +
    fsStr("subject",    DEVICE_SUBJECT)         + "," +
    fsStr("ip",         WiFi.localIP().toString()) + "," +
    fsStr("firmware",   "v2.1.0")               + "," +
    fsInt("rssi",       WiFi.RSSI())            + "," +
    fsTimestamp("lastSeen", firestoreTimestampNow());

  // Essayer PATCH d'abord, si 403 → ignorer (document peut ne pas exister)
  bool ok = firestorePatch(
    "devices", String(DEVICE_ID), firestoreBuildDoc(fields),
    "status,deviceId,room,subject,ip,firmware,rssi,lastSeen");
  if (!ok) {
    Serial.println("[Device] PATCH échoué, création du document...");
    ok = firestorePost("devices", firestoreBuildDoc(fields), String(DEVICE_ID));
  }
  Serial.println(String("[Device] ") + (ok ? "Statut online envoyé" : "Impossible de mettre le device en ligne"));
  return ok;
}

// ── Heartbeat périodique ─────────────────────────────────────
void sendHeartbeat() {
  String fields =
    fsStr("status",     "online")           + "," +
    fsInt("totalScans", totalScans)          + "," +
    fsInt("rssi",       WiFi.RSSI())        + "," +
    fsTimestamp("lastSeen", firestoreTimestampNow());

  bool ok = firestorePatch("devices", String(DEVICE_ID),
                           firestoreBuildDoc(fields), "status,totalScans,rssi,lastSeen");

  Serial.println("[Heartbeat] " + String(ok ? "OK" : "ERREUR") +
                 " | Scans: " + String(totalScans) +
                 " | RSSI: " + String(WiFi.RSSI()) + "dBm");
}

// ── Mode hors-ligne : stocker UID localement ─────────────────
void handleOfflineScan(String uid) {
  const uint32_t maxPendingScans = 100;
  uint32_t pendingCount = prefs.getUInt("qCount", 0);
  if (pendingCount >= maxPendingScans) {
    lcdShow("Memoire pleine", "Scan non stocke");
    ledRed();
    buzzerError();
    Serial.println("[Offline] File pleine: scan non stocké");
    delay(MSG_DISPLAY_TIME);
    ledOff();
    return;
  }

  uint32_t sequence = prefs.getUInt("qNext", 0);
  String key = "q" + String(sequence);
  DynamicJsonDocument queuedScan(256);
  queuedScan["uid"] = uid;
  queuedScan["scannedAt"] = firestoreTimestampNow();
  String payload;
  serializeJson(queuedScan, payload);

  if (prefs.putString(key.c_str(), payload) == 0) {
    lcdShow("Erreur memoire", "Scan non stocke");
    ledRed();
    buzzerError();
    Serial.println("[Offline] Echec d'ecriture NVS pour le scan");
    delay(MSG_DISPLAY_TIME);
    ledOff();
    return;
  }
  prefs.putUInt("qNext", sequence + 1);
  prefs.putUInt("qCount", pendingCount + 1);

  lcdShow("Mode hors-ligne", "Scan enregistre");
  ledYellow();
  buzz(200);
  delay(1500);
  ledOff();
  Serial.println("[Offline] Scan stocké localement: " + uid + " | En attente: " + String(pendingCount + 1));
  showStandby();
}

bool replayOfflineScans() {
  uint32_t head = prefs.getUInt("qHead", 0);
  uint32_t pendingCount = prefs.getUInt("qCount", 0);

  while (pendingCount > 0) {
    String key = "q" + String(head);
    String payload = prefs.getString(key.c_str(), "");
    DynamicJsonDocument queuedScan(256);
    DeserializationError parseError = deserializeJson(queuedScan, payload);
    if (parseError || !queuedScan["uid"].is<const char*>()) {
      Serial.println("[Offline] Scan en attente illisible; suppression de l'entree");
      prefs.remove(key.c_str());
    } else {
      String uid = queuedScan["uid"].as<String>();
      String scannedAt = queuedScan["scannedAt"] | "";
      String studentName = "";
      String studentStatus = "";
      String studentId = "";
      bool valid = checkBadge(uid, studentName, studentStatus, studentId);

      if (!valid && studentStatus != "unknown") {
        Serial.println("[Offline] Firebase toujours indisponible; conservation de la file");
        return false;
      }
      if (!valid) {
        logScan(uid, "unknown", false);
        Serial.println("[Offline] Badge inconnu rejoue et journalise: " + uid);
      } else if (studentStatus != "inactive") {
        String attendanceStatus = studentStatus == "payment_pending" ? "present_unpaid" : "present";
        if (!recordAttendance(uid, studentId, studentName, attendanceStatus, scannedAt)) {
          Serial.println("[Offline] Presence non envoyee; conservation de la file");
          return false;
        }
        totalScans++;
        prefs.putInt("scans", totalScans);
        Serial.println("[Offline] Presence rejouee: " + studentName);
      } else {
        Serial.println("[Offline] Etudiant inactif; scan rejoue sans presence");
      }
      prefs.remove(key.c_str());
    }

    head++;
    pendingCount--;
    prefs.putUInt("qHead", head);
    prefs.putUInt("qCount", pendingCount);
  }
  return true;
}

// ── Afficher écran d'attente ──────────────────────────────────
void showStandby() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(DEVICE_ROOM);
  lcd.setCursor(0, 1);
  lcd.print("Approchez badge ");
}
