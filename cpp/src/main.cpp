/**
 * @file main.cpp
 * @brief VECU-Twin Simulator — Entry Point
 *
 * Architecture:
 *
 *   Thread 1: Engine ECU     (10 Hz)
 *   Thread 2: Brake ECU      (20 Hz)
 *   Thread 3: Battery ECU    (5 Hz)
 *   Thread 4: Steering ECU   (20 Hz)
 *   Thread 5: Gateway ECU    (10 Hz)
 *   Thread 6: CAN Bus        (dispatch loop)
 *   Thread 7: State Writer   (10 Hz → SQLite JSON file)
 *   Thread 8: Command Reader (2 Hz → polls commands.json)
 *   Thread 9: Fault Detector (2 Hz → evaluates rules)
 *
 * IPC with Python backend:
 *   The simulator writes the current vehicle state, digital twin status,
 *   CAN frame log, fault events, and ECU status to JSON files in ./data/.
 *   The Python backend reads these files on every API call.
 *   Commands from the API are written to ./data/commands.json and polled here.
 *
 * This approach is the simplest possible IPC: no sockets, no shared memory,
 * no serialization libraries. It works identically on Windows and Linux.
 */

#include "Logger.hpp"
#include "CANBus.hpp"
#include "EngineECU.hpp"
#include "BrakeECU.hpp"
#include "BatteryECU.hpp"
#include "SteeringECU.hpp"
#include "GatewayECU.hpp"
#include "VehicleModel.hpp"
#include "DigitalTwin.hpp"
#include "FaultEngine.hpp"
#include "FaultDetector.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <atomic>
#include <chrono>
#include <csignal>
#include <mutex>
#include <deque>
#include <filesystem>

namespace fs = std::filesystem;
using namespace vecu;

// ─── Global shutdown flag ─────────────────────────────────────────────────────
static std::atomic<bool> g_running{true};

void signalHandler(int /*sig*/) {
    g_running.store(false);
    LOG_INFO("main", "Shutdown signal received");
}

// ─── Helpers ──────────────────────────────────────────────────────────────────
static void ensureDir(const std::string& path) {
    fs::create_directories(path);
}

static void writeFile(const std::string& path, const std::string& content) {
    // Atomic write via temp file to avoid partial reads by Python backend
    std::string tmp = path + ".tmp";
    {
        std::ofstream f(tmp);
        f << content;
    }
    fs::rename(tmp, path);
}

// ─── CAN Frame Ring Buffer ─────────────────────────────────────────────────────
static std::mutex g_can_log_mutex;
static std::deque<CANFrame> g_can_log;
static constexpr int CAN_LOG_MAX = 200;

static void canLogCallback(const CANFrame& frame) {
    std::lock_guard<std::mutex> lock(g_can_log_mutex);
    if (g_can_log.size() >= CAN_LOG_MAX) g_can_log.pop_front();
    g_can_log.push_back(frame);
}

static std::string canLogToJson() {
    std::lock_guard<std::mutex> lock(g_can_log_mutex);
    std::ostringstream ss;
    ss << "[";
    bool first = true;
    // Most recent first
    for (auto it = g_can_log.rbegin(); it != g_can_log.rend(); ++it) {
        if (!first) ss << ",";
        ss << it->toJson();
        first = false;
    }
    ss << "]";
    return ss.str();
}

// ─── Fault Events Buffer ──────────────────────────────────────────────────────
static std::mutex g_fault_mutex;
static std::deque<FaultEvent> g_faults;
static constexpr int FAULT_LOG_MAX = 100;

static void addFaultEvents(const std::vector<FaultEvent>& evs) {
    std::lock_guard<std::mutex> lock(g_fault_mutex);
    for (const auto& ev : evs) {
        if (g_faults.size() >= FAULT_LOG_MAX) g_faults.pop_front();
        g_faults.push_back(ev);
    }
}

static std::string faultEventsToJson() {
    std::lock_guard<std::mutex> lock(g_fault_mutex);
    std::ostringstream ss;
    ss << "[";
    bool first = true;
    for (auto it = g_faults.rbegin(); it != g_faults.rend(); ++it) {
        if (!first) ss << ",";
        ss << it->toJson();
        first = false;
    }
    ss << "]";
    return ss.str();
}

