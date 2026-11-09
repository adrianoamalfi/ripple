// WiFi provisioning: joins the saved network, or opens a setup portal.
//
// With no saved network (first boot), or when the saved one cannot be reached
// for ~25 s, the board opens its own open WiFi network "Ripple-xxxxxx". A DNS
// server answers every name with the board's address, so phones and laptops pop
// up the setup page by themselves (captive portal). The page lists the networks
// in range; the chosen network is tried first and only saved once the board
// really connects, so a typo can never lock the board out.
//
// If the portal was opened because the saved network was unreachable (router
// still booting after a power cut), it closes by itself once the board gets
// back online.
#pragma once

#include <stdint.h>

class AsyncWebServer;
class AsyncWebServerRequest;

namespace wifi_setup {

void begin();               // call once, before web::begin()
void loop(uint32_t nowMs);  // call from loop()
void registerRoutes(AsyncWebServer& server);

bool portalActive();        // the setup network is open: "/" shows the setup page
const char* hostName();     // "ripple-xxxxxx" (DHCP name and mDNS: ripple-xxxxxx.local)
const char* networkName();  // WiFi network the board is on / will join, or ""

void openPortal();          // force the setup network open (serial 'p')
void forget();              // delete the saved network and restart into the portal

void sendPortalPage(AsyncWebServerRequest* req);

// Development aid (serial 'W'): tries the build-time default network through
// the same path as the portal, without anyone typing a password.
void provisionWithBuildDefaults();

}  // namespace wifi_setup
