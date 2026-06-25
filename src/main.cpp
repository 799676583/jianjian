#include <Arduino.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <TFT_eSPI.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <USB.h>
#include <USBHIDKeyboard.h>

#if ARDUINO_USB_MODE != 0
#error Password Vault requires ARDUINO_USB_MODE=0 for USB CDC and HID composite support.
#endif
#include "ChineseFont.h"

#if __has_include(<esp_eap_client.h>)
#include <esp_eap_client.h>
#define HAS_ESP_EAP_CLIENT 1
#elif __has_include(<esp_wpa2.h>)
#include <esp_wpa2.h>
#define HAS_ESP_WPA2 1
#endif

TFT_eSPI tft;
TFT_eSprite dogSprite = TFT_eSprite(&tft);
Preferences prefs;
Preferences vaultPrefs;
WebServer server(80);
DNSServer dnsServer;
HardwareSerial jw01Serial(1);
USBHIDKeyboard keyboard;

constexpr int POWER_ON_PIN = 15;
constexpr int ENCODER_CLK_PIN = 10;
constexpr int ENCODER_DT_PIN = 11;
constexpr int ENCODER_SW_PIN = 12;
constexpr int JW01_RX_PIN = 18;
constexpr int JW01_TX_PIN = 17;
constexpr int ENCODER_TRANSITIONS_PER_STEP = 4;
constexpr uint32_t LONG_PRESS_MS = 750;
constexpr uint32_t DEBOUNCE_MS = 35;
constexpr uint16_t SCREEN_W = 320;
constexpr uint16_t SCREEN_H = 170;
constexpr uint16_t HEADER_H = 28;
constexpr uint16_t FOOTER_H = 20;
constexpr int MAX_SAVED_WIFI = 6;
constexpr int MAX_VAULT_ENTRIES = 8;
constexpr int VAULT_VISIBLE_ROWS = 4;
constexpr uint32_t VAULT_PAIR_WINDOW_MS = 45000;
constexpr uint32_t SAFE_DWELL_MS = 700;
constexpr uint32_t SAFE_DOUBLE_MS = 420;
constexpr int MAIN_MENU_VISIBLE_ROWS = 5;
constexpr int WIFI_VISIBLE_ROWS = 5;
constexpr byte DNS_PORT = 53;
constexpr const char *SETUP_AP_SSID = "Jianjian-Setup";
constexpr const char *SETUP_AP_PASS = "12345678";
constexpr int SETUP_AP_CHANNEL = 1;
constexpr uint8_t WIFI_JOIN_OPEN = 0;
constexpr uint8_t WIFI_JOIN_PERSONAL = 1;
constexpr uint8_t WIFI_JOIN_PORTAL = 2;
constexpr uint8_t WIFI_JOIN_ENTERPRISE = 3;
IPAddress apIP(192, 168, 4, 1);

enum class ScreenMode {
  MainMenu,
  AnswerBook,
  MarketTicker,
  CO2Sensor,
  PasswordVault,
  WiFiSettings,
  About
};

enum class VaultPage {
  Menu,
  List,
  Field,
  PairPrompt,
  SafeMenu,
  SafeDial
};

struct WiFiConfig {
  String ssid;
  String username;
  String password;
  bool enterprise;
  uint8_t joinType;
};

struct VaultEntry {
  String site;
  String username;
  String password;
};

struct MarketIndex {
  const char *name;
  const char *symbol;
  float price;
  float changePct;
  bool valid;
  uint32_t updatedAt;
  String error;
};

struct ScannedNetwork {
  String ssid;
  int rssi;
  wifi_auth_mode_t auth;
};

const char *mainMenu[] = {
  "CO2 Level",
  "Password Vault",
  "Answer Book",
  "Market Ticker",
  "WiFi Settings",
  "About"
};
constexpr int MAIN_MENU_COUNT = sizeof(mainMenu) / sizeof(mainMenu[0]);
const int mainMenuIconKinds[] = {2, 5, 0, 1, 3, 4};

const char *wifiSetupLabel = "Setup Portal";
const char *wifiSavedLabel = "Saved WiFi";
const char *wifiInstructionLabel = "Setting Instruction";
const char *wifiBackLabel = "Back";

const char *answerBookEntries[] = {
  "寻求更多的选择",
  "不",
  "别忽视显而易见的东西",
  "结果可能令人吃惊",
  "有决心就能成功",
  "做一次改变",
  "照别人告诉你的去做",
  "不能保证",
  "答案可能会以另一种形式出现",
  "毫无疑问",
  "这样做会使事情变得有趣",
  "这是肯定的",
  "有可能会伤害到他人",
  "全身心投入将赢得好结果",
  "采取冒险的态度",
  "最好等待",
  "可能会惹上麻烦",
  "你需要采取主动",
  "似乎没问题",
  "当然",
  "不要在意",
  "尽早做好它",
  "你终会发现你想知道的一切",
  "除非你独自一人",
  "是，但不要强求",
  "你需要去适应",
  "明天再来试试",
  "以更放松的态度去面对",
  "你需要考虑其他方法",
  "在习惯中接受一些改变",
  "要知道选择太多和选择太少一样很难",
  "别浪费时间了",
  "是",
  "花更多时间来决定",
  "灵活应对",
  "似乎已成事实",
  "看开一点",
  "问问你的异性同事",
  "柳暗花明又一村",
  "你会后悔的",
  "避免第一个解决办法",
  "随它去吧",
  "过段时间就不那么重要了",
  "拭目以待",
  "相信你最初的想法",
  "请不要抗拒",
  "这会带来好运",
  "那一定很棒",
  "把它记下来",
  "可能发生小意外",
  "学习并享受它",
  "这具有重要意义",
  "防备意外发生",
  "一切将依赖于你的选择",
  "转移注意力",
  "离开",
  "你需要其他人的帮助",
  "这有些特别",
  "不要犹豫",
  "先完成其他事",
  "给自己一点时间",
  "现在你就能",
  "那不值得纠结",
  "那将影响别人对你的看法",
  "照别人说的去做",
  "转移你的注意力",
  "你会失望的",
  "最好关注你的工作",
  "形式尚不明朗",
  "不要抱有成见",
  "你必须现在就行动",
  "那可能很难，但值得",
  "付出就会有回报",
  "对意外要有思想准备",
  "更细心地去倾听，你就会知道",
  "且行且思",
  "你有能力以任何方式改善",
  "这是你不会忘记的事物",
  "履行你的义务",
  "快刀斩乱麻",
  "别在这上面下赌注",
  "也许吧",
  "专注于你的家庭生活",
  "绝对不",
  "等待",
  "别犯傻了",
  "可能吧",
  "表示怀疑",
  "意义非凡",
  "那可能非同寻常",
  "不可能失败",
  "你需要了解更多",
  "情况很快就会有变化",
  "这并不重要",
  "顺其自然",
  "问问你最好的朋友",
  "这时不要再自找麻烦",
  "晚一点处理",
  "尝试一种更可能的解决方案",
  "先做重要的事”",
  "投硬币来做决定吧",
  "这也取决于另一种情况",
  "你最终能如愿",
  "可行",
  "答案就在你家窗外",
  "现在的你比以往任何时候都清楚",
  "只需说声“谢谢”",
  "或许，等你再年长些就明白了",
  "这将轰动一时",
  "放手一搏",
  "事情会朝目标发展",
  "更细心去了解，你就知道该怎么做了",
  "需要做更多的努力",
  "等待一个更好的机会",
  "数到5，再试一次",
  "你不得不妥协",
  "很快就能解决",
  "十分确定",
  "这还不确定",
  "谨慎处理",
  "全力以赴",
  "重新考虑你的做法",
  "问问你的母亲吧",
  "如果你独自一人就不要",
  "无需担忧",
  "保持开放的心态",
  "你会为自己所做的感到高兴的",
  "发挥你的想象力",
  "献出你的一切",
  "顺从你的意愿",
  "先做好自己的事",
  "不要怀疑",
  "是时候做新打算了​",
  "省省力气吧",
  "合作将是关键",
  "此时不宜",
  "把这看做一次机会",
  "莫等待",
  "你可能不得不放弃其他东西",
  "遵守规则",
  "相关问题可能会出现",
  "事情将如你所愿",
  "赌一把",
  "以后再处理",
  "结果是乐观的",
  "期待解决",
  "灵活应对",
  "注意细节",
  "你的行动会使一切变好",
  "答案就在公园里",
  "消除你自身的障碍",
  "这是不明智的",
  "将需要大量的努力",
  "不要勉强自己",
  "是时候做打算了",
  "别再犹豫了",
  "享受这次体验",
  "要付出坚持不懈的努力",
  "那仍旧无法预测",
  "毋庸置疑",
  "多花点时间来做决定",
  "只做这一次",
  "这是不明智的",
  "做些改变",
  "可行",
  "先做好其他事",
  "不要陷入到情绪之中",
  "相信你的直觉",
  "采纳智者的建议",
  "情况不明了",
  "你不得不妥协",
  "列出否定的理由",
  "要有耐心",
  "一笑置之",
  "继续",
  "你必须随机应变",
  "别忘记享受乐趣",
  "那是在浪费金钱",
  "重要的优先",
  "为了做出最好的决定，务必保持冷静",
  "尝试一个更没把握的方法",
  "清除你自身的障碍",
  "那可能已成事实",
  "保守你的秘密",
  "你必须马上行动",
  "不要妄下赌注",
  "那可能已无法改变",
  "一些帮助能确保你成功",
  "你肯定会获得支持",
  "只做一次",
  "遵循智者的建议",
  "如你所愿",
  "当局者迷",
  "无论你怎么做，结果依旧",
  "先主后次",
  "这会让你付出代价​​​",
  "尽早行动",
  "寻求更多选择",
  "你现在比以往任何时候都清楚",
  "极可能发生事故",
  "带着好奇去探索",
  "列出这样做的理由",
  "马上停下来",
  "这不是很确定",
  "不用担心",
  "不要告诉别人",
  "你需要其他人的帮助",
  "那将是一件乐事",
  "不要迫于压力草率行事",
  "不要等待",
  "你能以任何方式改善现状",
  "你会为此感到高兴",
  "放弃之前的想法",
  "你不会忘记这些",
  "谨慎对待",
  "放弃你现在的想法",
  "有理由保持乐观",
  "你会发现自己难以妥协",
  "改变不会很快发生",
  "有些障碍需要克服",
  "耐心点",
  "果断放弃",
  "最好把心思放在工作上",
  "要做就做好，否则就不要去做",
  "深表怀疑",
  "最好的解决方法可能不太明显",
  "已超出你的控制",
  "看看会发生什么",
  "你需要更多信息",
  "开阔视野",
  "看得更清楚些",
  "结果可能会令人震惊",
  "节省你的精力吧",
  "管它呢",
  "无论你做什么，结果依旧",
  "那将引起一些纷争",
  "相关问题可能会浮出水面",
  "遵循其他人的建议",
  "改变将不会很快发生​​​",
  "值得付出努力",
  "负起责任来",
  "不值得一争",
  "问问你的父亲吧",
  "向别人倾诉",
  "看起来还行",
  "绝不",
  "等待一个更好的提议",
  "你可能会遭遇反对",
  "告诉某人那对你意味着什么",
  "一切都取决于你的选择",
  "把这看作一个时机",
  "不要陷入你的情绪",
  "你一定得这么做",
  "享受这个过程",
  "不识庐山真面目，只缘身在此山中",
  "你不会失望的",
  "是时候走了",
  "欣然接受",
  "遵循其他人的意见",
  "说出来吧",
  "慷慨大度一些",
  "你可能遭遇反对",
  "你能否不要抗拒",
  "要障碍需要克服",
  "做出改变",
  "无法保证",
  "算了吧",
  "合作是关键"
};
constexpr int ANSWER_COUNT = sizeof(answerBookEntries) / sizeof(answerBookEntries[0]);

MarketIndex markets[] = {
  {"S&P 500", "%5EGSPC", 0, 0, false, 0, ""},
  {"NASDAQ", "%5EIXIC", 0, 0, false, 0, ""},
  {"Dow Jones", "%5EDJI", 0, 0, false, 0, ""},
  {"Hang Seng", "%5EHSI", 0, 0, false, 0, ""},
  {"Nikkei 225", "%5EN225", 0, 0, false, 0, ""},
  {"Shanghai", "000001.SS", 0, 0, false, 0, ""},
  {"Shenzhen", "399001.SZ", 0, 0, false, 0, ""},
  {"DAX", "%5EGDAXI", 0, 0, false, 0, ""},
  {"FTSE 100", "%5EFTSE", 0, 0, false, 0, ""}
};
constexpr int MARKET_COUNT = sizeof(markets) / sizeof(markets[0]);

ScreenMode mode = ScreenMode::MainMenu;
ScreenMode previousMode = ScreenMode::MainMenu;
WiFiConfig wifiConfig;
WiFiConfig savedWiFis[MAX_SAVED_WIFI];
VaultEntry vaultEntries[MAX_VAULT_ENTRIES];

int selectedIndex = 0;
int mainTopIndex = 0;
int wifiSelectedIndex = 0;
int wifiTopIndex = 0;
int savedWiFiCount = 0;
int vaultCount = 0;
int vaultSelectedIndex = 0;
int vaultTopIndex = 0;
int vaultActiveEntry = -1;
int pendingDeleteVault = -1;
int pendingDeleteWifi = -1;
bool wifiListOpen = false;
int answerIndex = 0;
int renderedSelectedIndex = -1;
int renderedWifiIndex = -1;
int marketScrollY = 0;
int marketFetchIndex = 0;
ScannedNetwork scannedNetworks[24];
int scannedNetworkCount = 0;
bool wifiScanInProgress = false;
bool wifiUiDirty = true;
uint32_t wifiScanStartedAt = 0;
uint32_t wifiScanNextReportAt = 0;
uint32_t lastWiFiUiRefresh = 0;
uint32_t lastApHealthCheck = 0;
uint32_t lastApStartAttempt = 0;
String wifiLogLines[8];
uint8_t wifiLogCount = 0;

uint8_t encoderState = 0;
int8_t encoderAccumulator = 0;
bool lastButton = HIGH;
bool buttonPressing = false;
bool longPressHandled = false;
uint32_t buttonDownAt = 0;
uint32_t lastButtonChange = 0;
uint32_t lastDraw = 0;
uint32_t lastMarqueeFrame = 0;
uint32_t lastMarketFrame = 0;
uint32_t lastMarketFetch = 0;
uint32_t wifiConnectStarted = 0;
uint32_t lastCo2Draw = 0;
uint32_t lastDogFrame = 0;
uint32_t lastCo2FrameAt = 0;
uint32_t lastCo2Poll = 0;
uint32_t lastCo2ByteAt = 0;
uint32_t lastCo2BaudSwitch = 0;
uint32_t co2BytesAtBaudSwitch = 0;
bool redrawRequested = true;
bool toggleImeForTyping = true;
bool safeCodeSet = false;
bool safeSettingMode = false;
uint8_t safeCodeValues[4] = {0, 0, 0, 0};
int8_t safeCodeDirections[4] = {0, 0, 0, 0};
uint8_t safeEnteredValues[4] = {0, 0, 0, 0};
int8_t safeEnteredDirections[4] = {0, 0, 0, 0};
int safeDialValue = 0;
int safeRenderedDialValue = -1;
int safeDialDirection = 0;
int safeEnteredCount = 0;
uint32_t safeLastMoveAt = 0;
uint32_t safeConfirmAt = 0;
bool connectingWiFi = false;
bool configApRunning = false;
bool configPortalStarted = false;
bool configRoutesRegistered = false;
bool wifiRadioInitialized = false;
bool autoWiFiConnect = false;
bool autoWiFiPending = false;
uint32_t autoWiFiReadyAt = 0;
bool wifiInstructionOpen = false;
VaultPage vaultPage = VaultPage::Menu;
ScreenMode vaultReturnMode = ScreenMode::MainMenu;
String vaultPairToken;
uint32_t vaultPairExpiresAt = 0;
int autoWiFiBaseIndex = 0;
int autoWiFiAttempt = 0;
String statusLine = "Ready";
String portalRedirectUrl;
String portalProbeStatus;
bool portalCheckPending = false;
uint32_t portalCheckReadyAt = 0;
int co2Ppm = -1;
uint32_t co2FrameCount = 0;
uint32_t co2ByteCount = 0;
String co2RawFrame = "none";
String co2Status = "warming up";
String serialCommandBuffer;
uint8_t jw01Buffer[48];
uint8_t jw01BufferLen = 0;
const uint32_t jw01Bauds[] = {9600};
uint8_t jw01BaudIndex = 0;

