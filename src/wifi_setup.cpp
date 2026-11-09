#include "wifi_setup.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncWebServer.h>
#include <string.h>

#include "portal_page.h"
#include "ripple_wifi.h"
#include "storage.h"

// Optional development defaults (include/secrets.h). They are only used when
// no network has been saved, so the portal is the normal path.
#if __has_include("secrets.h")
#include "secrets.h"
#endif

namespace wifi_setup {

static void openPortalInternal(bool recoverable);  // defined below

namespace {

enum class Mode : uint8_t { Connecting, Online, Portal };
enum class Attempt : uint8_t { Idle, Trying, Failed, Succeeded };

constexpr uint32_t kConnectTimeoutMs = 25000;  // saved network unreachable -> open the portal
constexpr uint32_t kAttemptTimeoutMs = 20000;  // a network chosen in the portal
constexpr uint32_t kSettleMs = 1000;           // ignore "connected" right after begin()
constexpr uint32_t kNoNetworkGraceMs = 8000;   // "network not found" may flicker at first
constexpr uint32_t kLingerMs = 20000;          // keep the portal open after success, so the
                                               // phone can read the new address
constexpr uint32_t kRecoverStableMs = 5000;
constexpr uint8_t kMaxNetworks = 20;

Mode mode = Mode::Connecting;
Attempt attempt = Attempt::Idle;

storage::Wifi creds;        // what the board joins
bool haveCreds = false;
bool credsSaved = false;    // false for build-time defaults
storage::Wifi pending;      // network being tried from the portal

char error[72] = "";
char apName[24] = "";
char host[24] = "";
char shownSsid[34] = "";

uint32_t modeSinceMs = 0;
uint32_t attemptStartMs = 0;
uint32_t beginDueMs = 0;
bool beginPending = false;  // attempt accepted, radio not touched yet
uint32_t successMs = 0;
uint32_t staUpSinceMs = 0;
uint32_t restartAtMs = 0;
wl_status_t lastStatus = WL_IDLE_STATUS;  // last status seen during an attempt (for the log)
bool wasConnected = false;
bool mdnsStarted = false;
bool portalRecoverable = false;  // opened because the saved network was down

DNSServer dns;

void beginAttempt(uint32_t now);  // defined below

// POST /api/wifi body.
char body[256];
size_t bodyLen = 0;
bool bodyTooLarge = false;

bool connected() {
  return WiFi.status() == WL_CONNECTED;
}

void startMdns() {
  if (mdnsStarted) return;
  if (MDNS.begin(host)) {
    MDNS.addService("http", "tcp", 80);
    mdnsStarted = true;
  }
}

// The mDNS responder binds to the network interfaces that exist when it starts.
// Taking the setup network down (or bringing it up) under it leaves it with a
// dead interface and crashes the board a moment later, so it is always stopped
// before the interfaces change and started again afterwards.
void stopMdns() {
  if (!mdnsStarted) return;
  MDNS.end();
  mdnsStarted = false;
}

void closePortal() {
  stopMdns();
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  if (connected()) startMdns();
  mode = connected() ? Mode::Online : Mode::Connecting;
  modeSinceMs = millis();
  attempt = Attempt::Idle;
  Serial.println("WiFi: setup network closed.");
}

void failAttempt(const char* message) {
  strlcpy(error, message, sizeof(error));
  attempt = Attempt::Failed;
  // Go back to the network we were on, if any.
  if (haveCreds) WiFi.begin(creds.ssid, creds.pass);
  else WiFi.disconnect();
  Serial.printf("WiFi: could not join '%s': %s (status %d)\n", pending.ssid, message,
                static_cast<int>(lastStatus));
}

void succeedAttempt(uint32_t now) {
  creds = pending;
  haveCreds = true;
  credsSaved = storage::saveWifi(creds);
  attempt = Attempt::Succeeded;
  successMs = now;
  error[0] = 0;
  Serial.printf("WiFi: joined '%s', IP %s%s\n", creds.ssid, WiFi.localIP().toString().c_str(),
                credsSaved ? " (saved)" : " (could NOT be saved)");
}

void serviceAttempt(uint32_t now) {
  if (attempt == Attempt::Trying && beginPending) {
    if (static_cast<int32_t>(now - beginDueMs) >= 0) beginAttempt(now);
    return;
  }
  if (attempt == Attempt::Trying) {
    const wl_status_t s = WiFi.status();
    lastStatus = s;
    const uint32_t elapsed = now - attemptStartMs;
    // Only trust "connected" for the network we asked for, and only once the old
    // connection has had time to go away.
    if (s == WL_CONNECTED && elapsed > kSettleMs && WiFi.SSID() == pending.ssid) succeedAttempt(now);
    else if (s == WL_WRONG_PASSWORD) failAttempt("Wrong password for that network.");
    else if (s == WL_NO_SSID_AVAIL && elapsed > kNoNetworkGraceMs)
      failAttempt("Network not found. Is it in range?");
    else if (elapsed > kAttemptTimeoutMs) failAttempt("Could not connect. Check the password and that the network is in range.");
  } else if (attempt == Attempt::Succeeded && now - successMs > kLingerMs) {
    closePortal();
  }
}

const char* stateName() {
  switch (attempt) {
    case Attempt::Trying: return "connecting";
    case Attempt::Succeeded: return "connected";
    case Attempt::Failed: return "failed";
    default: return "idle";
  }
}

void sendJsonError(AsyncWebServerRequest* req, int code, const char* message) {
  char esc[96], buf[140];
  ripple::wifi::escapeJson(message, esc, sizeof(esc));
  snprintf(buf, sizeof(buf), "{\"error\":\"%s\"}", esc);
  req->send(code, "application/json", buf);
}

void sendStatus(AsyncWebServerRequest* req) {
  char ssid[72], err[150], buf[400];
  ripple::wifi::escapeJson(networkName(), ssid, sizeof(ssid));
  ripple::wifi::escapeJson(error, err, sizeof(err));
  snprintf(buf, sizeof(buf),
           "{\"state\":\"%s\",\"error\":\"%s\",\"ssid\":\"%s\",\"ip\":\"%s\",\"host\":\"%s\","
           "\"ap\":\"%s\",\"portal\":%d,\"saved\":%d}",
           stateName(), err, ssid, connected() ? WiFi.localIP().toString().c_str() : "", host,
           apName, mode == Mode::Portal ? 1 : 0, credsSaved ? 1 : 0);
  req->send(200, "application/json", buf);
}

void sendScan(AsyncWebServerRequest* req) {
  const int n = WiFi.scanComplete();
  if (n < 0) {
    if (n == WIFI_SCAN_FAILED) WiFi.scanNetworks(true, true);
    req->send(200, "application/json", "{\"scanning\":1,\"networks\":[]}");
    return;
  }
  // One entry per network name (keeping its strongest signal), strongest first.
  uint8_t idx[kMaxNetworks];
  uint8_t count = 0;
  for (int i = 0; i < n; i++) {
    const String name = WiFi.SSID(i);
    if (name.length() == 0) continue;  // hidden: the user types it
    int same = -1;
    for (uint8_t k = 0; k < count; k++) {
      if (WiFi.SSID(idx[k]) == name) same = k;
    }
    if (same >= 0) {
      if (WiFi.RSSI(i) > WiFi.RSSI(idx[same])) idx[same] = static_cast<uint8_t>(i);
    } else if (count < kMaxNetworks) {
      idx[count++] = static_cast<uint8_t>(i);
    } else {
      uint8_t weakest = 0;  // full: replace the weakest one if this is stronger
      for (uint8_t k = 1; k < count; k++) {
        if (WiFi.RSSI(idx[k]) < WiFi.RSSI(idx[weakest])) weakest = k;
      }
      if (WiFi.RSSI(i) > WiFi.RSSI(idx[weakest])) idx[weakest] = static_cast<uint8_t>(i);
    }
  }
  for (uint8_t a = 1; a < count; a++) {  // insertion sort, strongest first
    const uint8_t v = idx[a];
    int b = a - 1;
    while (b >= 0 && WiFi.RSSI(idx[b]) < WiFi.RSSI(v)) {
      idx[b + 1] = idx[b];
      b--;
    }
    idx[b + 1] = v;
  }

  AsyncResponseStream* res = req->beginResponseStream("application/json");
  res->print("{\"scanning\":0,\"networks\":[");
  for (uint8_t k = 0; k < count; k++) {
    char esc[72];
    ripple::wifi::escapeJson(WiFi.SSID(idx[k]).c_str(), esc, sizeof(esc));
    res->printf("%s{\"ssid\":\"%s\",\"rssi\":%d,\"secure\":%d}", k ? "," : "", esc,
                static_cast<int>(WiFi.RSSI(idx[k])),
                WiFi.encryptionType(idx[k]) != ENC_TYPE_NONE ? 1 : 0);
  }
  res->print("]}");
  req->send(res);
}

bool startAttempt(const char* ssid, const char* pass, char* err, size_t errCap) {
  const char* why = nullptr;
  if (!ripple::wifi::validSsid(ssid, &why) || !ripple::wifi::validPassword(pass, &why)) {
    strlcpy(err, why, errCap);
    return false;
  }
  strlcpy(pending.ssid, ssid, sizeof(pending.ssid));
  strlcpy(pending.pass, pass, sizeof(pending.pass));
  error[0] = 0;
  attempt = Attempt::Trying;
  // Don't touch the radio yet: this runs inside the HTTP request that asked for
  // it, and dropping the connection the request arrived on before the reply has
  // gone out crashes the web server. The loop starts the attempt a moment later.
  beginPending = true;
  beginDueMs = millis() + 400;
  return true;
}

// Called from loop() once the reply has had time to leave.
void beginAttempt(uint32_t now) {
  beginPending = false;
  attemptStartMs = now;
  WiFi.scanDelete();  // a scan in progress would make the connection attempt fail
  // Drop the current connection first. begin() alone leaves it open, so joining
  // the network we are already on would look like success even with a wrong
  // password, and that password would be saved.
  WiFi.disconnect();
  WiFi.begin(pending.ssid, pending.pass);
  Serial.printf("WiFi: trying '%s'...\n", pending.ssid);
}

}  // namespace

void begin() {
  ripple::wifi::accessPointName(ESP.getChipId(), apName, sizeof(apName));
  ripple::wifi::hostName(ESP.getChipId(), host, sizeof(host));

  WiFi.persistent(false);  // we keep the credentials ourselves, no SDK flash writes
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);  // modem sleep makes RSSI updates bursty
  WiFi.hostname(host);

