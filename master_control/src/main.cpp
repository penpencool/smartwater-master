#include <Arduino.h>
#include <WebServer.h>

#include "system_pins.h"
#include "system_state.h"
#include "config_manager.h"
#include "hardware_controller.h"
#include "system_network.h"
#include "automation_engine.h"
#include "web_api_handler.h"

// ==========================================
// 1. GLOBAL STATE DEFINITIONS
// ==========================================
SemaphoreHandle_t stateMutex = NULL;

ErrorCode currentError = ERR_NONE;
String activeTaskName = "";

bool stateBorehole = false;
bool stateFilterPump = false;
bool stateMainA = false;
bool stateMainB = false;
bool stateSV1 = false;
bool stateSV2 = false;
bool stateSV3 = false;
bool stateSV4 = false;

PoolNodePayload pool1Data = {1, false, 0.0f, 0};
unsigned long lastNode1Time = 0;
String lastNode1TimeStr = "";
bool simPool1Active = false;
bool simPool1Low = false;

PoolNodePayload pool2Data = {2, false, 0.0f, 0};
unsigned long lastNode2Time = 0;
String lastNode2TimeStr = "";
bool simPool2Active = false;
bool simPool2Low = false;

TankNodePayload tankData = {3, 0.0f, 0.0f, false, 0.0f, 0.0f, 0.0f, 0};
unsigned long lastNode3Time = 0;
String lastNode3TimeStr = "";
bool simTankActive = false;
float simTankLevel = 0.0f;

SolarNodePayload solarData = {4, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0};
unsigned long lastNode4Time = 0;
String lastNode4TimeStr = "";

uint8_t currentGardenZone = 0;
unsigned long gardenTaskStartTime = 0;
unsigned long gardenTaskDurationMs = 0;
int lastRunMinuteZ1 = -1;
int lastRunMinuteZ2 = -1;

uint8_t currentPoolTaskZone = 0;
unsigned long poolTaskStartTime = 0;
unsigned long poolTaskDurationMs = 0;

TaskQueueItem taskQueue[MAX_QUEUE_SIZE];
uint8_t taskQueueCount = 0;

bool isManualPoolTask = false;
unsigned long poolLowStartTimeWave = 0;
unsigned long poolLowStartTimePlay = 0;

unsigned long filterPumpStartTime = 0;
bool flowWatchdogActive = false;

bool ntpSynced = false;

// Static member definitions
unsigned long SystemNetwork::apStartTime = 0;
bool SystemNetwork::apActive = true;
unsigned long SystemNetwork::lastNtpAttempt = 0;

unsigned long AutomationEngine::lastBlinkTime = 0;
int AutomationEngine::currentBlinkStep = 0;
unsigned long AutomationEngine::lastHeartbeatTime = 0;
bool AutomationEngine::heartbeatState = false;

// Server & Configuration Instances
WebServer server(80);
ConfigManager configManager;

// ==========================================
// 2. CORE 0 TASK: Network & WebServer Task
// ==========================================
void networkTask(void *pvParameters) {
    Serial.printf("🌐 [Core %d] Network & WebServer Task running...\n", xPortGetCoreID());
    for (;;) {
        // 1. Handle Web Clients
        server.handleClient();

        // 2. Handle Network, AP & NTP Heartbeats
        SystemNetwork::handleLoop(configManager);

        // Yield time to WiFi / TCP stack on Core 0
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// ==========================================
// 3. SETUP (Initialization)
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(500);

    // 0. Initialize Thread Safety Mutex
    stateMutex = xSemaphoreCreateRecursiveMutex();

    // 1. Hardware Initialization
    HardwareController::init();

    // 2. NVS Config Initialization
    configManager.begin();

    // 3. Network & Communication Stack (WiFi AP/STA, NTP, ESP-NOW)
    SystemNetwork::init(configManager);

    // 4. Web Dashboard & REST APIs
    WebApiHandler::setupRoutes(server, configManager);
    server.begin();

    // 5. Create Core 0 Network & WebServer Task
    xTaskCreatePinnedToCore(
        networkTask,        // Task function
        "NetworkTask",      // Task name
        20480,              // Stack size (20KB for JSON & WebServer buffers + String heap ops)
        NULL,               // Parameters
        1,                  // Priority
        NULL,               // Task Handle
        0                   // Core 0 (PRO_CPU)
    );

    // Startup Indicators
    HardwareController::soundBeep(2, 80);
    digitalWrite(PIN_SYS_LED, HIGH);

    Serial.println("==========================================");
    Serial.println("🚀 Smart Water Master Controller (Dual Core) is READY!");
    Serial.printf("⚙️ Core 0: Web Server & Network | Core 1: Automation Engine\n");
    Serial.println("🌐 Web Dashboard: http://192.168.4.1 or http://smartwater.local");
    Serial.println("📡 Listening for ESP-NOW sensors...");
    Serial.println("==========================================");
}

// ==========================================
// 4. MAIN LOOP (Core 1: Real-time Engine)
// ==========================================
void loop() {
    // Handle Automated Tasks, Watchdogs, Interlocks & Schedules (Core 1)
    AutomationEngine::handleLoop(configManager);

    // 50Hz execution cycle (20ms)
    vTaskDelay(pdMS_TO_TICKS(20));
}