void drawCurrentScreen();
void drawMarketRows();
void drawCo2Values();
void drawCo2DogAvatar();
void setJw01Baud(uint8_t index);
void startConfigAp();
void connectWiFi(bool showSettings = true, bool autoMode = false);
void updateWiFiScan();
void addWiFiLog(const String &message);
String makeWiFiDiagnostics();
void handleSetupNetworks();
void refreshWiFiSettingsIfNeeded();
void drawPasswordVault();
void drawSafeDial(bool entering=false);

void toggleInputMethod()
{
  keyboard.press(KEY_LEFT_CTRL);
  keyboard.press(' ');
  delay(30);
  keyboard.releaseAll();
  delay(140);
}

void typeCredentialText(const String &text)
{
  if (toggleImeForTyping) toggleInputMethod();
  keyboard.print(text);
  keyboard.releaseAll();
  delay(80);
  if (toggleImeForTyping) toggleInputMethod();
}

String masked(const String &value)
{
  if (value.length() == 0) return "(empty)";
  String out;
  for (size_t i = 0; i < value.length(); i++) out += '*';
  return out;
}

int base64Digit(char c)
{
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

bool decodeBase64(const String &encoded, String &decoded)
{
  decoded = "";
  uint32_t buffer = 0;
  int bits = 0;
  for (size_t i = 0; i < encoded.length(); i++) {
    char c = encoded[i];
    if (c == '=') break;
    int value = base64Digit(c);
    if (value < 0) return false;
    buffer = (buffer << 6) | static_cast<uint32_t>(value);
    bits += 6;
    while (bits >= 8) {
      bits -= 8;
      decoded += static_cast<char>((buffer >> bits) & 0xFF);
    }
  }
  return true;
}

uint16_t uiBg() { return tft.color565(24, 24, 34); }
uint16_t uiPanel() { return tft.color565(38, 34, 48); }
uint16_t uiHeader() { return tft.color565(246, 150, 184); }
uint16_t uiHeaderText() { return tft.color565(42, 28, 40); }
uint16_t uiSelect() { return tft.color565(255, 197, 218); }
uint16_t uiText() { return tft.color565(255, 246, 250); }
uint16_t uiMuted() { return tft.color565(176, 166, 188); }
uint16_t uiMint() { return tft.color565(128, 226, 205); }
uint16_t uiLemon() { return tft.color565(255, 224, 128); }
uint16_t uiCoral() { return tft.color565(255, 122, 132); }

uint8_t effectiveWiFiJoinType(const WiFiConfig &config)
{
  if (config.joinType <= WIFI_JOIN_ENTERPRISE) return config.joinType;
  return config.enterprise ? WIFI_JOIN_ENTERPRISE : WIFI_JOIN_PERSONAL;
}

const char *wifiJoinTypeLabel(uint8_t joinType)
{
  switch (joinType) {
    case WIFI_JOIN_OPEN: return "Open";
    case WIFI_JOIN_PERSONAL: return "Personal";
    case WIFI_JOIN_PORTAL: return "Portal";
    case WIFI_JOIN_ENTERPRISE: return "Enterprise";
    default: return "Personal";
  }
}
void saveWifiList()
{
  prefs.begin("ui", false);
  prefs.putInt("wifiCount", savedWiFiCount);
  for (int i = 0; i < MAX_SAVED_WIFI; i++) {
    String suffix = String(i);
    if (i < savedWiFiCount) {
      prefs.putString(("s" + suffix).c_str(), savedWiFis[i].ssid);
      prefs.putString(("u" + suffix).c_str(), savedWiFis[i].username);
      prefs.putString(("p" + suffix).c_str(), savedWiFis[i].password);
      prefs.putBool(("e" + suffix).c_str(), savedWiFis[i].enterprise);
      prefs.putUChar(("t" + suffix).c_str(), effectiveWiFiJoinType(savedWiFis[i]));
    } else {
      prefs.remove(("s" + suffix).c_str());
      prefs.remove(("u" + suffix).c_str());
      prefs.remove(("p" + suffix).c_str());
      prefs.remove(("e" + suffix).c_str());
      prefs.remove(("t" + suffix).c_str());
    }
  }

  prefs.putString("ssid", wifiConfig.ssid);
  prefs.putString("user", wifiConfig.username);
  prefs.putString("pass", wifiConfig.password);
  prefs.putBool("ent", wifiConfig.enterprise);
  prefs.putUChar("type", effectiveWiFiJoinType(wifiConfig));
  prefs.end();
}

void upsertSavedWiFi(const WiFiConfig &config)
{
  if (config.ssid.length() == 0) return;

  int found = -1;
  for (int i = 0; i < savedWiFiCount; i++) {
    if (savedWiFis[i].ssid == config.ssid) {
      found = i;
      break;
    }
  }

  if (found >= 0) {
    savedWiFis[found] = config;
  } else {
    if (savedWiFiCount >= MAX_SAVED_WIFI) {
      for (int i = 1; i < savedWiFiCount; i++) savedWiFis[i - 1] = savedWiFis[i];
      savedWiFiCount = MAX_SAVED_WIFI - 1;
    }
    savedWiFis[savedWiFiCount++] = config;
  }
}

void saveConfig()
{
  upsertSavedWiFi(wifiConfig);
  saveWifiList();
}

int findSavedWiFiIndex(const String &ssid)
{
  for (int i = 0; i < savedWiFiCount; i++) {
    if (savedWiFis[i].ssid == ssid) return i;
  }
  return -1;
}

void loadConfig()
{
  savedWiFiCount = 0;
  prefs.begin("ui", true);
  int count = prefs.getInt("wifiCount", -1);
  if (count >= 0) {
    savedWiFiCount = min(count, MAX_SAVED_WIFI);
    for (int i = 0; i < savedWiFiCount; i++) {
      String suffix = String(i);
      savedWiFis[i].ssid = prefs.getString(("s" + suffix).c_str(), "");
      savedWiFis[i].username = prefs.getString(("u" + suffix).c_str(), "");
      savedWiFis[i].password = prefs.getString(("p" + suffix).c_str(), "");
      savedWiFis[i].enterprise = prefs.getBool(("e" + suffix).c_str(), false);
      savedWiFis[i].joinType = prefs.getUChar(("t" + suffix).c_str(), savedWiFis[i].enterprise ? WIFI_JOIN_ENTERPRISE : WIFI_JOIN_PERSONAL);
    }
  }

  wifiConfig.ssid = prefs.getString("ssid", "");
  wifiConfig.username = prefs.getString("user", "");
  wifiConfig.password = prefs.getString("pass", "");
  wifiConfig.enterprise = prefs.getBool("ent", false);
  wifiConfig.joinType = prefs.getUChar("type", wifiConfig.enterprise ? WIFI_JOIN_ENTERPRISE : WIFI_JOIN_PERSONAL);
  prefs.end();

  if (savedWiFiCount == 0 && wifiConfig.ssid.length() > 0) {
    upsertSavedWiFi(wifiConfig);
    saveWifiList();
  } else if (savedWiFiCount > 0 && wifiConfig.ssid.length() == 0) {
    wifiConfig = savedWiFis[0];
  }
}

void saveVault()
{
  vaultPrefs.begin("vault", false);
  vaultPrefs.putInt("count", vaultCount);
  vaultPrefs.putBool("imeToggle", toggleImeForTyping);
  vaultPrefs.putBool("safeSet", safeCodeSet);
  for (int i = 0; i < 4; i++) {
    vaultPrefs.putUChar(("safeV" + String(i)).c_str(), safeCodeValues[i]);
    vaultPrefs.putChar(("safeD" + String(i)).c_str(), safeCodeDirections[i]);
  }
  for (int i = 0; i < MAX_VAULT_ENTRIES; i++) {
    String suffix = String(i);
    if (i < vaultCount) {
      vaultPrefs.putString(("site" + suffix).c_str(), vaultEntries[i].site);
      vaultPrefs.putString(("user" + suffix).c_str(), vaultEntries[i].username);
      vaultPrefs.putString(("pass" + suffix).c_str(), vaultEntries[i].password);
    } else {
      vaultPrefs.remove(("site" + suffix).c_str());
      vaultPrefs.remove(("user" + suffix).c_str());
      vaultPrefs.remove(("pass" + suffix).c_str());
    }
  }
  vaultPrefs.end();
}

void loadVault()
{
  vaultCount = 0;
  vaultPrefs.begin("vault", true);
  toggleImeForTyping = vaultPrefs.getBool("imeToggle", true);
  safeCodeSet = vaultPrefs.getBool("safeSet", false);
  for (int i = 0; i < 4; i++) {
    safeCodeValues[i] = vaultPrefs.getUChar(("safeV" + String(i)).c_str(), 0);
    safeCodeDirections[i] = vaultPrefs.getChar(("safeD" + String(i)).c_str(), 0);
  }
  int count = vaultPrefs.getInt("count", 0);
  vaultCount = min(max(count, 0), MAX_VAULT_ENTRIES);
  for (int i = 0; i < vaultCount; i++) {
    String suffix = String(i);
    vaultEntries[i].site = vaultPrefs.getString(("site" + suffix).c_str(), "");
    vaultEntries[i].username = vaultPrefs.getString(("user" + suffix).c_str(), "");
    vaultEntries[i].password = vaultPrefs.getString(("pass" + suffix).c_str(), "");
  }
  vaultPrefs.end();
}

void upsertVaultEntry(const String &site, const String &username, const String &password)
{
  int found = -1;
  for (int i = 0; i < vaultCount; i++) {
    if (vaultEntries[i].site == site) {
      found = i;
      break;
    }
  }

  if (found < 0) {
    if (vaultCount >= MAX_VAULT_ENTRIES) {
      for (int i = 1; i < vaultCount; i++) vaultEntries[i - 1] = vaultEntries[i];
      vaultCount = MAX_VAULT_ENTRIES - 1;
    }
    found = vaultCount++;
  }

  vaultEntries[found] = {site, username, password};
  saveVault();
}

void deleteVaultEntry(int index)
{
  if (index < 0 || index >= vaultCount) return;
  for (int i = index + 1; i < vaultCount; i++) vaultEntries[i - 1] = vaultEntries[i];
  vaultCount--;
  pendingDeleteVault = -1;
  if (vaultSelectedIndex >= vaultCount + 1) vaultSelectedIndex = max(0, vaultCount);
  saveVault();
}
void clearConfig()
{
  prefs.begin("ui", false);
  prefs.clear();
  prefs.end();
  savedWiFiCount = 0;
  pendingDeleteWifi = -1;
  wifiListOpen = false;
  wifiInstructionOpen = false;
  wifiSelectedIndex = 0;
  wifiTopIndex = 0;
  wifiConfig = {"", "", "", false, WIFI_JOIN_PERSONAL};
}

void deleteSavedWiFi(int savedIndex)
{
  if (savedIndex < 0 || savedIndex >= savedWiFiCount) return;
  String removed = savedWiFis[savedIndex].ssid;
  for (int i = savedIndex + 1; i < savedWiFiCount; i++) savedWiFis[i - 1] = savedWiFis[i];
  savedWiFiCount--;
  if (wifiConfig.ssid == removed) {
    wifiConfig = savedWiFiCount > 0 ? savedWiFis[0] : WiFiConfig{"", "", "", false, WIFI_JOIN_PERSONAL};
  }
  pendingDeleteWifi = -1;
  int count = wifiListOpen ? savedWiFiCount + 1 : 3;
  if (wifiSelectedIndex >= count) wifiSelectedIndex = max(0, count - 1);
  saveWifiList();
}

void drawHeader(const char *title)
{
  tft.fillRect(0, 0, SCREEN_W, HEADER_H, uiHeader());
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(uiHeaderText(), uiHeader());
  tft.drawString(title, 10, HEADER_H / 2, 2);
  tft.drawFastHLine(0, HEADER_H, SCREEN_W, uiMint());
}

void drawFooter(const String &text)
{
  tft.fillRect(0, SCREEN_H - FOOTER_H, SCREEN_W, FOOTER_H, uiPanel());
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(uiMuted(), uiPanel());
  tft.drawString(text.substring(0, 38), SCREEN_W / 2, SCREEN_H - 10, 2);
}

void drawMarqueeText(const String &text, int x, int y, int width, uint16_t fg, uint16_t bg, bool selected)
{
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(fg, bg);
  int textWidth = tft.textWidth(text, 2);
  int offset = 0;
  if (selected && textWidth > width) offset = ((millis() / 420) * 3) % (textWidth + 22);
  tft.setViewport(x, y - 12, width, 24, true);
  tft.fillRect(0, 0, width, 24, bg);
  tft.drawString(text, -offset, 12, 2);
  if (selected && textWidth > width) tft.drawString(text, -offset + textWidth + 22, 12, 2);
  tft.resetViewport();
}

bool textNeedsMarquee(const String &text, int width)
{
  return tft.textWidth(text, 2) > width;
}

int wifiItemCount()
{
  if (wifiInstructionOpen) return 1;
  return wifiListOpen ? savedWiFiCount + 1 : 4;
}

bool wifiIndexIsSaved(int index)
{
  return wifiListOpen && !wifiInstructionOpen && index >= 0 && index < savedWiFiCount;
}

int wifiSavedIndex(int index)
{
  return index;
}

String wifiItemLabel(int index)
{
  if (wifiInstructionOpen) return wifiBackLabel;

  if (!wifiListOpen) {
    if (index == 0) return wifiSetupLabel;
    if (index == 1) return wifiSavedLabel;
    if (index == 2) return wifiInstructionLabel;
    return wifiBackLabel;
  }

  if (wifiIndexIsSaved(index)) {
    int savedIndex = wifiSavedIndex(index);
    if (pendingDeleteWifi == savedIndex) return "Delete? " + savedWiFis[savedIndex].ssid;
    String label = savedWiFis[savedIndex].ssid;
    label += " / ";
    label += wifiJoinTypeLabel(effectiveWiFiJoinType(savedWiFis[savedIndex]));
    if (WiFi.status() == WL_CONNECTED && wifiConfig.ssid == savedWiFis[savedIndex].ssid) label += " / online";
    return label;
  }
  return wifiBackLabel;
}

void keepWifiSelectionVisible()
{
  if (wifiSelectedIndex < wifiTopIndex) wifiTopIndex = wifiSelectedIndex;
  if (wifiSelectedIndex >= wifiTopIndex + WIFI_VISIBLE_ROWS) wifiTopIndex = wifiSelectedIndex - WIFI_VISIBLE_ROWS + 1;
  int maxTop = max(0, wifiItemCount() - WIFI_VISIBLE_ROWS);
  if (wifiTopIndex > maxTop) wifiTopIndex = maxTop;
}

void drawTinyIcon(int kind, int x, int y, uint16_t color, uint16_t bg)
{
  tft.fillRect(x - 1, y - 9, 18, 18, bg);
  switch (kind % 6) {
    case 0:
      tft.drawRoundRect(x + 1, y - 7, 7, 14, 2, color);
      tft.drawRoundRect(x + 8, y - 7, 7, 14, 2, color);
      tft.drawLine(x + 8, y - 6, x + 8, y + 6, color);
      tft.drawFastHLine(x + 3, y - 3, 3, color);
      tft.drawFastHLine(x + 10, y - 3, 3, color);
      break;
    case 1:
      tft.drawFastVLine(x + 3, y - 4, 9, color);
      tft.fillRect(x + 1, y - 2, 5, 5, color);
      tft.drawFastVLine(x + 8, y - 8, 13, color);
      tft.fillRect(x + 6, y - 6, 5, 7, color);
      tft.drawFastVLine(x + 13, y - 6, 12, color);
      tft.fillRect(x + 11, y + 1, 5, 4, color);
      break;
    case 2:
      tft.fillCircle(x + 8, y - 4, 5, color);
      tft.fillCircle(x + 4, y - 1, 4, color);
      tft.fillCircle(x + 12, y - 1, 4, color);
      tft.drawFastVLine(x + 8, y, 8, color);
      tft.drawLine(x + 8, y + 4, x + 4, y + 8, color);
      tft.drawLine(x + 8, y + 4, x + 13, y + 8, color);
      break;
    case 3:
      tft.drawCircle(x + 8, y, 5, color);
      tft.fillCircle(x + 8, y, 2, color);
      tft.drawFastVLine(x + 8, y - 8, 4, color);
      tft.drawFastVLine(x + 8, y + 5, 4, color);
      tft.drawFastHLine(x, y, 4, color);
      tft.drawFastHLine(x + 13, y, 4, color);
      tft.drawLine(x + 3, y - 6, x + 5, y - 4, color);
      tft.drawLine(x + 13, y + 6, x + 11, y + 4, color);
      break;
    case 4:
      tft.drawCircle(x + 8, y, 7, color);
      tft.fillCircle(x + 8, y - 4, 1, color);
      tft.drawFastVLine(x + 8, y - 1, 7, color);
      break;
    default:
      tft.drawCircle(x + 5, y - 2, 4, color);
      tft.drawLine(x + 8, y + 1, x + 16, y + 1, color);
      tft.drawFastVLine(x + 12, y + 1, 4, color);
      tft.drawFastVLine(x + 15, y + 1, 3, color);
      break;
  }
}

void drawSparkle(int x, int y, uint16_t color, uint16_t bg)
{
  tft.fillRect(x - 1, y - 9, 18, 18, bg);
  tft.drawLine(x + 8, y - 8, x + 8, y + 8, color);
  tft.drawLine(x, y, x + 16, y, color);
  tft.drawLine(x + 3, y - 5, x + 13, y + 5, color);
  tft.drawLine(x + 13, y - 5, x + 3, y + 5, color);
}

void drawMiniCandles(int x, int y, uint16_t color, uint16_t bg)
{
  tft.fillRect(x - 1, y - 9, 18, 18, bg);
  tft.drawFastVLine(x + 3, y - 4, 9, color);
  tft.fillRect(x + 1, y - 2, 5, 5, color);
  tft.drawFastVLine(x + 8, y - 8, 13, color);
  tft.fillRect(x + 6, y - 6, 5, 7, color);
  tft.drawFastVLine(x + 13, y - 6, 12, color);
  tft.fillRect(x + 11, y + 1, 5, 4, color);
}

String htmlEscape(const String &text)
{
  String out;
  for (size_t i = 0; i < text.length(); i++) {
    char c = text[i];
    if (c == '&') out += F("&amp;");
    else if (c == '<') out += F("&lt;");
    else if (c == '>') out += F("&gt;");
    else if (c == '"') out += F("&quot;");
    else out += c;
  }
  return out;
}

String encryptionLabel(wifi_auth_mode_t type)
{
  if (type == WIFI_AUTH_OPEN) return "Open";
  if (type == WIFI_AUTH_WEP) return "WEP";
  if (type == WIFI_AUTH_WPA_PSK) return "WPA";
  if (type == WIFI_AUTH_WPA2_PSK) return "WPA2";
  if (type == WIFI_AUTH_WPA_WPA2_PSK) return "WPA/WPA2";
  if (type == WIFI_AUTH_WPA2_ENTERPRISE) return "Enterprise";
  if (type == WIFI_AUTH_WPA3_PSK) return "WPA3";
  if (type == WIFI_AUTH_WPA2_WPA3_PSK) return "WPA2/WPA3";
  return "Secured";
}

String makeNetworkOptions()
{
  String options;
  bool matchedCurrent = false;
  if (scannedNetworkCount <= 0) {
    options += F("<option value=\"\">Tap Refresh Network List</option>");
  } else {
    for (int i = 0; i < scannedNetworkCount; i++) {
      String ssid = scannedNetworks[i].ssid;
      if (ssid.length() == 0) continue;
      bool selected = ssid == wifiConfig.ssid;
      if (selected) matchedCurrent = true;
      options += F("<option value=\"");
      options += htmlEscape(ssid);
      options += F("\"");
      if (selected) options += F(" selected");
      options += F(">");
      options += htmlEscape(ssid);
      options += F(" / ");
      options += String(scannedNetworks[i].rssi);
      options += F(" dBm / ");
      options += encryptionLabel(scannedNetworks[i].auth);
      options += F("</option>");
    }
  }
  return options;
}

void addWiFiLog(const String &message)
{
  String entry = String(millis() / 1000) + "s " + message;
  Serial.println("[WiFi] " + entry);
  if (wifiLogCount < 8) {
    wifiLogLines[wifiLogCount++] = entry;
  } else {
    for (int i = 0; i < 7; i++) wifiLogLines[i] = wifiLogLines[i + 1];
    wifiLogLines[7] = entry;
  }
  wifiUiDirty = true;
}

String makeWiFiDiagnostics()
{
  String html;
  html += "AP=" + String(configApRunning ? "on" : "off");
  html += "  STA status=" + String((int)WiFi.status());
  html += "  mode=" + String((int)WiFi.getMode());
  html += "  scan=" + String(wifiScanInProgress ? "running" : "idle");
  html += "<br>AP clients=" + String(WiFi.softAPgetStationNum());
  html += "  AP IP=" + WiFi.softAPIP().toString();
  html += "<br><b>Recent events</b>";
  for (uint8_t i = 0; i < wifiLogCount; i++) {
    html += "<br>" + htmlEscape(wifiLogLines[i]);
  }
  return html;
}
void scanNearbyNetworks()
{
  if (wifiScanInProgress) {
    statusLine = "Scanning WiFi...";
    addWiFiLog("scan ignored: already running");
    return;
  }

  // The radio cannot reliably scan while a prior STA association attempt is
  // still active. Abort that attempt without touching the AP interface.
  if (connectingWiFi) WiFi.disconnect(false, false);
  connectingWiFi = false;
  autoWiFiConnect = false;
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  wifiUiDirty = true;
  addWiFiLog("scan request: mode=" + String((int)WiFi.getMode()) + " sta=" + String((int)WiFi.status()));
  WiFi.scanDelete();

  // Active scans are markedly more reliable than passive scans on this S3
  // while its AP is beaconing on channel 1.
  int scanState = WiFi.scanNetworks(true, true, false, 300);
  if (scanState == WIFI_SCAN_RUNNING) {
    scannedNetworkCount = 0;
    wifiScanInProgress = true;
    wifiScanStartedAt = millis();
    wifiScanNextReportAt = wifiScanStartedAt + 3000;
    statusLine = "Scanning WiFi...";
    addWiFiLog("scan started");
    return;
  }

  WiFi.scanDelete();
  statusLine = "WiFi scan start error " + String(scanState);
  addWiFiLog("scan start error=" + String(scanState));
}

void updateWiFiScan()
{
  if (!wifiScanInProgress) return;

  int count = WiFi.scanComplete();
  if (count == WIFI_SCAN_RUNNING) {
    if (millis() >= wifiScanNextReportAt) {
      addWiFiLog("scan running " + String((millis() - wifiScanStartedAt) / 1000) + "s");
      wifiScanNextReportAt += 3000;
    }
    if (millis() - wifiScanStartedAt > 20000) {
      wifiScanInProgress = false;
      WiFi.scanDelete();
      statusLine = "WiFi scan timeout";
      addWiFiLog("scan timeout");
      wifiUiDirty = true;
    }
    return;
  }

  wifiScanInProgress = false;
  if (count < 0) {
    statusLine = "WiFi scan result error " + String(count);
    addWiFiLog("scan result error=" + String(count));
    WiFi.scanDelete();
    wifiUiDirty = true;
    return;
  }
  if (count == 0) {
    statusLine = "No 2.4G WiFi found";
    addWiFiLog("scan completed: 0 networks");
    WiFi.scanDelete();
    wifiUiDirty = true;
    return;
  }

  scannedNetworkCount = min(count, (int)(sizeof(scannedNetworks) / sizeof(scannedNetworks[0])));
  for (int i = 0; i < scannedNetworkCount; i++) {
    scannedNetworks[i].ssid = WiFi.SSID(i);
    scannedNetworks[i].rssi = WiFi.RSSI(i);
    scannedNetworks[i].auth = WiFi.encryptionType(i);
  }
  WiFi.scanDelete();
  statusLine = "Scan found " + String(scannedNetworkCount);
  addWiFiLog("scan completed: " + String(scannedNetworkCount) + " networks");
  wifiUiDirty = true;
}
String answerText(int index)
{
  index = ((index % ANSWER_COUNT) + ANSWER_COUNT) % ANSWER_COUNT;
  return answerBookEntries[index];
}

bool readUtf8Codepoint(const String &text, int &pos, uint32_t &codepoint, String &token)
{
  if (pos >= (int)text.length()) return false;
  int start = pos;
  uint8_t first = (uint8_t)text[pos++];

  if (first < 0x80) {
    codepoint = first;
  } else if ((first & 0xE0) == 0xC0 && pos < (int)text.length()) {
    codepoint = first & 0x1F;
    codepoint = (codepoint << 6) | ((uint8_t)text[pos++] & 0x3F);
  } else if ((first & 0xF0) == 0xE0 && pos + 1 < (int)text.length()) {
    codepoint = first & 0x0F;
    codepoint = (codepoint << 6) | ((uint8_t)text[pos++] & 0x3F);
    codepoint = (codepoint << 6) | ((uint8_t)text[pos++] & 0x3F);
  } else if ((first & 0xF8) == 0xF0 && pos + 2 < (int)text.length()) {
    codepoint = first & 0x07;
    codepoint = (codepoint << 6) | ((uint8_t)text[pos++] & 0x3F);
    codepoint = (codepoint << 6) | ((uint8_t)text[pos++] & 0x3F);
    codepoint = (codepoint << 6) | ((uint8_t)text[pos++] & 0x3F);
  } else {
    codepoint = '?';
  }

  token = text.substring(start, pos);
  return true;
}

int findChineseGlyph(uint32_t codepoint)
{
  int low = 0;
  int high = CHINESE_GLYPH_COUNT - 1;
  while (low <= high) {
    int mid = (low + high) / 2;
    uint32_t value = pgm_read_dword(&chineseGlyphs[mid].codepoint);
    if (value == codepoint) return mid;
    if (value < codepoint) low = mid + 1;
    else high = mid - 1;
  }
  return -1;
}

int utf8GlyphAdvance(uint32_t codepoint, const String &token)
{
  if (codepoint < 0x80) {
    if (codepoint == ' ') return 4;
    return tft.textWidth(token, 2);
  }
  return CHINESE_FONT_WIDTH + 1;
}

int measureUtf8Text(const String &text)
{
  int pos = 0;
  int width = 0;
  while (pos < (int)text.length()) {
    uint32_t codepoint;
    String token;
    if (!readUtf8Codepoint(text, pos, codepoint, token)) break;
    width += utf8GlyphAdvance(codepoint, token);
  }
  return width;
}

void drawChineseGlyph(uint32_t codepoint, int x, int y, uint16_t color)
{
  int glyph = findChineseGlyph(codepoint);
  if (glyph < 0) {
    tft.drawRect(x + 2, y + 2, CHINESE_FONT_WIDTH - 4, CHINESE_FONT_HEIGHT - 4, color);
    return;
  }

  uint16_t offset = pgm_read_word(&chineseGlyphs[glyph].offset);
  for (int row = 0; row < CHINESE_FONT_HEIGHT; row++) {
    for (int col = 0; col < CHINESE_FONT_WIDTH; col++) {
      int bitIndex = row * CHINESE_FONT_WIDTH + col;
      uint8_t packed = pgm_read_byte(&chineseGlyphBitmap[offset + bitIndex / 8]);
      if (packed & (0x80 >> (bitIndex % 8))) {
        tft.drawPixel(x + col, y + row, color);
      }
    }
  }
}

void drawUtf8Text(const String &text, int x, int y, uint16_t color, uint16_t bg)
{
  int pos = 0;
  int cursorX = x;
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(color, bg);
  while (pos < (int)text.length()) {
    uint32_t codepoint;
    String token;
    if (!readUtf8Codepoint(text, pos, codepoint, token)) break;
    if (codepoint < 0x80) {
      tft.drawString(token, cursorX, y + 1, 2);
    } else {
      drawChineseGlyph(codepoint, cursorX, y, color);
    }
    cursorX += utf8GlyphAdvance(codepoint, token);
  }
}

void drawWrappedCenterUtf8(const String &text, int centerY, int maxWidth)
{
  constexpr int maxLines = 3;
  constexpr int lineHeight = 19;
  String lines[maxLines];
  int widths[maxLines] = {0, 0, 0};
  int lineCount = 0;
  String current;
  int currentWidth = 0;

  int pos = 0;
  while (pos < (int)text.length()) {
    uint32_t codepoint;
    String token;
    if (!readUtf8Codepoint(text, pos, codepoint, token)) break;
    int advance = utf8GlyphAdvance(codepoint, token);
    if (current.length() > 0 && currentWidth + advance > maxWidth) {
      if (lineCount < maxLines) {
        lines[lineCount] = current;
        widths[lineCount] = currentWidth;
        lineCount++;
      }
      current = "";
      currentWidth = 0;
    }
    if (lineCount >= maxLines) break;
    current += token;
    currentWidth += advance;
  }

  if (lineCount < maxLines && current.length() > 0) {
    lines[lineCount] = current;
    widths[lineCount] = currentWidth;
    lineCount++;
  }

  int top = centerY - (lineCount * lineHeight) / 2;
  for (int i = 0; i < lineCount; i++) {
    int x = (SCREEN_W - widths[i]) / 2;
    drawUtf8Text(lines[i], x, top + i * lineHeight, uiText(), uiBg());
  }
}

void drawWrappedCenter(String text, int y)
{
  tft.setTextDatum(MC_DATUM);
  while (text.length() > 0) {
    int split = text.length() > 34 ? 34 : text.length();
    if (split < text.length()) {
      int space = text.lastIndexOf(' ', split);
      if (space > 5) split = space;
    }
    String line = text.substring(0, split);
    text.remove(0, split);
    text.trim();
    tft.drawString(line, SCREEN_W / 2, y, 2);
    y += 20;
  }
}

void drawAnswerContent()
{
  tft.fillRect(10, 50, 300, 84, uiBg());
  tft.drawRoundRect(8, 48, 304, 88, 6, uiSelect());
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(uiMint(), uiBg());
  tft.drawString(String(answerIndex + 1) + "/" + String(ANSWER_COUNT), SCREEN_W / 2, 64, 2);
  drawWrappedCenterUtf8(answerText(answerIndex), 102, 284);
}

void drawSelectableRow(int index, int y, const String &text, bool selected)
{
  uint16_t bg = selected ? uiSelect() : uiBg();
  uint16_t fg = selected ? uiHeaderText() : uiText();
  tft.fillRect(0, y - 16, SCREEN_W, 34, uiBg());
  if (selected) {
    tft.fillRoundRect(8, y - 13, SCREEN_W - 16, 28, 4, bg);
    tft.fillTriangle(SCREEN_W - 24, y - 5, SCREEN_W - 24, y + 5, SCREEN_W - 14, y, uiHeaderText());
  }
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(fg, bg);
  drawTinyIcon(mainMenuIconKinds[index], 18, y, selected ? uiHeaderText() : uiMint(), bg);
  drawMarqueeText(text, 44, y, SCREEN_W - 76, fg, bg, selected);
  (void)index;
}

void drawWiFiRow(int index, bool selected)
{
  int x = 164;
  int row = index - wifiTopIndex;
  int y = 42 + row * 23;
  int w = 148;
  uint16_t bg = selected ? uiSelect() : uiBg();
  uint16_t fg = selected ? uiHeaderText() : uiText();
  tft.fillRect(x - 4, y - 13, w + 8, 26, uiBg());
  if (selected) {
    tft.fillRoundRect(x, y - 11, w, 22, 4, bg);
    tft.fillTriangle(x + w - 18, y - 5, x + w - 18, y + 5, x + w - 8, y, uiHeaderText());
  }
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(fg, bg);
  drawTinyIcon(index + 3, x + 8, y, selected ? uiHeaderText() : uiMint(), bg);
  drawMarqueeText(wifiItemLabel(index), x + 30, y, w - 52, fg, bg, selected);
}

void drawWiFiRows()
{
  keepWifiSelectionVisible();
  tft.fillRect(158, 30, 160, 118, uiBg());
  int count = wifiItemCount();
  int last = min(count, wifiTopIndex + WIFI_VISIBLE_ROWS);
  for (int i = wifiTopIndex; i < last; i++) {
    drawWiFiRow(i, i == wifiSelectedIndex);
  }

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(uiMuted(), uiBg());
  if (wifiTopIndex > 0) tft.drawString("^", 310, 34, 2);
  if (last < count) tft.drawString("v", 310, 142, 2);
}

void drawMainRows()
{
  tft.fillRect(0, 30, SCREEN_W, 120, uiBg());
  int last = min(MAIN_MENU_COUNT, mainTopIndex + MAIN_MENU_VISIBLE_ROWS);
  for (int i = mainTopIndex; i < last; i++) {
    drawSelectableRow(i, 42 + (i - mainTopIndex) * 24, mainMenu[i], i == selectedIndex);
  }

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(uiMuted(), uiBg());
  if (mainTopIndex > 0) tft.drawString("^", 310, 34, 2);
  if (last < MAIN_MENU_COUNT) tft.drawString("v", 310, 142, 2);
}

void drawMainMenu()
{
  tft.fillScreen(uiBg());
  drawHeader("Main Control");
  renderedSelectedIndex = selectedIndex;
  drawMainRows();

  String wifi = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "WiFi offline";
  drawFooter(wifi);
}
void updateMainSelection(int oldIndex, int newIndex)
{
  drawSelectableRow(oldIndex, 42 + (oldIndex - mainTopIndex) * 24, mainMenu[oldIndex], false);
  drawSelectableRow(newIndex, 42 + (newIndex - mainTopIndex) * 24, mainMenu[newIndex], true);
  renderedSelectedIndex = newIndex;
}
void drawAnswerBook()
{
  tft.fillScreen(uiBg());
  drawHeader("Answer Book");

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(uiMuted(), uiBg());
  tft.drawString("Turn to browse", SCREEN_W / 2, 42, 2);
  drawSparkle(18, 42, uiMint(), uiBg());
  drawSparkle(SCREEN_W - 34, 42, uiHeader(), uiBg());

  drawAnswerContent();
  drawFooter("Press: shuffle  Hold: back");
}

void drawMarketTicker()
{
  tft.fillScreen(uiBg());
  drawHeader("Market Ticker");

  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(uiMuted(), uiBg());
  String top = WiFi.status() == WL_CONNECTED ? "Online " + WiFi.localIP().toString() : "WiFi offline";
  drawMiniCandles(12, 42, uiMint(), uiBg());
  tft.drawString(top.substring(0, 32), 36, 42, 2);
  drawFooter("Press: refresh  Hold: back");
  drawMarketRows();
}

void drawMarketRows()
{
  tft.fillRect(0, 54, SCREEN_W, 94, uiBg());
  for (int i = 0; i < MARKET_COUNT; i++) {
    int y = 66 + i * 20;
    if (y > 140) break;
    bool hasPct = markets[i].valid && !isnan(markets[i].changePct);
    uint16_t fg = markets[i].valid ? (hasPct ? (markets[i].changePct >= 0 ? uiMint() : uiCoral()) : uiMint()) : uiMuted();
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(uiText(), uiBg());
    tft.drawString(markets[i].name, 8, y, 2);
    if (markets[i].valid) {
      char priceLine[18];
      char pctLine[14];
      snprintf(priceLine, sizeof(priceLine), "%.2f", markets[i].price);
      if (hasPct) snprintf(pctLine, sizeof(pctLine), "%+0.2f%%", markets[i].changePct);
      else snprintf(pctLine, sizeof(pctLine), "--");

      tft.setTextDatum(MR_DATUM);
      tft.setTextColor(uiText(), uiBg());
      tft.drawString(priceLine, 236, y, 2);
      tft.setTextColor(fg, uiBg());
      tft.drawString(pctLine, SCREEN_W - 8, y, 2);
    } else {
      tft.setTextDatum(MR_DATUM);
      tft.setTextColor(fg, uiBg());
      tft.drawString(markets[i].error.length() ? markets[i].error : "--", SCREEN_W - 8, y, 2);
    }
  }
}

void drawCo2Values()
{
  tft.fillRect(96, 42, SCREEN_W - 96, 100, uiBg());
  tft.drawCircle(280, 58, 8, uiSelect());
  tft.drawCircle(294, 82, 12, uiMint());
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(uiMuted(), uiBg());
  tft.drawString("CO2", SCREEN_W / 2, 50, 2);

  tft.setTextColor(co2Ppm >= 1000 ? uiCoral() : (co2Ppm >= 0 ? uiMint() : uiMuted()), uiBg());
  if (co2Ppm >= 0) {
    tft.drawString(String(co2Ppm), SCREEN_W / 2, 86, 6);
  } else {
    tft.drawString("--", SCREEN_W / 2, 86, 6);
  }

  uint32_t age = lastCo2FrameAt == 0 ? 0 : (millis() - lastCo2FrameAt) / 1000;
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(uiText(), uiBg());
  tft.drawString("ppm", SCREEN_W / 2 + 86, 92, 2);

  String level = "NO DATA";
  uint16_t levelColor = uiMuted();
  if (co2Ppm >= 0 && age <= 3) {
    if (co2Ppm < 800) {
      level = "GOOD";
      levelColor = uiMint();
    } else if (co2Ppm < 1200) {
      level = "MED";
      levelColor = uiLemon();
    } else if (co2Ppm < 2000) {
      level = "HIGH";
      levelColor = uiHeader();
    } else {
      level = "BAD";
      levelColor = uiCoral();
    }
  }

  tft.setTextColor(levelColor, uiBg());
  tft.drawString(level, SCREEN_W / 2, 124, 2);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(uiText(), uiBg());
  tft.drawString(lastCo2FrameAt ? String(age) + "s" : String("--"), SCREEN_W - 12, 128, 2);
}

void drawCo2DogAvatar()
{
  static bool spriteReady = false;
  if (!spriteReady) {
    dogSprite.setColorDepth(16);
    dogSprite.createSprite(86, 90);
    spriteReady = true;
  }

  uint32_t age = lastCo2FrameAt == 0 ? 999 : (millis() - lastCo2FrameAt) / 1000;
  int mood = 0;
  if (co2Ppm < 0 || age > 3) mood = 0;
  else if (co2Ppm < 800) mood = 1;
  else if (co2Ppm < 1200) mood = 2;
  else if (co2Ppm < 2000) mood = 3;
  else mood = 4;

  int frame = (millis() / 240) % 24;
  int action = frame % 6;
  bool blink = action == 0 && mood != 4;
  int earWiggle = (action == 1) ? -3 : ((action == 4) ? 3 : 0);
  int breath = action < 3 ? 0 : 1;
  int headBob = (action == 2 || action == 3) ? 1 : 0;
  int noseWiggle = (action == 1 || action == 5) ? 1 : 0;
  int pawLift = (action == 2 || action == 5) ? -3 : 0;
  if (mood == 2) {
    earWiggle += 2;
    headBob += 1;
  } else if (mood == 3) {
    headBob = (action % 2 == 0) ? -1 : 1;
  } else if (mood == 4) {
    earWiggle = (action % 2 == 0) ? -4 : 4;
    headBob = (action % 2 == 0) ? -1 : 1;
  }

  uint16_t tan = tft.color565(190, 126, 62);
  uint16_t lightTan = tft.color565(226, 185, 122);
  uint16_t cream = tft.color565(245, 220, 164);
  uint16_t shadow = tft.color565(116, 70, 42);
  uint16_t dark = tft.color565(42, 31, 31);
  uint16_t nose = tft.color565(36, 33, 37);
  uint16_t blush = tft.color565(255, 178, 190);
  uint16_t sweat = tft.color565(126, 216, 255);

  dogSprite.fillSprite(uiBg());
  int x = 34 + (mood == 0 && action >= 3 ? 1 : 0);
  int y = 43 + headBob;

  dogSprite.fillTriangle(x - 24, y - 20, x - 8, y - 54 + earWiggle, x - 2, y - 15, tan);
  dogSprite.fillTriangle(x - 18, y - 22, x - 8, y - 44 + earWiggle, x - 6, y - 17, cream);
  dogSprite.drawLine(x - 23, y - 20, x - 8, y - 54 + earWiggle, shadow);

  dogSprite.fillTriangle(x + 4, y - 17, x + 20, y - 46 - earWiggle, x + 22, y - 8, tan);
  dogSprite.fillTriangle(x + 9, y - 16, x + 18, y - 36 - earWiggle, x + 18, y - 8, cream);
  dogSprite.drawLine(x + 4, y - 17, x + 20, y - 46 - earWiggle, shadow);

  dogSprite.fillEllipse(x, y - 10, 24, 28, tan);
  dogSprite.fillEllipse(x + 17, y - 5, 22, 17, lightTan);
  dogSprite.fillEllipse(x + 19, y + 2, 18, 11, cream);
  dogSprite.fillEllipse(x - 7, y + 8, 14, 10, cream);
  dogSprite.fillTriangle(x - 20, y - 20, x - 1, y - 33, x + 10, y - 20, lightTan);

  if (mood == 4) {
    dogSprite.drawLine(x + 4, y - 17, x + 12, y - 9, dark);
    dogSprite.drawLine(x + 12, y - 17, x + 4, y - 9, dark);
    if (action >= 3) {
      dogSprite.drawCircle(x + 8, y - 13, 6, dark);
      dogSprite.drawCircle(x + 8, y - 13, 3, dark);
    }
  } else if (mood == 3) {
    dogSprite.drawLine(x + 3, y - 16, x + 12, y - 12, dark);
    dogSprite.fillCircle(x + 9, y - 11, action == 4 ? 1 : 2, dark);
    dogSprite.fillEllipse(x + 26, y - 19 + breath, 3, 6, sweat);
    if (action == 1 || action == 5) dogSprite.fillEllipse(x + 31, y - 12, 2, 4, sweat);
  } else if (mood == 2 || blink) {
    dogSprite.drawFastHLine(x + 4, y - 12 + (mood == 2 ? 1 : 0), 8, dark);
  } else {
    dogSprite.fillCircle(x + 8, y - 13, mood == 0 && action >= 3 ? 4 : 3, dark);
    dogSprite.fillCircle(x + 9 + (mood == 0 ? noseWiggle : 0), y - 14, 1, uiText());
  }

  dogSprite.fillEllipse(x + 34 + breath + noseWiggle, y - 5, 6, 4, nose);
  dogSprite.drawLine(x + 28, y, x + 20, y + 4, shadow);
  if (mood == 1) {
    dogSprite.drawLine(x + 22, y + 7, x + 26, y + 9, shadow);
    dogSprite.drawLine(x + 26, y + 9, x + 31, y + 6, shadow);
    if (action == 3 || action == 4) {
      dogSprite.fillEllipse(x + 28, y + 12, 4, 5, blush);
      dogSprite.drawFastVLine(x + 28, y + 9, 5, shadow);
    }
  } else if (mood == 2) {
    dogSprite.drawFastHLine(x + 24, y + 7, 7, shadow);
    dogSprite.fillCircle(x + 31, y + 12 + breath, 2, blush);
    if (action >= 3) dogSprite.fillEllipse(x + 33, y + 15, 2, 4, sweat);
  } else if (mood == 3) {
    dogSprite.drawLine(x + 24, y + 10, x + 28, y + 7, shadow);
    dogSprite.drawLine(x + 28, y + 7, x + 32, y + 10, shadow);
  } else if (mood == 4) {
    dogSprite.drawCircle(x + 27, y + 8, 3, shadow);
    dogSprite.drawCircle(x + 28, y + 8, 3, shadow);
  } else {
    dogSprite.drawPixel(x + 29, y + 6, shadow);
    dogSprite.drawPixel(x + 30, y + 7, shadow);
    if (action == 2) {
      dogSprite.drawFastHLine(x + 38, y - 5, 5, uiMuted());
      dogSprite.drawFastHLine(x + 40, y - 1, 6, uiMuted());
    }
  }
  dogSprite.fillCircle(x + 7, y + 3, 2, blush);

  dogSprite.setTextColor(uiText(), uiBg());
  if (mood == 0) {
    if (action < 3) dogSprite.drawString("?", x + 42, y - 24, 2);
    else {
      dogSprite.drawCircle(x + 44, y - 20, 3, uiMint());
      dogSprite.drawPixel(x + 45, y - 21, uiText());
    }
  } else if (mood == 1) {
    if (action == 1 || action == 4) {
      dogSprite.fillCircle(x + 43, y - 21, 3, blush);
      dogSprite.fillCircle(x + 48, y - 21, 3, blush);
      dogSprite.fillTriangle(x + 40, y - 20, x + 51, y - 20, x + 46, y - 13, blush);
    } else {
      dogSprite.drawCircle(x - 24, y - 22, 2 + breath, uiMint());
      dogSprite.fillCircle(x + 41, y - 21, 2, uiLemon());
      if (action == 5) dogSprite.drawLine(x - 26, y - 28, x - 21, y - 34, uiLemon());
    }
  } else if (mood == 2) {
    dogSprite.drawString(action < 3 ? "z" : "Z", x + 38, y - 24, 2);
    dogSprite.drawString("Z", x + 48, y - 34 - breath, 2);
  } else if (mood == 3) {
    dogSprite.drawString(action % 2 == 0 ? "!" : "!!", x + 39, y - 26, 2);
  } else {
    dogSprite.drawCircle(x + 42, y - 24, 5 + breath, uiCoral());
    dogSprite.drawLine(x + 38, y - 28, x + 47, y - 20, uiCoral());
    dogSprite.drawLine(x + 47, y - 28, x + 38, y - 20, uiCoral());
  }

  dogSprite.fillEllipse(x - 12, y + 28 + pawLift, 7, 5, lightTan);
  dogSprite.drawLine(x - 15, y + 28 + pawLift, x - 18, y + 24 + pawLift, shadow);
  dogSprite.fillTriangle(x - 4, y + 12, x + 8, y + 28, x - 16, y + 28, cream);
  dogSprite.drawFastHLine(x - 18, y + 24, 35, cream);
  dogSprite.drawFastHLine(x - 20, y + 27, 40, lightTan);
  dogSprite.pushSprite(10, 50);
}

void drawCo2Sensor()
{
  tft.fillScreen(uiBg());
  drawHeader("CO2 Level");
  drawCo2Values();
  drawCo2DogAvatar();
  drawFooter("Press: reset  Hold: back");
}

void drawWiFiSettings()
{
  tft.fillScreen(uiBg());
  drawHeader(wifiInstructionOpen ? "Setting Help" : (wifiListOpen ? "Saved WiFi" : "WiFi Settings"));
  keepWifiSelectionVisible();
  renderedWifiIndex = wifiSelectedIndex;

  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(uiMuted(), uiBg());
  if (wifiInstructionOpen) {
    tft.drawString(String("1. Join ") + SETUP_AP_SSID, 8, 42, 2);
    tft.drawString(String("2. Password ") + SETUP_AP_PASS, 8, 62, 2);
    tft.drawString("3. Open 192.168.4.1", 8, 82, 2);
    tft.drawString("4. Pick WiFi and save", 8, 102, 2);
    tft.setTextColor(uiText(), uiBg());
    tft.drawString("Enterprise: user/pass", 8, 126, 2);
    tft.drawString("Portal: web login", 8, 144, 2);
  } else if (!wifiListOpen) {
    tft.drawString(String("AP: ") + SETUP_AP_SSID, 8, 42, 2);
    tft.drawString(String("Pass: ") + SETUP_AP_PASS, 8, 62, 2);
    tft.drawString("Portal: 192.168.4.1", 8, 82, 2);

    tft.setTextColor(uiText(), uiBg());
    String saved = wifiConfig.ssid.length() ? wifiConfig.ssid : "(not set)";
    drawMarqueeText("Current: " + saved, 8, 108, 146, uiText(), uiBg(), true);
    tft.drawString("Saved: " + String(savedWiFiCount), 8, 128, 2);
  } else {
    tft.drawString("Press: connect", 8, 48, 2);
    tft.drawString("Hold: delete", 8, 70, 2);
    tft.drawString("Saved: " + String(savedWiFiCount), 8, 100, 2);
    if (savedWiFiCount == 0) {
      tft.setTextColor(uiText(), uiBg());
      tft.drawString("Use AP setup first", 8, 126, 2);
    }
  }

  drawWiFiRows();

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(uiMuted(), uiBg());
  String latestLog = wifiLogCount ? wifiLogLines[wifiLogCount - 1] : "WiFi log waiting";
  tft.fillRect(4, 132, 150, 17, uiBg());
  tft.drawString("LOG " + latestLog.substring(0, 28), 8, 132, 1);
  tft.drawString("M" + String((int)WiFi.getMode()) + " CH" + String(SETUP_AP_CHANNEL) + " STA" + String(WiFi.softAPgetStationNum()), 8, 142, 1);
  String wifi = WiFi.status() == WL_CONNECTED ? "STA " + WiFi.localIP().toString() : statusLine;
  if (pendingDeleteWifi >= 0) wifi = "Press to delete, hold to cancel";
  drawFooter(wifi);
}

void updateWiFiSelection(int oldIndex, int newIndex)
{
  keepWifiSelectionVisible();
  drawWiFiRows();
  if (pendingDeleteWifi >= 0) drawFooter("Press to delete, hold to cancel");
  renderedWifiIndex = newIndex;
}

void drawAbout()
{
  tft.fillScreen(uiBg());
  drawHeader("About");
  drawTinyIcon(4, 24, 52, uiMint(), uiBg());
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(uiText(), uiBg());
  tft.drawString("Jianjian", 48, 50, 4);
  tft.setTextColor(uiMuted(), uiBg());
  tft.drawString("A tiny desktop CO2 buddy", 48, 96, 2);
  tft.setTextColor(uiText(), uiBg());
  tft.drawString("by zihao", 48, 124, 2);
  drawFooter("Hold: back");
}

void drawCurrentScreen()
{
  if (mode == ScreenMode::MainMenu) drawMainMenu();
  else if (mode == ScreenMode::AnswerBook) drawAnswerBook();
  else if (mode == ScreenMode::MarketTicker) drawMarketTicker();
  else if (mode == ScreenMode::CO2Sensor) drawCo2Sensor();
  else if (mode == ScreenMode::PasswordVault) { if (vaultPage == VaultPage::SafeDial) drawSafeDial(); else drawPasswordVault(); }
  else if (mode == ScreenMode::WiFiSettings) drawWiFiSettings();
  else if (mode == ScreenMode::About) drawAbout();
}

void moveMainSelection(int direction)
{
  int oldIndex = selectedIndex;
  int oldTopIndex = mainTopIndex;
  selectedIndex += direction;
  if (selectedIndex < 0) selectedIndex = MAIN_MENU_COUNT - 1;
  if (selectedIndex >= MAIN_MENU_COUNT) selectedIndex = 0;
  if (selectedIndex < mainTopIndex) mainTopIndex = selectedIndex;
  if (selectedIndex >= mainTopIndex + MAIN_MENU_VISIBLE_ROWS) mainTopIndex = selectedIndex - MAIN_MENU_VISIBLE_ROWS + 1;

  if (mainTopIndex != oldTopIndex) drawMainRows();
  else updateMainSelection(oldIndex, selectedIndex);
}

void moveWiFiSelection(int direction)
{
  pendingDeleteWifi = -1;
  int old = wifiSelectedIndex;
  wifiSelectedIndex += direction;
  int count = wifiItemCount();
  if (wifiSelectedIndex < 0) wifiSelectedIndex = count - 1;
  if (wifiSelectedIndex >= count) wifiSelectedIndex = 0;
  updateWiFiSelection(old, wifiSelectedIndex);
}

void moveAnswer(int direction)
{
  answerIndex += direction;
  if (answerIndex < 0) answerIndex = ANSWER_COUNT - 1;
  if (answerIndex >= ANSWER_COUNT) answerIndex = 0;
  drawAnswerContent();
}

float parseJsonNumber(const String &body, const char *key, bool &ok)
{
  ok = false;
  int at = body.indexOf(key);
  if (at < 0) return 0;
  at += strlen(key);
  while (at < (int)body.length() && (body[at] == ' ' || body[at] == ':')) at++;
  int end = at;
  while (end < (int)body.length()) {
    char c = body[end];
    if (!isDigit(c) && c != '-' && c != '+' && c != '.') break;
    end++;
  }
  if (end <= at) return 0;
  ok = true;
  return body.substring(at, end).toFloat();
}

bool fetchMarket(MarketIndex &market)
{
  if (WiFi.status() != WL_CONNECTED) {
    market.error = "offline";
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url = "https://query1.finance.yahoo.com/v8/finance/chart/";
  url += market.symbol;
  url += "?range=1d&interval=1d";

  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("Mozilla/5.0 ESP32-S3");
  http.setTimeout(6500);

  if (!http.begin(client, url)) {
    market.error = "http";
    return false;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    market.error = String(code);
    http.end();
    return false;
  }

  String body = http.getString();
  http.end();

  bool okPrice = false;
  bool okPct = false;
  bool okPrev = false;
  float price = parseJsonNumber(body, "\"regularMarketPrice\"", okPrice);
  float pct = parseJsonNumber(body, "\"regularMarketChangePercent\"", okPct);
  float previousClose = parseJsonNumber(body, "\"chartPreviousClose\"", okPrev);
  if (!okPrev) previousClose = parseJsonNumber(body, "\"previousClose\"", okPrev);
  if (!okPrev) previousClose = parseJsonNumber(body, "\"regularMarketPreviousClose\"", okPrev);
  if (!okPrice) {
    market.error = "parse";
    return false;
  }

  market.price = price;
  if (okPct) {
    market.changePct = pct;
  } else if (okPrev && previousClose != 0) {
    market.changePct = (price - previousClose) * 100.0f / previousClose;
  } else {
    market.changePct = NAN;
  }
  market.valid = true;
  market.error = "";
  market.updatedAt = millis();
  return true;
}

void refreshOneMarket(bool force)
{
  if (mode != ScreenMode::MarketTicker && !force) return;
  if (!force && millis() - lastMarketFetch < 9000) return;
  lastMarketFetch = millis();

  fetchMarket(markets[marketFetchIndex]);
  marketFetchIndex = (marketFetchIndex + 1) % MARKET_COUNT;
  drawMarketRows();
}

void refreshAllMarkets()
{
  for (int i = 0; i < MARKET_COUNT; i++) {
    fetchMarket(markets[i]);
    drawMarketRows();
    delay(20);
  }
}

String hexFrame(const uint8_t *data, uint8_t len)
{
  String out;
  for (uint8_t i = 0; i < len; i++) {
    if (i) out += ' ';
    if (data[i] < 16) out += '0';
    out += String(data[i], HEX);
  }
  out.toUpperCase();
  return out;
}

bool plausibleCo2(int value)
{
  return value >= 300 && value <= 10000;
}

bool acceptCo2Value(int value, const uint8_t *frame, uint8_t len, const char *status)
{
  if (!plausibleCo2(value)) return false;
  co2Ppm = value;
  co2RawFrame = hexFrame(frame, len);
  co2Status = status;
  co2FrameCount++;
  lastCo2FrameAt = millis();
  Serial.printf("JW01 CO2=%d ppm, frame=%s, status=%s\n", co2Ppm, co2RawFrame.c_str(), status);
  return true;
}

bool parseSixByteFrame(const uint8_t *frame)
{
  if (frame[0] != 0x2C) return false;
  uint8_t sum = 0;
  for (int i = 0; i < 5; i++) sum += frame[i];
  if (sum != frame[5]) return false;

  int value = (frame[1] << 8) | frame[2];
  return acceptCo2Value(value, frame, 6, "JW01 0x2C frame");
}

bool parseKnownFrame(const uint8_t *frame, uint8_t len)
{
  if (len >= 9) {
    if (acceptCo2Value((frame[3] << 8) | frame[4], frame, min<uint8_t>(len, 11), "air-quality frame")) return true;
    if (acceptCo2Value((frame[6] << 8) | frame[7], frame, min<uint8_t>(len, 11), "CO2 offset 6")) return true;
  }

  if (len >= 8 && frame[0] == 0x42 && frame[1] == 0x4D) {
    for (uint8_t i = 2; i + 1 < min<uint8_t>(len, 16); i++) {
      if (acceptCo2Value((frame[i] << 8) | frame[i + 1], frame, min<uint8_t>(len, 16), "42 4D frame")) return true;
    }
  }

  return false;
}

void trimJw01Buffer(uint8_t count)
{
  if (count >= jw01BufferLen) {
    jw01BufferLen = 0;
    return;
  }
  memmove(jw01Buffer, jw01Buffer + count, jw01BufferLen - count);
  jw01BufferLen -= count;
}

void parseJw01Buffer()
{
  bool parsed = true;
  while (parsed && jw01BufferLen >= 6) {
    parsed = false;

    for (uint8_t start = 0; start + 6 <= jw01BufferLen; start++) {
      if (jw01Buffer[start] != 0x2C) continue;
      if (parseSixByteFrame(jw01Buffer + start)) {
        trimJw01Buffer(start + 6);
        parsed = true;
        break;
      }
    }
  }

  if (jw01BufferLen > 32) trimJw01Buffer(jw01BufferLen - 16);
}

void pollJw01()
{
  lastCo2Poll = millis();
}

void setJw01Baud(uint8_t index)
{
  jw01BaudIndex = index % (sizeof(jw01Bauds) / sizeof(jw01Bauds[0]));
  jw01Serial.end();
  delay(20);
  jw01Serial.begin(jw01Bauds[jw01BaudIndex], SERIAL_8N1, JW01_RX_PIN, JW01_TX_PIN);
  jw01BufferLen = 0;
  co2RawFrame = "none";
  co2Status = "trying " + String(jw01Bauds[jw01BaudIndex]);
  lastCo2BaudSwitch = millis();
  co2BytesAtBaudSwitch = co2ByteCount;
  lastCo2Poll = 0;
  Serial.printf("JW01 baud switched to %lu\n", (unsigned long)jw01Bauds[jw01BaudIndex]);
  if (mode == ScreenMode::CO2Sensor) drawCo2Values();
}

void updateJw01()
{
  while (jw01Serial.available()) {
    uint8_t b = jw01Serial.read();
    co2ByteCount++;
    lastCo2ByteAt = millis();
    if (jw01BufferLen < sizeof(jw01Buffer)) {
      jw01Buffer[jw01BufferLen++] = b;
    } else {
      trimJw01Buffer(1);
      jw01Buffer[jw01BufferLen++] = b;
    }
    co2RawFrame = hexFrame(jw01Buffer, min<uint8_t>(jw01BufferLen, 16));
  }

  parseJw01Buffer();

  if (co2ByteCount == 0 && millis() > 7000) {
    co2Status = "no RX bytes; check B->GPIO18";
  } else if (co2ByteCount > 0 && lastCo2FrameAt == 0) {
    co2Status = "RX bytes, waiting 0x2C frame";
  } else if (lastCo2FrameAt && millis() - lastCo2FrameAt > 7000) {
    co2Status = "stale data";
  }

  if (mode == ScreenMode::CO2Sensor && millis() - lastCo2Draw > 1000) {
    lastCo2Draw = millis();
    drawCo2Values();
  }
}

void beginEnterpriseWiFi()
{
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect(false, false);
  delay(200);

#if defined(HAS_ESP_EAP_CLIENT)
  esp_eap_client_set_identity((const unsigned char *)wifiConfig.username.c_str(), wifiConfig.username.length());
  esp_eap_client_set_username((const unsigned char *)wifiConfig.username.c_str(), wifiConfig.username.length());
  esp_eap_client_set_password((const unsigned char *)wifiConfig.password.c_str(), wifiConfig.password.length());
  esp_wifi_sta_enterprise_enable();
#elif defined(HAS_ESP_WPA2)
  esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)wifiConfig.username.c_str(), wifiConfig.username.length());
  esp_wifi_sta_wpa2_ent_set_username((uint8_t *)wifiConfig.username.c_str(), wifiConfig.username.length());
  esp_wifi_sta_wpa2_ent_set_password((uint8_t *)wifiConfig.password.c_str(), wifiConfig.password.length());
  esp_wifi_sta_wpa2_ent_enable();
#endif

  WiFi.begin(wifiConfig.ssid.c_str());
}