  if (storage::loadWifi(creds)) {
    haveCreds = credsSaved = true;
  }
#ifdef RIPPLE_WIFI_SSID
  else {
    strlcpy(creds.ssid, RIPPLE_WIFI_SSID, sizeof(creds.ssid));
    strlcpy(creds.pass, RIPPLE_WIFI_PASSWORD, sizeof(creds.pass));
    haveCreds = true;
  }
#endif

  modeSinceMs = millis();
  if (haveCreds) {
    Serial.printf("WiFi: connecting to '%s'%s\n", creds.ssid, credsSaved ? "" : " (build default)");
    mode = Mode::Connecting;
    WiFi.begin(creds.ssid, creds.pass);
  } else {
    Serial.println("WiFi: no network saved.");
    openPortal();
  }
}

void openPortal() {
  openPortalInternal(false);
}

static void openPortalInternal(bool recoverable) {
  if (mode == Mode::Portal) return;
  portalRecoverable = recoverable;
  stopMdns();
  WiFi.mode(WIFI_AP_STA);
  const IPAddress ip(192, 168, 4, 1);
  WiFi.softAPConfig(ip, ip, IPAddress(255, 255, 255, 0));
  WiFi.softAP(apName);
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", ip);  // every name resolves to us: that is what makes it captive
  if (connected()) startMdns();
  mode = Mode::Portal;
  modeSinceMs = millis();
  attempt = Attempt::Idle;
  staUpSinceMs = 0;
  WiFi.scanDelete();
  WiFi.scanNetworks(true, true);
  Serial.printf("WiFi: setup network '%s' is open (no password). Join it and a setup page "
                "opens; or browse to http://192.168.4.1/\n",
                apName);
}

