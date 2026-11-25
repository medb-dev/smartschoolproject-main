// ================================================================
//  wifi_manager.h — Gestion connexion WiFi avec reconnexion auto
// ================================================================
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <time.h>
#include "config.h"
#include "hardware.h"

// ── Connexion WiFi initiale ───────────────────────────────────
bool wifiConnect() {
  Serial.print("[WiFi] Connexion à: ");
  Serial.println(WIFI_SSID);

  lcdShow("Connexion WiFi", String(WIFI_SSID).substring(0, 16));
  ledBlue();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  int dots = 0;

  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > WIFI_TIMEOUT) {
      Serial.println("[WiFi] Timeout !");
      lcdShow("WiFi ERREUR", "Verif config.h");
      ledRed();
      buzzerError();
      return false;
    }
    delay(500);
    Serial.print(".");
    // Animer LCD
    lcd.setCursor(dots % 16, 1);
    lcd.print(".");
    dots++;
  }

  Serial.println("\n[WiFi] Connecté !");
  Serial.print("[WiFi] IP: ");
  Serial.println(WiFi.localIP());
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  struct tm timeInfo;
  if (!getLocalTime(&timeInfo, 10000)) {
    Serial.println("[Clock] NTP non disponible; horodatage Firestore indisponible");
  }
  lcdShow("WiFi OK", WiFi.localIP().toString());
  ledGreen();
  delay(1000);
  ledOff();
  return true;
}

// ── Vérifier et reconnecter si nécessaire ────────────────────
bool wifiCheck() {
  static bool reconnecting = false;
  static unsigned long reconnectStarted = 0;

  if (WiFi.status() == WL_CONNECTED) {
    reconnecting = false;
    return true;
  }

  if (!reconnecting) {
    Serial.println("[WiFi] Connexion perdue, reconnexion non bloquante...");
    lcdShow("WiFi perdu...", "Reconnexion");
    ledYellow();
    WiFi.disconnect(false);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    reconnectStarted = millis();
    reconnecting = true;
  } else if (millis() - reconnectStarted >= WIFI_TIMEOUT) {
    Serial.println("[WiFi] Reconnexion en cours; le lecteur reste disponible");
    reconnectStarted = millis();
    WiFi.reconnect();
  }

  return WiFi.status() == WL_CONNECTED;
}

// ── Statut WiFi ──────────────────────────────────────────────
String wifiStatus() {
  switch (WiFi.status()) {
    case WL_CONNECTED:       return "Connecte";
    case WL_DISCONNECTED:    return "Deconnecte";
    case WL_CONNECT_FAILED:  return "Echec";
    case WL_NO_SSID_AVAIL:   return "SSID inconnu";
    default:                 return "Inconnu";
  }
}

int wifiRSSI() {
  return WiFi.RSSI();
}

#endif