void beginNormalWiFi()
{
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect(false, false);
  delay(200);
  uint8_t joinType = effectiveWiFiJoinType(wifiConfig);
  if (joinType == WIFI_JOIN_OPEN || wifiConfig.password.length() == 0) WiFi.begin(wifiConfig.ssid.c_str());
  else WiFi.begin(wifiConfig.ssid.c_str(), wifiConfig.password.c_str());
}

void connectWiFi(bool showSettings, bool autoMode)
{
  if (wifiConfig.ssid.length() == 0) {
    statusLine = "SSID empty";
    wifiUiDirty = true;
    drawWiFiSettings();
    return;
  }

  autoWiFiConnect = autoMode;
  connectingWiFi = true;
  wifiConnectStarted = millis();
  statusLine = autoMode ? ("Auto WiFi " + String(autoWiFiAttempt + 1) + "/" + String(max(1, savedWiFiCount))) : "Connecting...";
  wifiUiDirty = true;
  if (showSettings) drawWiFiSettings();
  else drawCurrentScreen();

  uint8_t joinType = effectiveWiFiJoinType(wifiConfig);
  Serial.printf("WiFi connect start: ssid=%s type=%u enterprise=%d user=%s\n",
                wifiConfig.ssid.c_str(),
                joinType,
                wifiConfig.enterprise,
                wifiConfig.username.c_str());

  if (joinType == WIFI_JOIN_ENTERPRISE) beginEnterpriseWiFi();
  else beginNormalWiFi();
}