void forget() {
  storage::clearWifi();
  credsSaved = false;
  restartAtMs = millis() + 700;  // let the HTTP reply go out first
  Serial.println("WiFi: saved network removed, restarting.");
}

void loop(uint32_t now) {
  if (restartAtMs && static_cast<int32_t>(now - restartAtMs) >= 0) ESP.restart();

  const bool conn = connected();
  if (conn && !wasConnected) {
    Serial.printf("WiFi: online, IP %s, RSSI %d dBm, http://%s.local/\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI(), host);
    startMdns();
  } else if (!conn && wasConnected) {
    Serial.println("WiFi: connection lost, retrying.");
  }
  wasConnected = conn;
  if (mdnsStarted) MDNS.update();

  switch (mode) {
    case Mode::Connecting:
      if (conn) {
        mode = Mode::Online;
      } else if (now - modeSinceMs > kConnectTimeoutMs) {
        Serial.println("WiFi: saved network not reachable, opening the setup network.");
        openPortalInternal(true);
      }
      break;
    case Mode::Online:
      break;
    case Mode::Portal:
      dns.processNextRequest();
      serviceAttempt(now);
      // Opened because the saved network was down: close again once it is back.
      if (portalRecoverable && haveCreds && attempt == Attempt::Idle && conn) {
        if (!staUpSinceMs) staUpSinceMs = now;
        else if (now - staUpSinceMs > kRecoverStableMs) closePortal();
      } else {
        staUpSinceMs = 0;
      }
      break;
  }
}

bool portalActive() {
  return mode == Mode::Portal;
}

const char* hostName() {
  return host;
}

const char* networkName() {
  if (connected()) {
    strlcpy(shownSsid, WiFi.SSID().c_str(), sizeof(shownSsid));
    return shownSsid;
  }
  return haveCreds ? creds.ssid : "";
}

void provisionWithBuildDefaults() {
#ifdef RIPPLE_WIFI_SSID
  if (mode != Mode::Portal) {
    Serial.println("WiFi: open the portal first (serial 'p').");
    return;
  }
  char err[64];
  if (!startAttempt(RIPPLE_WIFI_SSID, RIPPLE_WIFI_PASSWORD, err, sizeof(err))) {
    Serial.printf("WiFi: %s\n", err);
  }
#else
  Serial.println("WiFi: this build has no default network (include/secrets.h).");
#endif
}

void sendPortalPage(AsyncWebServerRequest* req) {
  AsyncWebServerResponse* res = req->beginResponse(
      200, "text/html", reinterpret_cast<const uint8_t*>(PORTAL_HTML), strlen_P(PORTAL_HTML));
  res->addHeader("Cache-Control", "no-store");  // "/" is the dashboard once the board is online
  req->send(res);
}

void registerRoutes(AsyncWebServer& server) {
  // Registered first on purpose: ESPAsyncWebServer also matches "/api/wifi/..." to a
  // handler registered for "/api/wifi", and the first match wins.
  // Forget the saved network and restart into the portal. Lives here because it
  // is the way out when the board is moved to a new network.
  server.on("/api/wifi/reset", HTTP_POST, [](AsyncWebServerRequest* req) {
    forget();
    req->send(200, "application/json", "{\"state\":\"restarting\"}");
  });

  server.on("/api/wifi/status", HTTP_GET, [](AsyncWebServerRequest* req) { sendStatus(req); });

  // Scanning briefly stops the radio, which would disturb the motion sensing,
  // so it is only available while the setup portal is open.
  server.on("/api/wifi/scan", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (mode != Mode::Portal) {
      sendJsonError(req, 403, "Setup mode is not active");
      return;
    }
    if (req->hasParam("rescan")) {
      WiFi.scanDelete();
      WiFi.scanNetworks(true, true);
    }
    sendScan(req);
  });

  server.on(
      "/api/wifi", HTTP_POST,
      [](AsyncWebServerRequest* req) {  // the whole body has arrived
        const bool tooLarge = bodyTooLarge;
        bodyTooLarge = false;
        if (mode != Mode::Portal) {
          sendJsonError(req, 403, "Setup mode is not active");
          return;
        }
        if (tooLarge) {
          sendJsonError(req, 413, "Request too large");
          return;
        }
        if (attempt == Attempt::Trying) {
          sendJsonError(req, 409, "Already connecting");
          return;
        }
        JsonDocument doc;
        if (deserializeJson(doc, body, bodyLen)) {
          sendJsonError(req, 400, "Invalid JSON");
          return;
        }
        char err[64];
        if (!startAttempt(doc["ssid"] | "", doc["pass"] | "", err, sizeof(err))) {
          sendJsonError(req, 400, err);
          return;
        }
        req->send(202, "application/json", "{\"state\":\"connecting\"}");
      },
      nullptr,
      [](AsyncWebServerRequest*, uint8_t* data, size_t len, size_t index, size_t total) {
        if (index == 0) {
          bodyLen = 0;
          bodyTooLarge = total >= sizeof(body);
        }
        if (bodyTooLarge) return;
        memcpy(body + index, data, len);
        bodyLen = index + len;
      });
}

}  // namespace wifi_setup