// ─── ECU Status JSON ──────────────────────────────────────────────────────────
static std::string ecuStatusJson(const EngineECU& engine,
                                  const BrakeECU& brake,
                                  const BatteryECU& battery,
                                  const SteeringECU& steering,
                                  const GatewayECU& gateway) {
    auto fmt = [](const VirtualECU& ecu) -> std::string {
        std::ostringstream ss;
        ss << std::fixed;
        ss << "{"
           << "\"name\":\"" << ecu.getName() << "\","
           << "\"status\":\"" << ecu.getStatusString() << "\","
           << "\"message_count\":" << ecu.getMessageCount() << ","
           << "\"messages_per_sec\":" << std::setprecision(1) << ecu.getMessagesPerSecond() << ","
           << "\"fault_count\":" << ecu.getFaultCount() << ","
           << "\"fault_active\":" << (ecu.isFaultActive() ? "true" : "false") << ","
           << "\"silenced\":" << (ecu.isSilenced() ? "true" : "false")
           << "}";
        return ss.str();
    };
    return "[" + fmt(engine) + "," + fmt(brake) + "," +
           fmt(battery) + "," + fmt(steering) + "," + fmt(gateway) + "]";
}

// ─── Command Polling ──────────────────────────────────────────────────────────
static void processCommands(const std::string& cmd_file,
                             FaultEngine& fault_engine) {
    std::ifstream f(cmd_file);
    if (!f.is_open()) return;

    std::string line;
    std::vector<std::string> pending;
    while (std::getline(f, line)) {
        if (!line.empty()) pending.push_back(line);
    }
    f.close();

    if (pending.empty()) return;

    // Clear command file after reading
    std::ofstream clear(cmd_file);
    clear << "[]";

    for (const auto& cmd : pending) {
        // Simple JSON key extraction — no JSON parser dependency
        auto extract = [&](const std::string& key) -> std::string {
            std::string search = "\"" + key + "\":\"";
            auto pos = cmd.find(search);
            if (pos == std::string::npos) return "";
            pos += search.size();
            auto end = cmd.find('"', pos);
            return cmd.substr(pos, end - pos);
        };

        std::string type    = extract("type");
        std::string value   = extract("value");

        if (type == "scenario") {
            Scenario s = scenarioFromString(value);
            if (s != Scenario::NONE) {
                fault_engine.startScenario(s);
            } else {
                fault_engine.stopScenario();
            }
        } else if (type == "fault_start") {
            fault_engine.startFault(value);
        } else if (type == "fault_stop") {
            fault_engine.stopFault(value);
        } else if (type == "stop") {
            fault_engine.stopScenario();
        }
    }
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    // Setup
    std::string data_dir = "./data";
    if (argc > 1) data_dir = argv[1];
    ensureDir(data_dir);
    ensureDir(data_dir + "/logs");

    Logger::instance().init(data_dir + "/logs/simulator.log", LogLevel::INFO);
    LOG_INFO("main", "VECU-Twin Simulator starting");

    // Initialize simulation components
    CANBus    bus("MainBus");
    EngineECU   engine(bus, 10);
    BrakeECU    brake(bus, 20);
    BatteryECU  battery(bus, 5);
    SteeringECU steering(bus, 20);
    GatewayECU  gateway(bus, 10, 2000);

    VehicleModel model;
    DigitalTwin  twin;

    FaultEngine  fault_engine(engine, brake, battery, steering, model);
    FaultDetector detector(model, twin, gateway);

    // ── Subscribe to CAN bus ─────────────────────────────────────────────────
    // The CAN bus delivers every frame to all subscribers.
    // This simulates the broadcast nature of CAN: every node sees every frame.

    // Vehicle Model subscriber — updates internal state from CAN data
    bus.subscribe("VehicleModel", [&](const CANFrame& f) {
        const auto& name = f.message_name;
        if (name == "ENGINE_RPM") {
            uint16_t rpm = CANFrame::decodeUInt16(f.data, 0);
            double load = static_cast<double>(f.data[2]);
            model.updateRPM(static_cast<double>(rpm), load);
        } else if (name == "ENGINE_TEMP") {
            int16_t raw = CANFrame::decodeInt16(f.data, 0);
            model.updateEngineTemp(raw / 10.0);
        } else if (name == "BRAKE_STATUS") {
            // pressure updated separately
        } else if (name == "BRAKE_PRESSURE") {
            uint16_t raw = CANFrame::decodeUInt16(f.data, 0);
            bool active = false; // will be updated by BRAKE_STATUS
            model.updateBrake(false, raw / 10.0);
        } else if (name == "BRAKE_STATUS") {
            model.updateBrake(f.data[0] == 1, 0.0);
        } else if (name == "VEHICLE_SPEED") {
            uint16_t raw = CANFrame::decodeUInt16(f.data, 0);
            model.updateSpeed(raw / 10.0);
        } else if (name == "BATTERY_STATUS") {
            // voltage/temp updated separately
        } else if (name == "BATTERY_VOLTAGE") {
            uint16_t raw = CANFrame::decodeUInt16(f.data, 0);
            double pct = battery.getPercentage();
            double temp = battery.getTemperature();
            bool charging = battery.isCharging();
            model.updateBattery(pct, raw / 10.0, temp, charging);
        } else if (name == "STEERING_ANGLE") {
            int16_t raw = CANFrame::decodeInt16(f.data, 0);
            model.updateSteering(raw / 10.0);
        }
    });

    // Digital Twin subscriber — builds twin state from received CAN frames
    bus.subscribe("DigitalTwin", [&](const CANFrame& f) {
        twin.processCANFrame(f);
    });

    // Gateway subscriber — monitors network health
    bus.subscribe("GatewayECU", [&](const CANFrame& f) {
        gateway.receiveMessage(f);
    });

    // CAN log subscriber — ring buffer for API
    bus.subscribe("CANLogger", [&](const CANFrame& f) {
        canLogCallback(f);
    });

    // ── Start everything ─────────────────────────────────────────────────────
    signal(SIGINT,  signalHandler);
    signal(SIGTERM, signalHandler);

    bus.start();
    engine.start();
    brake.start();
    battery.start();
    steering.start();
    gateway.start();

    // Start with normal drive scenario
    fault_engine.startScenario(Scenario::NORMAL_DRIVE);

    LOG_INFO("main", "All ECUs online. Writing state to " + data_dir);

    // ── State writer thread ───────────────────────────────────────────────────
    // Writes current state to JSON files at 10 Hz for Python backend to read.
    std::thread state_writer([&]() {
        while (g_running.load()) {
            try {
                // vehicle_state.json
                VehicleState state = model.getCurrentState();
                writeFile(data_dir + "/vehicle_state.json", state.toJson());

                // digital_twin_status.json
                writeFile(data_dir + "/digital_twin_status.json", twin.toStatusJson());

                // twin_state.json
                VehicleState twin_state = twin.getCurrentState();
                writeFile(data_dir + "/twin_state.json", twin_state.toJson());

                // can_log.json
                writeFile(data_dir + "/can_log.json", canLogToJson());

                // fault_events.json
                writeFile(data_dir + "/fault_events.json", faultEventsToJson());

                // ecu_status.json
                writeFile(data_dir + "/ecu_status.json",
                          ecuStatusJson(engine, brake, battery, steering, gateway));

                // scenario.json
                {
                    std::ostringstream ss;
                    ss << "{\"active_scenario\":\"" << fault_engine.getActiveScenarioString() << "\","
                       << "\"active_faults\":[";
                    auto faults = fault_engine.getActiveFaults();
                    for (size_t i = 0; i < faults.size(); ++i) {
                        if (i) ss << ",";
                        ss << "\"" << faults[i] << "\"";
                    }
                    ss << "]}";
                    writeFile(data_dir + "/scenario.json", ss.str());
                }

            } catch (const std::exception& e) {
                LOG_WARN("state_writer", "Error writing state: " + std::string(e.what()));
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    // ── Fault detector thread ─────────────────────────────────────────────────
    std::thread detector_thread([&]() {
        while (g_running.load()) {
            auto new_events = detector.evaluate();
            if (!new_events.empty()) {
                addFaultEvents(new_events);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });

    // ── Command reader thread ─────────────────────────────────────────────────
    std::string cmd_file = data_dir + "/commands.json";
    // Initialize empty
    writeFile(cmd_file, "[]");

    std::thread cmd_reader([&]() {
        while (g_running.load()) {
            try {
                processCommands(cmd_file, fault_engine);
            } catch (...) {}
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });

    // ── Main loop ─────────────────────────────────────────────────────────────
    LOG_INFO("main", "Simulator running. Press Ctrl+C to stop.");
    while (g_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // ── Graceful shutdown ─────────────────────────────────────────────────────
    LOG_INFO("main", "Shutting down...");
    fault_engine.stopScenario();

    gateway.stop();
    steering.stop();
    battery.stop();
    brake.stop();
    engine.stop();
    bus.stop();

    state_writer.join();
    detector_thread.join();
    cmd_reader.join();

    LOG_INFO("main", "VECU-Twin Simulator stopped cleanly");
    return 0;
}