void updateWiFiConnect()
{
  if (!connectingWiFi) return;

  if (WiFi.status() == WL_CONNECTED) {
    connectingWiFi = false;
    autoWiFiConnect = false;
    statusLine = "Connected";
    saveConfig();
    wifiUiDirty = true;
    Serial.printf("WiFi connected: ip=%s rssi=%d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    if (effectiveWiFiJoinType(wifiConfig) == WIFI_JOIN_PORTAL) {
      portalCheckPending = true;
      portalCheckReadyAt = millis() + 1500;
      statusLine = "Connected; checking portal";
    }
    if (mode == ScreenMode::WiFiSettings || mode == ScreenMode::MainMenu) drawCurrentScreen();
    return;
  }

  if (millis() - wifiConnectStarted > 18000) {
    wl_status_t status = WiFi.status();
    Serial.printf("WiFi connect failed: status=%d ssid=%s\n", (int)status, wifiConfig.ssid.c_str());

    if (autoWiFiConnect && savedWiFiCount > 1 && autoWiFiAttempt + 1 < savedWiFiCount) {
      autoWiFiAttempt++;
      int nextIndex = (autoWiFiBaseIndex + autoWiFiAttempt) % savedWiFiCount;
      wifiConfig = savedWiFis[nextIndex];
      statusLine = "Auto WiFi " + String(autoWiFiAttempt + 1) + "/" + String(savedWiFiCount);
      Serial.printf("Auto WiFi trying next: %s\n", wifiConfig.ssid.c_str());
      connectWiFi(false, true);
      return;
    }

    connectingWiFi = false;
    autoWiFiConnect = false;
    statusLine = "Failed code " + String((int)status);
    wifiUiDirty = true;
    if (mode == ScreenMode::WiFiSettings || mode == ScreenMode::MainMenu) drawCurrentScreen();
  }
}

String checkUpstreamPortal()
{
  portalRedirectUrl = "";
  portalProbeStatus = "";
  if (WiFi.status() != WL_CONNECTED) {
    portalProbeStatus = "ESP32 is not connected to upstream WiFi.";
    return portalProbeStatus;
  }

  struct PortalProbe {
    const char *url;
    const char *openMarker;
    int openCode;
  };
  const PortalProbe probes[] = {
    {"http://connectivitycheck.gstatic.com/generate_204", "", 204},
    {"http://www.gstatic.com/generate_204", "", 204},
    {"http://captive.apple.com/hotspot-detect.html", "Success", 200},
    {"http://www.msftconnecttest.com/connecttest.txt", "Microsoft Connect Test", 200},
    {"http://neverssl.com/", "NeverSSL", 200}
  };

  String lastResult;
  for (const PortalProbe &probe : probes) {
    WiFiClient client;
    HTTPClient http;
    const char *headers[] = {"Location"};
    http.setTimeout(5000);
    http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
    http.collectHeaders(headers, 1);
    if (!http.begin(client, probe.url)) {
      lastResult = "begin failed: " + String(probe.url);
      continue;
    }

    int code = http.GET();
    String location = http.header("Location");
    String body;
    if (code > 0) body = http.getString().substring(0, 240);
    http.end();

    lastResult = String(probe.url) + " code=" + String(code);
    if (location.length()) lastResult += " location=" + location;

    if (code >= 300 && code < 400 && location.length()) {
      portalRedirectUrl = location;
      portalProbeStatus = "Captive portal redirect detected.";
      return portalProbeStatus;
    }

    if (code == probe.openCode && (strlen(probe.openMarker) == 0 || body.indexOf(probe.openMarker) >= 0)) {
      portalProbeStatus = "Internet looks open; no captive portal detected.";
      return portalProbeStatus;
    }

    if (code == 200 && body.length()) {
      String lower = body;
      lower.toLowerCase();
      if (lower.indexOf("login") >= 0 || lower.indexOf("password") >= 0 || lower.indexOf("portal") >= 0 || lower.indexOf("认证") >= 0) {
        portalRedirectUrl = probe.url;
        portalProbeStatus = "Portal page detected without Location header.";
        return portalProbeStatus;
      }
    }
  }

  portalProbeStatus = "No portal link found. Last probe: " + lastResult;
  return portalProbeStatus;
}
bool currentWiFiSsidInScan()
{
  if (wifiConfig.ssid.length() == 0) return false;
  for (int i = 0; i < scannedNetworkCount; i++) {
    if (scannedNetworks[i].ssid == wifiConfig.ssid) return true;
  }
  return false;
}
String makeSetupPage()
{
  String page;
  page.reserve(9400);
  uint8_t currentJoinType = effectiveWiFiJoinType(wifiConfig);
  bool currentSsidIsScanned = currentWiFiSsidInScan();
  bool currentIsHiddenPersonal = currentJoinType == WIFI_JOIN_PERSONAL && wifiConfig.ssid.length() > 0 && !currentSsidIsScanned;
  page += F("<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>");
  page += F("<title>ESP32 WiFi Setup</title><style>");
  page += F("body{margin:0;background:#101318;color:#f4f7fb;font-family:system-ui,Segoe UI,sans-serif}main{max-width:520px;margin:0 auto;padding:18px}");
  page += F("h1{font-size:24px;margin:8px 0 14px}.panel{border:1px solid #283241;border-radius:8px;padding:14px;margin:14px 0;background:#171d25}");
  page += F("label{display:block;margin:12px 0 6px;color:#aab5c2}input,select{width:100%;box-sizing:border-box;border:1px solid #3a4656;border-radius:8px;background:#0e131a;color:#fff;padding:12px;font-size:16px}");
  page += F("button{width:100%;border:0;border-radius:8px;background:#f2a51a;color:#111;padding:14px;font-size:17px;font-weight:700;margin-top:16px}.secondary{background:#2d3949;color:#fff}.muted{color:#9aa7b4;font-size:14px;line-height:1.45}</style></head><body><main>");
  page += F("<h1>ESP32 WiFi Setup</h1><section class='panel'><div class='muted'>Status: ");
  page += htmlEscape(statusLine);
  page += F("</div><div class='muted'>STA IP: ");
  page += WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("offline");
  page += F("</div><div class='muted'>AP IP: 192.168.4.1</div><button id='scanBtn' class='secondary' type='button' onclick='startScan()'>Refresh Network List</button><div id='scanHint' class='muted'></div>");
  page += F("</div></section><section class='panel'><div class='muted' id='diagnostics'>");
  page += makeWiFiDiagnostics();
  page += F("</div></section><form method='post' action='/save' class='panel'>");
  page += F("<label>WiFi security type</label><select id='joinType' name='join_type' onchange='updateFields()'>");
  page += F("<option value='0'");
  if (currentJoinType == WIFI_JOIN_OPEN) page += F(" selected");
  page += F(">Open network, no password</option><option value='1'");
  if (currentJoinType == WIFI_JOIN_PERSONAL) page += F(" selected");
  page += F(">WPA/WPA2/WPA3 Personal, SSID + password</option><option value='2'");
  if (currentJoinType == WIFI_JOIN_PORTAL) page += F(" selected");
  page += F(">Captive Portal / Web login, no WiFi password</option><option value='3'");
  if (currentJoinType == WIFI_JOIN_ENTERPRISE) page += F(" selected");
  page += F(">WPA2-Enterprise 802.1X, username + password</option></select><p class='muted' id='typeHelp'></p>");
  page += F("<div id='networkWrap'><label>Network</label><select id='ssidList' name='picked_ssid' onchange='onNetworkChange()'>");
  page += makeNetworkOptions();
  page += F("</select></div><div id='hiddenToggleWrap'><label><input id='hiddenToggle' name='hidden_network' type='checkbox' value='1' onchange='onHiddenToggle()'");
  if (currentIsHiddenPersonal) page += F(" checked");
  page += F("> Hidden network</label></div><div id='hiddenSsidWrap'><label>Hidden SSID</label><input id='hiddenSsidInput' name='hidden_ssid' value=\"");
  if (currentIsHiddenPersonal) page += htmlEscape(wifiConfig.ssid);
  page += F("\" placeholder='Enter hidden network SSID'></div>");
  page += F("<div id='usernameWrap'><label>Username / Identity</label><input id='usernameInput' name='username' value=\"");
  page += htmlEscape(wifiConfig.username);
  page += F("\"></div><div id='passwordWrap'><label id='passwordLabel'>Password</label><input id='passwordInput' name='password' type='password' value=\"");
  page += htmlEscape(wifiConfig.password);
  page += F("\"></div><button type='submit'>Save and Connect ESP32</button></form>");
  page += F("<section class='panel'><a href='/portalcheck'><button class='secondary'>Check Upstream Portal</button></a>");
  if (portalRedirectUrl.length()) {
    page += F("<a href=\"");
    page += htmlEscape(portalRedirectUrl);
    page += F("\"><button type='button'>Open Login Page</button></a><p class='muted'>Detected portal: ");
    page += htmlEscape(portalRedirectUrl);
    page += F("</p>");
  } else if (portalProbeStatus.length()) {
    page += F("<p class='muted'>Portal status: ");
    page += htmlEscape(portalProbeStatus);
    page += F("</p>");
  }
  page += F("<p class='muted'>Portal forwarding needs NAT/router support. This build can detect and show the login URL, but cannot transparently proxy HTTPS portal pages.</p></section>");
  page += F("<section class='panel'><p class='muted'>Phone should auto-open this page after joining AP Jianjian-Setup / 12345678. If it does not, open 192.168.4.1 manually.</p></section><script>function updateFields(){var t=document.getElementById('joinType').value,s=document.getElementById('ssidList'),nw=document.getElementById('networkWrap'),ht=document.getElementById('hiddenToggle'),htw=document.getElementById('hiddenToggleWrap'),hw=document.getElementById('hiddenSsidWrap'),hi=document.getElementById('hiddenSsidInput'),u=document.getElementById('usernameWrap'),p=document.getElementById('passwordWrap'),pi=document.getElementById('passwordInput'),ui=document.getElementById('usernameInput'),h=document.getElementById('typeHelp'),pl=document.getElementById('passwordLabel'),hidden=t==='1'&&ht&&ht.checked;if(htw)htw.style.display=(t==='1')?'block':'none';if(nw)nw.style.display=hidden?'none':'block';if(hw)hw.style.display=hidden?'block':'none';if(hi)hi.required=hidden;u.style.display=(t==='3')?'block':'none';p.style.display=(t==='0'||t==='2')?'none':'block';ui.required=(t==='3');pi.required=(t==='1'||t==='3');pl.textContent='Password';if(t==='2'&&pi)pi.value='';h.textContent=t==='0'?'Open WiFi: choose a network, no password needed.':t==='1'?'Personal WiFi: choose SSID and password, or check Hidden network to type SSID.':t==='2'?'Captive portal: choose SSID, then use the detected portal link below.':'Enterprise 802.1X: choose SSID, then enter identity/username and password.';}function clearCredentialFields(){var ui=document.getElementById('usernameInput'),pi=document.getElementById('passwordInput');if(ui)ui.value='';if(pi)pi.value='';}function onNetworkChange(){clearCredentialFields();updateFields();}function onHiddenToggle(){clearCredentialFields();updateFields();}function refreshNetworks(){var s=document.getElementById('ssidList'),keep=s?s.value:'';return fetch('/networks').then(function(r){return r.text();}).then(function(t){if(!s)return;s.innerHTML=t;if(keep){var found=false;for(var i=0;i<s.options.length;i++){if(s.options[i].value===keep){found=true;break;}}if(found)s.value=keep;}updateFields();});}function startScan(){var b=document.getElementById('scanBtn'),h=document.getElementById('scanHint');b.disabled=true;b.textContent='Scanning...';h.textContent='Looking for nearby 2.4 GHz WiFi...';fetch('/scan').then(function(){return refreshNetworks();}).then(function(){setTimeout(function(){b.disabled=false;b.textContent='Refresh Network List';},600);}).catch(function(){b.disabled=false;b.textContent='Refresh Network List';h.textContent='Scan request failed. Try again.';});}updateFields();setInterval(function(){fetch('/diagnostics').then(function(r){return r.text();}).then(function(t){var d=document.getElementById('diagnostics');if(d)d.innerHTML=t;});refreshNetworks();},1800);</script></main></body></html>");
  return page;
}

void handleSetupRoot()
{
  server.send(200, "text/html; charset=utf-8", makeSetupPage());
}

void handleSetupDiagnostics()
{
  server.send(200, "text/html; charset=utf-8", makeWiFiDiagnostics());
}

void handleSetupNetworks()
{
  server.send(200, "text/html; charset=utf-8", makeNetworkOptions());
}

void handleSetupSave()
{
  String pickedSsid = server.hasArg("picked_ssid") ? server.arg("picked_ssid") : "";
  String hiddenSsid = server.hasArg("hidden_ssid") ? server.arg("hidden_ssid") : "";
  pickedSsid.trim();
  hiddenSsid.trim();
  uint8_t requestedType = server.hasArg("join_type") ? (uint8_t)server.arg("join_type").toInt() : WIFI_JOIN_PERSONAL;
  if (requestedType > WIFI_JOIN_ENTERPRISE) requestedType = WIFI_JOIN_PERSONAL;
  bool hiddenPersonal = requestedType == WIFI_JOIN_PERSONAL && server.hasArg("hidden_network");
  wifiConfig.ssid = hiddenPersonal ? hiddenSsid : pickedSsid;
  wifiConfig.joinType = requestedType;
  wifiConfig.enterprise = requestedType == WIFI_JOIN_ENTERPRISE;
  wifiConfig.username = server.hasArg("username") ? server.arg("username") : "";
  wifiConfig.password = server.hasArg("password") ? server.arg("password") : "";
  if (requestedType == WIFI_JOIN_OPEN) {
    wifiConfig.username = "";
    wifiConfig.password = "";
  } else if (requestedType == WIFI_JOIN_PERSONAL) {
    wifiConfig.username = "";
  } else if (requestedType == WIFI_JOIN_PORTAL) {
    wifiConfig.username = "";
    wifiConfig.password = "";
  }
  saveConfig();
  statusLine = "Saved from AP";
  wifiUiDirty = true;
  connectWiFi(false);
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetupScan()
{
  scanNearbyNetworks();
  server.send(204);
}

void handlePortalCheck()
{
  statusLine = checkUpstreamPortal();
  server.send(200, "text/html; charset=utf-8", makeSetupPage());
}

void handleSetupClear()
{
  clearConfig();
  WiFi.disconnect(false, false);
  statusLine = "Cleared";
  server.sendHeader("Location", "/");
  server.send(303);
}

void registerConfigPortalRoutes()
{
  if (configRoutesRegistered) return;
  server.on("/", HTTP_GET, handleSetupRoot);
  server.on("/diagnostics", HTTP_GET, handleSetupDiagnostics);
  server.on("/networks", HTTP_GET, handleSetupNetworks);
  server.on("/scan", HTTP_GET, handleSetupScan);
  server.on("/portalcheck", HTTP_GET, handlePortalCheck);
  server.on("/save", HTTP_POST, handleSetupSave);
  server.on("/clear", HTTP_POST, handleSetupClear);
  server.onNotFound(handleSetupRoot);
  configRoutesRegistered = true;
}

void startConfigAp()
{
  const IPAddress currentApIP = WiFi.softAPIP();
  const bool hasAp = ((WiFi.getMode() & WIFI_AP) != 0) && currentApIP != IPAddress(0, 0, 0, 0);
  if (hasAp) {
    if (!configApRunning) addWiFiLog("AP recovered ip=" + currentApIP.toString());
    configApRunning = true;
    apIP = currentApIP;
    return;
  }

  configApRunning = false;
  if (lastApStartAttempt != 0 && millis() - lastApStartAttempt < 5000) return;
  lastApStartAttempt = millis();

  addWiFiLog("AP start attempt mode=" + String((int)WiFi.getMode()));
  if (!wifiRadioInitialized) {
    WiFi.mode(WIFI_OFF);
    delay(180);
    wifiRadioInitialized = true;
  }
  WiFi.persistent(false);
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  delay(80);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  bool apConfigOk = WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  addWiFiLog("AP config=" + String(apConfigOk ? "ok" : "fail"));
  bool apStarted = WiFi.softAP(SETUP_AP_SSID, SETUP_AP_PASS, SETUP_AP_CHANNEL, false, 4);
  if (!apStarted) {
    addWiFiLog("AP start retry");
    WiFi.mode(WIFI_OFF);
    delay(180);
    WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    delay(80);
    apConfigOk = WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    addWiFiLog("AP retry config=" + String(apConfigOk ? "ok" : "fail"));
    apStarted = WiFi.softAP(SETUP_AP_SSID, SETUP_AP_PASS, SETUP_AP_CHANNEL, false, 4);
  }

  const IPAddress startedIp = WiFi.softAPIP();
  configApRunning = apStarted && startedIp != IPAddress(0, 0, 0, 0);
  if (!configApRunning) {
    statusLine = "AP start failed";
    addWiFiLog("AP start failed");
    wifiUiDirty = true;
    return;
  }

  apIP = startedIp;
  registerConfigPortalRoutes();
  if (!configPortalStarted) {
    server.begin();
    configPortalStarted = true;
  }
  dnsServer.stop();
  dnsServer.start(DNS_PORT, "*", apIP);
  statusLine = String("AP: ") + SETUP_AP_SSID;
  addWiFiLog("AP ready ip=" + apIP.toString() + " mode=" + String((int)WiFi.getMode()) + " ch=" + String(SETUP_AP_CHANNEL));
  wifiUiDirty = true;
}
void refreshWiFiSettingsIfNeeded()
{
  if (mode != ScreenMode::WiFiSettings) return;
  if (!wifiUiDirty) return;
  if (millis() - lastWiFiUiRefresh < 150) return;
  lastWiFiUiRefresh = millis();
  wifiUiDirty = false;
  drawWiFiSettings();
}

void handleWiFiSelect()
{
  if (pendingDeleteWifi >= 0) {
    deleteSavedWiFi(pendingDeleteWifi);
    statusLine = "WiFi deleted";
    drawWiFiSettings();
    return;
  }

  if (wifiInstructionOpen) {
    wifiInstructionOpen = false;
    wifiSelectedIndex = 2;
    wifiTopIndex = 0;
    drawWiFiSettings();
    return;
  }

  if (!wifiListOpen) {
    if (wifiSelectedIndex == 0) {
      statusLine = String("AP: ") + SETUP_AP_SSID;
      drawWiFiSettings();
      return;
    }

    if (wifiSelectedIndex == 1) {
      wifiListOpen = true;
      wifiSelectedIndex = 0;
      wifiTopIndex = 0;
      pendingDeleteWifi = -1;
      drawWiFiSettings();
      return;
    }

    if (wifiSelectedIndex == 2) {
      wifiInstructionOpen = true;
      wifiSelectedIndex = 0;
      wifiTopIndex = 0;
      drawWiFiSettings();
      return;
    }

    mode = ScreenMode::MainMenu;
    pendingDeleteWifi = -1;
    drawMainMenu();
    return;
  }

  if (wifiIndexIsSaved(wifiSelectedIndex)) {
    wifiConfig = savedWiFis[wifiSavedIndex(wifiSelectedIndex)];
    statusLine = "Connecting saved";
    connectWiFi(true);
    return;
  }

  wifiListOpen = false;
  wifiSelectedIndex = 1;
  wifiTopIndex = 0;
  pendingDeleteWifi = -1;
  drawWiFiSettings();
}

int vaultItems() { if (vaultPage == VaultPage::Menu) return 5; if (vaultPage == VaultPage::PairPrompt) return 2; if (vaultPage == VaultPage::SafeMenu) return safeCodeSet ? 3 : 2; if (vaultPage == VaultPage::Field) return 4; return vaultCount + 1; }
String vaultLabel(int i) {
  if (vaultPage == VaultPage::Menu) { if(i==0)return "Desktop Pairing"; if(i==1)return "Type Saved Credential"; if(i==2)return toggleImeForTyping?"Input Mode: Toggle IME":"Input Mode: Direct ASCII"; if(i==3)return "Safe Code"; return "Back"; }
  if (vaultPage == VaultPage::PairPrompt) return i==0?"Yes, connect":"No, deny";
  if (vaultPage == VaultPage::SafeMenu) { if(i==0)return safeCodeSet?"Reset Safe Code":"Set Safe Code"; if(safeCodeSet&&i==1)return "Remove Safe Code"; return "Back"; }
  if (vaultPage == VaultPage::Field) { if(i==0)return "Type Username"; if(i==1)return "Type Password"; if(i==2)return "Type Username + Password"; return "Back"; }
  return i<vaultCount?vaultEntries[i].site:"Back";
}
void drawVaultRow(int index, bool selected) {
  int row=index-vaultTopIndex;
  if(row<0||row>=VAULT_VISIBLE_ROWS)return;
  int y=54+row*23;
  uint16_t bg=selected?uiSelect():uiBg();
  tft.fillRect(8,y-10,304,20,bg);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(selected?uiHeaderText():uiText(),bg);
  tft.drawString(vaultLabel(index),20,y,2);
}
void drawVaultRows() {
  tft.fillRect(0,32,SCREEN_W,118,uiBg());
  int count=vaultItems();
  if(vaultSelectedIndex<0)vaultSelectedIndex=0;
  if(vaultSelectedIndex>=count)vaultSelectedIndex=count-1;
  if(vaultSelectedIndex<vaultTopIndex)vaultTopIndex=vaultSelectedIndex;
  if(vaultSelectedIndex>=vaultTopIndex+VAULT_VISIBLE_ROWS)vaultTopIndex=vaultSelectedIndex-VAULT_VISIBLE_ROWS+1;
  int last=min(count,vaultTopIndex+VAULT_VISIBLE_ROWS);
  for(int i=vaultTopIndex;i<last;i++)drawVaultRow(i,i==vaultSelectedIndex);
  if(vaultTopIndex>0)tft.drawString("^",304,38,2);
  if(last<count)tft.drawString("v",304,142,2);
}
void updateVaultSelection(int oldIndex,int newIndex) {
  drawVaultRow(oldIndex,false);
  drawVaultRow(newIndex,true);
}
void drawPasswordVault(){ tft.fillScreen(uiBg()); drawHeader(vaultPage==VaultPage::PairPrompt?"Desktop Pairing":(vaultPage==VaultPage::SafeMenu?"Safe Code":"Password Vault")); drawVaultRows(); drawFooter(vaultPage==VaultPage::PairPrompt?"Press: confirm  Hold: deny":"Press: select  Hold: back"); }
void drawSafeDial(bool entering){
  int cx=160,cy=88,r=55;
  if(entering||safeRenderedDialValue<0){
    tft.fillScreen(uiBg());
    drawHeader(safeSettingMode?"Set Safe Code":"Please input password");
    tft.drawCircle(cx,cy,r,uiMint());
    for(int i=0;i<100;i++){float a=(-90+i*3.6f)*DEG_TO_RAD;int in=r-(i%10?4:8);tft.drawLine(cx+cosf(a)*in,cy+sinf(a)*in,cx+cosf(a)*r,cy+sinf(a)*r,uiMuted());}
  }else{
    float oldAngle=(-90+safeRenderedDialValue*3.6f)*DEG_TO_RAD;
    tft.fillCircle(cx+cosf(oldAngle)*(r-12),cy+sinf(oldAngle)*(r-12),4,uiBg());
  }
  tft.fillCircle(cx,cy,31,uiBg());
  float angle=(-90+safeDialValue*3.6f)*DEG_TO_RAD;
  tft.fillCircle(cx+cosf(angle)*(r-12),cy+sinf(angle)*(r-12),4,uiHeader());
  char b[3];snprintf(b,3,"%02d",safeDialValue);
  tft.setTextDatum(MC_DATUM);tft.setTextColor(uiText(),uiBg());tft.drawString(b,cx,cy,6);
  safeRenderedDialValue=safeDialValue;
  drawFooter(safeEnteredCount==4?"Press confirm | Double retry | Hold exit":(safeSettingMode?"Set 4 numbers | Hold exit":"Please input password | Hold exit"));
}
void startSafe(bool set){safeSettingMode=set;safeDialValue=0;safeRenderedDialValue=-1;safeEnteredCount=0;safeLastMoveAt=0;safeConfirmAt=0;vaultPage=VaultPage::SafeDial;drawSafeDial(true);}
void finishSafeConfirm(){bool ok=true;for(int i=0;i<4;i++)if(safeEnteredValues[i]!=safeCodeValues[i]||safeEnteredDirections[i]!=safeCodeDirections[i])ok=false;if(safeSettingMode){for(int i=0;i<4;i++){safeCodeValues[i]=safeEnteredValues[i];safeCodeDirections[i]=safeEnteredDirections[i];}safeCodeSet=true;saveVault();vaultPage=VaultPage::SafeMenu;drawPasswordVault();}else if(ok){vaultPage=VaultPage::Menu;drawPasswordVault();}else{safeEnteredCount=0;safeDialValue=0;safeConfirmAt=0;drawSafeDial();drawFooter(safeSettingMode?"Format error - retry":"Password incorrect");}}
void handleVaultRotate(int d){
  if(vaultPage==VaultPage::SafeDial){safeDialValue=(safeDialValue+d+100)%100;safeDialDirection=d>0?1:-1;safeLastMoveAt=millis();safeConfirmAt=0;drawSafeDial();return;}
  bool cancelledDelete=pendingDeleteVault>=0;
  pendingDeleteVault=-1;
  int oldIndex=vaultSelectedIndex;
  int oldTopIndex=vaultTopIndex;
  int n=vaultItems();vaultSelectedIndex=(vaultSelectedIndex+d+n)%n;
  if(vaultSelectedIndex<vaultTopIndex)vaultTopIndex=vaultSelectedIndex;
  if(vaultSelectedIndex>=vaultTopIndex+VAULT_VISIBLE_ROWS)vaultTopIndex=vaultSelectedIndex-VAULT_VISIBLE_ROWS+1;
  if(vaultTopIndex!=oldTopIndex)drawVaultRows();
  else updateVaultSelection(oldIndex,vaultSelectedIndex);
  if(cancelledDelete)drawFooter(vaultPage==VaultPage::PairPrompt?"Press: confirm  Hold: deny":"Press: select  Hold: back");
}
void handleVaultPress(){
  if(vaultPage==VaultPage::PairPrompt){
    if(vaultSelectedIndex==0){vaultPairToken=String(static_cast<uint32_t>(esp_random()),HEX);vaultPairExpiresAt=0;Serial.printf("#PAIR_OK|%s\n",vaultPairToken.c_str());}
    else{vaultPairToken="";vaultPairExpiresAt=0;Serial.println("#PAIR_DENY");}
    vaultPage=VaultPage::Menu;vaultSelectedIndex=0;vaultTopIndex=0;drawPasswordVault();return;
  }
  if(vaultPage==VaultPage::SafeDial){
    if(safeEnteredCount<4){drawFooter(safeSettingMode?"Format error - retry":"Password incorrect");return;}
    if(safeConfirmAt&&millis()-safeConfirmAt<SAFE_DOUBLE_MS){safeEnteredCount=0;safeDialValue=0;safeConfirmAt=0;drawSafeDial();}
    else{safeConfirmAt=millis();drawFooter("Confirming... double click to retry");}
    return;
  }
  if(vaultPage==VaultPage::Menu){
    if(vaultSelectedIndex==1){vaultPage=VaultPage::List;vaultSelectedIndex=0;vaultTopIndex=0;}
    else if(vaultSelectedIndex==2){toggleImeForTyping=!toggleImeForTyping;saveVault();}
    else if(vaultSelectedIndex==3){vaultPage=VaultPage::SafeMenu;vaultSelectedIndex=0;vaultTopIndex=0;}
    else if(vaultSelectedIndex==4){mode=ScreenMode::MainMenu;drawMainMenu();return;}
    drawPasswordVault();return;
  }
  if(vaultPage==VaultPage::SafeMenu){
    if(vaultSelectedIndex==0){startSafe(true);return;}
    if(safeCodeSet&&vaultSelectedIndex==1){safeCodeSet=false;saveVault();}
    else{vaultPage=VaultPage::Menu;vaultSelectedIndex=0;vaultTopIndex=0;}
    drawPasswordVault();return;
  }
  if(vaultPage==VaultPage::List){
    if(pendingDeleteVault>=0){
      deleteVaultEntry(pendingDeleteVault);
      vaultSelectedIndex=min(vaultSelectedIndex,vaultCount);
      drawPasswordVault();
      drawFooter("Deleted");
      return;
    }
    if(vaultSelectedIndex<vaultCount){vaultActiveEntry=vaultSelectedIndex;vaultPage=VaultPage::Field;vaultSelectedIndex=0;vaultTopIndex=0;}
    else{vaultPage=VaultPage::Menu;vaultSelectedIndex=0;vaultTopIndex=0;}
    drawPasswordVault();return;
  }
  if(vaultPage==VaultPage::Field){
    if(vaultSelectedIndex==3){vaultPage=VaultPage::List;vaultSelectedIndex=0;vaultTopIndex=0;drawPasswordVault();return;}
    if(vaultActiveEntry<0||vaultActiveEntry>=vaultCount){vaultPage=VaultPage::List;vaultSelectedIndex=0;vaultTopIndex=0;drawPasswordVault();return;}
    if(vaultSelectedIndex==2){
      typeCredentialText(vaultEntries[vaultActiveEntry].username);
      keyboard.write(KEY_TAB);
      delay(80);
      typeCredentialText(vaultEntries[vaultActiveEntry].password);
    } else if(vaultSelectedIndex==0) {
      typeCredentialText(vaultEntries[vaultActiveEntry].username);
    } else {
      typeCredentialText(vaultEntries[vaultActiveEntry].password);
    }
    drawPasswordVault();
    drawFooter("Typed to computer");
  }
}
void updateSafe(){if(vaultPage!=VaultPage::SafeDial)return;if(safeEnteredCount<4&&safeLastMoveAt&&millis()-safeLastMoveAt>SAFE_DWELL_MS){safeEnteredValues[safeEnteredCount]=safeDialValue;safeEnteredDirections[safeEnteredCount++]=safeDialDirection;safeLastMoveAt=0;drawSafeDial();}if(safeConfirmAt&&millis()-safeConfirmAt>=SAFE_DOUBLE_MS){safeConfirmAt=0;finishSafeConfirm();}}
void handleRotate(int direction)
{
  if (mode == ScreenMode::MainMenu) moveMainSelection(direction);
  else if (mode == ScreenMode::AnswerBook) moveAnswer(direction);
  else if (mode == ScreenMode::PasswordVault) handleVaultRotate(direction);
  else if (mode == ScreenMode::WiFiSettings) moveWiFiSelection(direction);
}

void shuffleAnswer()
{
  int target = esp_random() % ANSWER_COUNT;
  int currentDelay = 22;
  int steps = 26 + (esp_random() % 16);

  for (int i = 0; i < steps; i++) {
    answerIndex = (answerIndex + 1 + (esp_random() % 9)) % ANSWER_COUNT;
    drawAnswerContent();
    delay(currentDelay);
    if (i > 7) currentDelay += 7;
    if (i > 16) currentDelay += 12;
  }

  answerIndex = target;
  drawAnswerContent();
}

void handleShortPress()
{
  if (mode == ScreenMode::MainMenu) {
    if (selectedIndex == 0) mode = ScreenMode::CO2Sensor;
    else if (selectedIndex == 1) { mode = ScreenMode::PasswordVault; vaultSelectedIndex = 0; vaultTopIndex = 0; if (safeCodeSet) startSafe(false); else vaultPage = VaultPage::Menu; }
    else if (selectedIndex == 2) mode = ScreenMode::AnswerBook;
    else if (selectedIndex == 3) { mode = ScreenMode::MarketTicker; drawCurrentScreen(); refreshAllMarkets(); return; }
    else if (selectedIndex == 4) { mode = ScreenMode::WiFiSettings; wifiListOpen = false; wifiInstructionOpen = false; wifiSelectedIndex = 0; wifiTopIndex = 0; pendingDeleteWifi = -1; }
    else mode = ScreenMode::About;
    drawCurrentScreen();
    return;
  }
  if (mode == ScreenMode::AnswerBook) shuffleAnswer();
  else if (mode == ScreenMode::MarketTicker) refreshAllMarkets();
  else if (mode == ScreenMode::CO2Sensor) { co2ByteCount=0;co2FrameCount=0;co2Ppm=-1;co2RawFrame="none";co2Status="counter reset";jw01BufferLen=0;drawCo2Values(); }
  else if (mode == ScreenMode::PasswordVault) handleVaultPress();
  else if (mode == ScreenMode::WiFiSettings) handleWiFiSelect();
}
void handleLongPress()
{
  if (mode == ScreenMode::PasswordVault && vaultPage == VaultPage::PairPrompt) {
    vaultPairToken = "";
    vaultPairExpiresAt = 0;
    Serial.println("#PAIR_DENY");
    vaultPage = VaultPage::Menu;
    vaultSelectedIndex = 0;
    vaultTopIndex = 0;
    drawPasswordVault();
    return;
  }
  if (mode == ScreenMode::PasswordVault && vaultPage == VaultPage::List) {
    if (pendingDeleteVault >= 0) {
      pendingDeleteVault = -1;
      drawPasswordVault();
      drawFooter("Delete cancelled");
      return;
    }
    if (vaultSelectedIndex < vaultCount) {
      pendingDeleteVault = vaultSelectedIndex;
      drawPasswordVault();
      drawFooter("Press: delete | Hold: cancel");
      return;
    }
  }
  if (mode == ScreenMode::PasswordVault && vaultPage == VaultPage::SafeDial) {
    safeConfirmAt = 0;
    safeLastMoveAt = 0;
    safeEnteredCount = 0;
    safeDialValue = 0;
    safeRenderedDialValue = -1;
    vaultSelectedIndex = 0;
    vaultTopIndex = 0;
    if (safeSettingMode) {
      vaultPage = VaultPage::SafeMenu;
      drawPasswordVault();
    } else {
      vaultPage = VaultPage::Menu;
      mode = ScreenMode::MainMenu;
      drawMainMenu();
    }
    return;
  }
  if (mode == ScreenMode::WiFiSettings) {
    if (wifiInstructionOpen) {
      wifiInstructionOpen = false;
      wifiSelectedIndex = 2;
      wifiTopIndex = 0;
      drawWiFiSettings();
      return;
    }

    if (pendingDeleteWifi >= 0) {
      pendingDeleteWifi = -1;
      statusLine = "Delete cancelled";
      drawWiFiSettings();
      return;
    }

    if (wifiListOpen && wifiIndexIsSaved(wifiSelectedIndex)) {
      pendingDeleteWifi = wifiSavedIndex(wifiSelectedIndex);
      statusLine = "Confirm delete";
      drawWiFiSettings();
      return;
    }

    if (wifiListOpen) {
      wifiListOpen = false;
      wifiSelectedIndex = 1;
      wifiTopIndex = 0;
      drawWiFiSettings();
      return;
    }
  }

  if (mode != ScreenMode::MainMenu) {
    pendingDeleteWifi = -1;
    wifiListOpen = false;
    mode = ScreenMode::MainMenu;
    drawMainMenu();
  }
}

void readEncoder()
{
  uint8_t newEncoderState = (digitalRead(ENCODER_CLK_PIN) << 1) | digitalRead(ENCODER_DT_PIN);
  if (newEncoderState != encoderState) {
    uint8_t transition = (encoderState << 2) | newEncoderState;

    if (transition == 0b0001 || transition == 0b0111 ||
        transition == 0b1110 || transition == 0b1000) {
      encoderAccumulator++;
    } else if (transition == 0b0010 || transition == 0b1011 ||
               transition == 0b1101 || transition == 0b0100) {
      encoderAccumulator--;
    } else {
      encoderAccumulator = 0;
    }

    encoderState = newEncoderState;

    if (encoderAccumulator >= ENCODER_TRANSITIONS_PER_STEP) {
      encoderAccumulator = 0;
      handleRotate(-1);
    } else if (encoderAccumulator <= -ENCODER_TRANSITIONS_PER_STEP) {
      encoderAccumulator = 0;
      handleRotate(1);
    }
  }
}

void readButton()
{
  bool button = digitalRead(ENCODER_SW_PIN);
  uint32_t now = millis();

  if (button != lastButton && now - lastButtonChange > DEBOUNCE_MS) {
    lastButtonChange = now;
    lastButton = button;

    if (button == LOW) {
      buttonPressing = true;
      longPressHandled = false;
      buttonDownAt = now;
    } else if (buttonPressing) {
      buttonPressing = false;
      if (!longPressHandled) handleShortPress();
    }
  }

  if (buttonPressing && !longPressHandled && now - buttonDownAt > LONG_PRESS_MS) {
    longPressHandled = true;
    handleLongPress();
  }
}

void readSerialControls()
{
  while (Serial.available()) {
    char c = Serial.read();
    if (serialCommandBuffer.length() > 0 || c == '#') {
      if (c == '\r') continue;
      if (c == '\n') {
        String command = serialCommandBuffer;
        serialCommandBuffer = "";
        if (command == "#PING") {
          Serial.println("#READY|JJ1");
        } else if (command == "#APSTATUS") {
          Serial.printf("#AP|started=%d|ip=%s|stations=%d\n", configApRunning, WiFi.softAPIP().toString().c_str(), WiFi.softAPgetStationNum());
        } else if (command == "#PAIR") {
          vaultPairToken = "";
          vaultPairExpiresAt = 0;
          mode = ScreenMode::PasswordVault;
          vaultPage = VaultPage::PairPrompt;
          vaultSelectedIndex = 0;
          vaultTopIndex = 0;
          drawPasswordVault();
          Serial.println("#PAIR_PROMPT");
        } else if (command.startsWith("#SAVE|")) {
          String fields[4];
          int start = 6;
          bool parsed = true;
          for (int field = 0; field < 4; field++) {
            int separator = command.indexOf('|', start);
            if (field == 3) fields[field] = command.substring(start);
            else if (separator >= 0) { fields[field] = command.substring(start, separator); start = separator + 1; }
            else { parsed = false; break; }
          }
          if (!parsed || vaultPairToken.length() == 0 || fields[0] != vaultPairToken || (vaultPairExpiresAt != 0 && static_cast<int32_t>(millis() - vaultPairExpiresAt) > 0)) {
            Serial.println("#ERROR|Pair again before saving");
          } else {
            String site, username, password;
            if (!decodeBase64(fields[1], site) || !decodeBase64(fields[2], username) || !decodeBase64(fields[3], password) || site.length() == 0) {
              Serial.println("#ERROR|Invalid credential data");
            } else {
              upsertVaultEntry(site, username, password);
              Serial.println("#SAVED");
            }
          }
        } else if (command == "#CLOSE") {
          vaultPairToken = "";
          vaultPairExpiresAt = 0;
        }
      } else if (serialCommandBuffer.length() < 480) {
        serialCommandBuffer += c;
      } else {
        serialCommandBuffer = "";
      }
      continue;
    }
    if (c == 'u' || c == 'U' || c == 'l' || c == 'L') handleRotate(-1);
    else if (c == 'd' || c == 'D' || c == 'r' || c == 'R') handleRotate(1);
    else if (c == ' ' || c == '\r' || c == '\n' || c == 's' || c == 'S') handleShortPress();
    else if (c == 'b' || c == 'B' || c == 8 || c == 127) handleLongPress();
  }
}

void updateMarketAnimation()
{
  if (mode != ScreenMode::MarketTicker) return;
  if (millis() - lastMarketFrame < 2600) return;
  lastMarketFrame = millis();

  MarketIndex first = markets[0];
  for (int i = 0; i < MARKET_COUNT - 1; i++) markets[i] = markets[i + 1];
  markets[MARKET_COUNT - 1] = first;
  drawMarketRows();
}

void updateMarqueeAnimation()
{
  if (millis() - lastMarqueeFrame < 420) return;

  if (mode == ScreenMode::MainMenu) {
    String text = mainMenu[selectedIndex];
    int width = SCREEN_W - 76;
    if (!textNeedsMarquee(text, width)) return;
    lastMarqueeFrame = millis();
    drawMarqueeText(text, 44, 42 + selectedIndex * 24, width, uiHeaderText(), uiSelect(), true);
    return;
  }

  if (mode != ScreenMode::WiFiSettings) return;

  if (wifiInstructionOpen) {
    static const char *helpLines[] = {
      "1. Join Jianjian-Setup",
      "2. Password 12345678",
      "3. Open 192.168.4.1",
      "4. Pick WiFi and save",
      "Enterprise: username + password",
      "Portal: choose SSID then web login"
    };
    for (int i = 0; i < 6; i++) {
      String text = helpLines[i];
      int width = 148;
      if (textNeedsMarquee(text, width)) {
        lastMarqueeFrame = millis();
        drawMarqueeText(text, 8, 42 + i * 20 + (i >= 4 ? 4 : 0), width, i >= 4 ? uiText() : uiMuted(), uiBg(), true);
        return;
      }
    }
    return;
  }

  if (!wifiListOpen) {
    String label = wifiItemLabel(wifiSelectedIndex);
    int width = 96;
    if (textNeedsMarquee(label, width)) {
      lastMarqueeFrame = millis();
      int row = wifiSelectedIndex - wifiTopIndex;
      int y = 42 + row * 23;
      drawMarqueeText(label, 194, y, width, uiHeaderText(), uiSelect(), true);
      return;
    }

    String current = "Current: " + (wifiConfig.ssid.length() ? wifiConfig.ssid : String("(not set)"));
    width = 146;
    if (textNeedsMarquee(current, width)) {
      lastMarqueeFrame = millis();
      drawMarqueeText(current, 8, 108, width, uiText(), uiBg(), true);
    }
    return;
  }

  String label = wifiItemLabel(wifiSelectedIndex);
  int width = 96;
  if (!textNeedsMarquee(label, width)) return;
  lastMarqueeFrame = millis();
  int row = wifiSelectedIndex - wifiTopIndex;
  int y = 42 + row * 23;
  drawMarqueeText(label, 194, y, width, uiHeaderText(), uiSelect(), true);
}

void updateCo2DogAnimation()
{
  if (mode != ScreenMode::CO2Sensor) return;
  if (millis() - lastDogFrame < 360) return;
  lastDogFrame = millis();
  drawCo2DogAvatar();
}

void setup()
{
  Serial.begin(115200);
  keyboard.begin();
  USB.begin();
  delay(300);

  pinMode(POWER_ON_PIN, OUTPUT);
  digitalWrite(POWER_ON_PIN, HIGH);
  delay(100);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  pinMode(ENCODER_CLK_PIN, INPUT_PULLUP);
  pinMode(ENCODER_DT_PIN, INPUT_PULLUP);
  pinMode(ENCODER_SW_PIN, INPUT_PULLUP);
  encoderState = (digitalRead(ENCODER_CLK_PIN) << 1) | digitalRead(ENCODER_DT_PIN);
  lastButton = digitalRead(ENCODER_SW_PIN);
  setJw01Baud(0);

  tft.init();
  tft.setRotation(3);
  loadConfig();
  loadVault();
  startConfigAp();
  drawMainMenu();

  if (savedWiFiCount > 0) {
    autoWiFiBaseIndex = findSavedWiFiIndex(wifiConfig.ssid);
    if (autoWiFiBaseIndex < 0) autoWiFiBaseIndex = 0;
    autoWiFiAttempt = 0;
    wifiConfig = savedWiFis[autoWiFiBaseIndex];
    autoWiFiPending = false;
  } else if (wifiConfig.ssid.length() > 0) {
    autoWiFiPending = false;
  }
  autoWiFiReadyAt = millis() + 12000;
  statusLine = String("AP: ") + SETUP_AP_SSID;
}

void loop()
{
  startConfigAp();
  if (autoWiFiPending && !connectingWiFi && millis() >= autoWiFiReadyAt) {
    autoWiFiPending = false;
    statusLine = "Auto WiFi";
    connectWiFi(false, true);
  }
  dnsServer.processNextRequest();
  server.handleClient();
  if (portalCheckPending && millis() >= portalCheckReadyAt) {
    portalCheckPending = false;
    statusLine = checkUpstreamPortal();
    wifiUiDirty = true;
  }
  updateWiFiScan();
  readEncoder();
  readButton();
  readSerialControls();
  updateWiFiConnect();
  updateSafe();
  updateMarketAnimation();
  updateMarqueeAnimation();
  updateCo2DogAnimation();
  refreshOneMarket(false);
  updateJw01();
  refreshWiFiSettingsIfNeeded();

  if (redrawRequested) {
    redrawRequested = false;
    drawCurrentScreen();
  }

  if (millis() - lastDraw > 2000) {
    lastDraw = millis();
    Serial.printf("mode=%d menu=%d wifi=%d ip=%s ssid=%s\n",
                  static_cast<int>(mode),
                  selectedIndex,
                  WiFi.status(),
                  WiFi.localIP().toString().c_str(),
                  wifiConfig.ssid.c_str());
  }
}
