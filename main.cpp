#define GLAD_GL_IMPLEMENTATION
#include "include/glad/glad.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <iostream>
#include <algorithm>
#include <vector>
#include <string>
#include <filesystem>
#include <map>
#include <cstring>
#include <functional>
#include <cctype>
#include <fstream>
#include <optional>
#include <cstdint>
#include <cmath>
#include <array>
#include <queue>
#include <limits>
#include <cstdio>
#include "json.hpp"

#if __has_include(<AL/al.h>) && __has_include(<AL/alc.h>)
#include <AL/al.h>
#include <AL/alc.h>
#define TRAIN_SIM_HAS_OPENAL 1
#else
#define TRAIN_SIM_HAS_OPENAL 0
#endif

namespace fs = std::filesystem;

// Константы
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;

// Камера
glm::vec3 cameraPos = glm::vec3(5.0f, 5.0f, 10.0f);
glm::vec3 cameraFront = glm::vec3(-1.0f, -1.0f, -1.0f);
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

float yaw = -135.0f;
float pitch = -30.0f;
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Структура для текстуры
struct Texture {
    unsigned int id;
    std::string type;
    std::string path;
};

// Структура для Mesh с текстурами
struct Mesh {
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texCoords;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;

    unsigned int VAO, VBO, EBO, NBO, TBO;

    void setup() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
        glGenBuffers(1, &NBO);
        glGenBuffers(1, &TBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), &vertices[0], GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(0);

        if (!normals.empty()) {
            glBindBuffer(GL_ARRAY_BUFFER, NBO);
            glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(glm::vec3), &normals[0], GL_STATIC_DRAW);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
            glEnableVertexAttribArray(1);
        }

        if (!texCoords.empty()) {
            glBindBuffer(GL_ARRAY_BUFFER, TBO);
            glBufferData(GL_ARRAY_BUFFER, texCoords.size() * sizeof(glm::vec2), &texCoords[0], GL_STATIC_DRAW);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
            glEnableVertexAttribArray(2);
        }

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

        glBindVertexArray(0);
    }

    void draw(unsigned int shaderProgram) {
        const auto diffuseTexture = std::find_if(
            textures.begin(), textures.end(),
            [](const Texture& texture) { return texture.type == "diffuse"; });
        const bool hasDiffuseTexture = diffuseTexture != textures.end();
        glUniform1i(glGetUniformLocation(shaderProgram, "hasTexture"), hasDiffuseTexture);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, hasDiffuseTexture ? diffuseTexture->id : 0);
        glUniform1i(glGetUniformLocation(shaderProgram, "texture_diffuse1"), 0);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }

    void cleanup() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        glDeleteBuffers(1, &NBO);
        glDeleteBuffers(1, &TBO);
        for (auto& texture : textures) {
            glDeleteTextures(1, &texture.id);
        }
    }
};

// Структура для модели
struct Model {
    std::string name;
    std::string description;
    std::string type;
    float weightTonnes = 0.0f;
    float maxSpeedKmh = 0.0f;
    float accelerationMs2 = 0.0f;
    float brakeDecelerationMs2 = 0.0f;
    float enginePowerKw = 0.0f;
    float tractionForceKn = 0.0f;
    float fuelCapacityL = 0.0f;
    float fuelConsumptionLPerKm = 0.0f;
    std::vector<Mesh> meshes;
    glm::mat4 transform = glm::mat4(1.0f);
    glm::mat4 sourceTransform = glm::mat4(1.0f);
    std::map<std::string, glm::vec3> markers;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 routeStart = glm::vec3(0.0f);
    glm::vec3 routeEnd = glm::vec3(0.0f);
    glm::vec3 routeDirection = glm::vec3(0.0f, 0.0f, 1.0f);
    float routeDistance = 0.0f;
    float routePosition = 0.0f;
    float routeVelocity = 0.0f;
    float motionDirection = 1.0f;
    float routeStopTimer = 0.0f;
    float engineThrottle = 0.0f;
    float scale = 0.5f;
    unsigned int engineSoundSource = 0;
    unsigned int engineSoundBuffer = 0;
    float previousRoutePosition = 0.0f;

    void draw(unsigned int shaderProgram) {
        for (auto& mesh : meshes) {
            mesh.draw(shaderProgram);
        }
    }

    void cleanup() {
        for (auto& mesh : meshes) {
            mesh.cleanup();
        }
    }
};

std::vector<Model> trains;

struct TrainConfiguration {
    std::string name;
    std::string type;
    std::string description;
    float weightTonnes = 0.0f;
    float maxSpeedKmh = 0.0f;
    float accelerationMs2 = 0.0f;
    float brakeDecelerationMs2 = 0.0f;
    float enginePowerKw = 0.0f;
    float tractionForceKn = 0.0f;
    float fuelCapacityL = 0.0f;
    float fuelConsumptionLPerKm = 0.0f;
    std::string engineSound;
};

std::optional<TrainConfiguration> loadTrainConfiguration(const fs::path& trainDirectory) {
    const fs::path configPath = fs::exists(trainDirectory / "config.json")
        ? trainDirectory / "config.json" : trainDirectory / "confg.json";
    if (!fs::exists(configPath)) {
        return std::nullopt;
    }

    try {
        std::ifstream input(configPath);
        nlohmann::json json;
        input >> json;
        TrainConfiguration config;
        config.name = json.value("name", trainDirectory.filename().string());
        config.type = json.value("type", "");
        config.description = json.value("description", "");
        const auto& physical = json.value("physical", nlohmann::json::object());
        config.weightTonnes = physical.value("weight_tonnes", 0.0f);
        config.maxSpeedKmh = physical.value("max_speed_kmh", 0.0f);
        config.accelerationMs2 = physical.value("acceleration_ms2", 0.0f);
        config.brakeDecelerationMs2 = physical.value("brake_deceleration_ms2", 0.0f);
        const auto& technical = json.value("technical", nlohmann::json::object());
        config.enginePowerKw = technical.value("engine_power_kw", 0.0f);
        config.tractionForceKn = technical.value("traction_force_kn", 0.0f);
        config.fuelCapacityL = technical.value("fuel_capacity_l", 0.0f);
        config.fuelConsumptionLPerKm = technical.value("fuel_consumption_l_per_km", 0.0f);
        const auto& visual = json.value("visual", nlohmann::json::object());
        config.engineSound = visual.value("engine_sound", "");
        return config;
    } catch (const std::exception& error) {
        std::cout << "Cannot read train configuration " << configPath << ": "
                  << error.what() << std::endl;
        return std::nullopt;
    }
}

#if TRAIN_SIM_HAS_OPENAL
class TrainAudioSystem {
public:
    bool initialize() {
        device = alcOpenDevice(nullptr);
        if (!device || !(context = alcCreateContext(device, nullptr)) || !alcMakeContextCurrent(context)) {
            std::cout << "OpenAL is unavailable; train sounds are disabled" << std::endl;
            shutdown();
            return false;
        }
        alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
        alListenerf(AL_GAIN, 1.0f);
        return true;
    }

    bool startLoopingEngine(Model& train, const fs::path& soundPath) {
        std::vector<char> pcm;
        ALenum format;
        ALsizei sampleRate;
        float durationSeconds = 0.0f;
        if (!loadPcmSound(soundPath, pcm, format, sampleRate, durationSeconds)) {
            std::cout << "Cannot load engine WAV: " << soundPath << std::endl;
            return false;
        }
        ALuint buffer = 0, source = 0;
        alGenBuffers(1, &buffer);
        alBufferData(buffer, format, pcm.data(), static_cast<ALsizei>(pcm.size()), sampleRate);
        alGenSources(1, &source);
        alSourcei(source, AL_BUFFER, buffer);
        alSourcei(source, AL_LOOPING, AL_TRUE);
        alSourcef(source, AL_REFERENCE_DISTANCE, 4.0f);
        alSourcef(source, AL_MAX_DISTANCE, 90.0f);
        alSourcef(source, AL_ROLLOFF_FACTOR, 1.6f);
        train.engineSoundBuffer = buffer;
        train.engineSoundSource = source;
        updateSource(train);
        alSourcePlay(source);
        return true;
    }

    bool loadOneShotBuffer(const fs::path& soundPath, unsigned int& buffer, float& durationSeconds) const {
        std::vector<char> pcm;
        ALenum format;
        ALsizei sampleRate;
        if (!loadPcmSound(soundPath, pcm, format, sampleRate, durationSeconds)) {
            std::cout << "Cannot load rail joint sound: " << soundPath << std::endl;
            return false;
        }

        alGenBuffers(1, &buffer);
        alBufferData(buffer, format, pcm.data(), static_cast<ALsizei>(pcm.size()), sampleRate);
        return true;
    }

    unsigned int playOneShot(unsigned int buffer, const glm::vec3& position, float pitch, float gain) const {
        ALuint source = 0;
        alGenSources(1, &source);
        alSourcei(source, AL_BUFFER, buffer);
        alSourcei(source, AL_LOOPING, AL_FALSE);
        alSource3f(source, AL_POSITION, position.x, position.y, position.z);
        alSourcef(source, AL_REFERENCE_DISTANCE, 5.0f);
        alSourcef(source, AL_MAX_DISTANCE, 120.0f);
        alSourcef(source, AL_ROLLOFF_FACTOR, 1.4f);
        alSourcef(source, AL_PITCH, pitch);
        alSourcef(source, AL_GAIN, gain);
        alSourcePlay(source);
        return source;
    }

    void cleanupStoppedSources(std::vector<unsigned int>& sources) const {
        sources.erase(std::remove_if(sources.begin(), sources.end(), [](unsigned int source) {
            ALint state = AL_STOPPED;
            alGetSourcei(source, AL_SOURCE_STATE, &state);
            if (state == AL_STOPPED) {
                alDeleteSources(1, &source);
                return true;
            }
            return false;
        }), sources.end());
    }

    void releaseSources(std::vector<unsigned int>& sources) const {
        if (!sources.empty()) {
            alDeleteSources(static_cast<ALsizei>(sources.size()), sources.data());
            sources.clear();
        }
    }

    void releaseBuffer(unsigned int& buffer) const {
        if (buffer != 0) alDeleteBuffers(1, &buffer);
        buffer = 0;
    }

    void updateListener(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up) {
        alListener3f(AL_POSITION, position.x, position.y, position.z);
        const float orientation[] = {forward.x, forward.y, forward.z, up.x, up.y, up.z};
        alListenerfv(AL_ORIENTATION, orientation);
    }

    void updateSource(const Model& train) const {
        if (train.engineSoundSource != 0) {
            alSource3f(train.engineSoundSource, AL_POSITION, train.position.x, train.position.y, train.position.z);
        }
    }

    void updateEngine(const Model& train, const glm::vec3& listenerPosition) const {
        if (train.engineSoundSource == 0) return;

        updateSource(train);
        const float distance = glm::length(listenerPosition - train.position);
        const float fadeStart = 60.0f;
        const float fadeEnd = 90.0f;
        const float distanceGain = distance >= fadeEnd ? 0.0f
            : (distance <= fadeStart ? 1.0f : (fadeEnd - distance) / (fadeEnd - fadeStart));
        const float maxSpeedMs = train.maxSpeedKmh > 0.0f ? train.maxSpeedKmh / 3.6f : 20.0f;
        const float speedFactor = std::clamp(std::abs(train.routeVelocity) / maxSpeedMs, 0.0f, 1.0f);
        const float throttleFactor = std::clamp(train.engineThrottle, 0.0f, 1.0f);
        const float gain = distanceGain * (0.25f + 0.75f * std::max(speedFactor, throttleFactor));
        const float pitch = 0.85f + 0.25f * speedFactor + 0.40f * throttleFactor;
        alSourcef(train.engineSoundSource, AL_GAIN, gain);
        alSourcef(train.engineSoundSource, AL_PITCH, pitch);
    }

    void release(Model& train) const {
        if (train.engineSoundSource != 0) alDeleteSources(1, &train.engineSoundSource);
        if (train.engineSoundBuffer != 0) alDeleteBuffers(1, &train.engineSoundBuffer);
        train.engineSoundSource = train.engineSoundBuffer = 0;
    }

    void shutdown() {
        if (context) { alcMakeContextCurrent(nullptr); alcDestroyContext(context); context = nullptr; }
        if (device) { alcCloseDevice(device); device = nullptr; }
    }

private:
    ALCdevice* device = nullptr;
    ALCcontext* context = nullptr;

    static uint32_t readU32(const char* value) {
        return static_cast<unsigned char>(value[0]) | (static_cast<uint32_t>(static_cast<unsigned char>(value[1])) << 8) |
               (static_cast<uint32_t>(static_cast<unsigned char>(value[2])) << 16) | (static_cast<uint32_t>(static_cast<unsigned char>(value[3])) << 24);
    }
    static uint16_t readU16(const char* value) { return static_cast<unsigned char>(value[0]) | (static_cast<uint16_t>(static_cast<unsigned char>(value[1])) << 8); }

    static bool loadPcmSound(const fs::path& path, std::vector<char>& pcm, ALenum& format, ALsizei& sampleRate, float& durationSeconds) {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".mp3") {
            return readMp3WithFfmpeg(path, pcm, format, sampleRate, durationSeconds);
        }
        return readWave(path, pcm, format, sampleRate, durationSeconds);
    }

    static std::string shellQuote(const fs::path& path) {
        std::string quoted = "'";
        for (char c : path.string()) {
            quoted += c == '\'' ? "'\\''" : std::string(1, c);
        }
        quoted += "'";
        return quoted;
    }

    static bool readMp3WithFfmpeg(const fs::path& path, std::vector<char>& pcm, ALenum& format, ALsizei& sampleRate, float& durationSeconds) {
        constexpr ALsizei mp3SampleRate = 44100;
        const std::string command = "ffmpeg -v error -i " + shellQuote(path)
            + " -f s16le -acodec pcm_s16le -ac 1 -ar 44100 -";
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) return false;

        std::array<char, 4096> buffer;
        while (true) {
            const size_t read = std::fread(buffer.data(), 1, buffer.size(), pipe);
            if (read > 0) pcm.insert(pcm.end(), buffer.data(), buffer.data() + read);
            if (read < buffer.size()) {
                if (std::feof(pipe)) break;
                if (std::ferror(pipe)) {
                    pclose(pipe);
                    return false;
                }
            }
        }

        const int status = pclose(pipe);
        if (status != 0 || pcm.empty()) return false;
        format = AL_FORMAT_MONO16;
        sampleRate = mp3SampleRate;
        durationSeconds = static_cast<float>(pcm.size()) / (static_cast<float>(sampleRate) * sizeof(int16_t));
        return true;
    }

    static bool readWave(const fs::path& path, std::vector<char>& pcm, ALenum& format, ALsizei& sampleRate, float& durationSeconds) {
        std::ifstream input(path, std::ios::binary);
        char header[12];
        if (!input.read(header, sizeof(header)) || std::memcmp(header, "RIFF", 4) != 0 || std::memcmp(header + 8, "WAVE", 4) != 0) return false;
        uint16_t channels = 0, bitsPerSample = 0, encoding = 0;
        while (input) {
            char chunk[8];
            if (!input.read(chunk, sizeof(chunk))) break;
            const uint32_t size = readU32(chunk + 4);
            if (std::memcmp(chunk, "fmt ", 4) == 0) {
                std::vector<char> fmt(size);
                if (size < 16 || !input.read(fmt.data(), size)) return false;
                encoding = readU16(fmt.data()); channels = readU16(fmt.data() + 2);
                sampleRate = static_cast<ALsizei>(readU32(fmt.data() + 4)); bitsPerSample = readU16(fmt.data() + 14);
            } else if (std::memcmp(chunk, "data", 4) == 0) {
                pcm.resize(size);
                if (!input.read(pcm.data(), size)) return false;
                break;
            } else input.seekg(size + (size & 1), std::ios::cur);
        }
        if (encoding != 1 || pcm.empty() || (channels != 1 && channels != 2)) return false;
        if (bitsPerSample == 8 && channels == 2) {
            std::vector<char> mono(pcm.size() / 2);
            for (size_t i = 0; i < mono.size(); ++i) {
                const unsigned int left = static_cast<unsigned char>(pcm[i * 2]);
                const unsigned int right = static_cast<unsigned char>(pcm[i * 2 + 1]);
                mono[i] = static_cast<char>((left + right) / 2);
            }
            pcm = std::move(mono);
            channels = 1;
        } else if (bitsPerSample == 16 && channels == 2) {
            std::vector<char> mono(pcm.size() / 2);
            for (size_t i = 0; i < mono.size() / 2; ++i) {
                int16_t left, right;
                std::memcpy(&left, pcm.data() + i * 4, sizeof(left));
                std::memcpy(&right, pcm.data() + i * 4 + sizeof(right), sizeof(right));
                const int16_t mixed = static_cast<int16_t>((static_cast<int>(left) + static_cast<int>(right)) / 2);
                std::memcpy(mono.data() + i * 2, &mixed, sizeof(mixed));
            }
            pcm = std::move(mono);
            channels = 1;
        }
        if (channels == 1 && bitsPerSample == 8) format = AL_FORMAT_MONO8;
        else if (channels == 1 && bitsPerSample == 16) format = AL_FORMAT_MONO16;
        else return false;
        durationSeconds = sampleRate > 0
            ? static_cast<float>(pcm.size()) / (static_cast<float>(sampleRate) * static_cast<float>(bitsPerSample / 8))
            : 0.0f;
        return true;
    }
};
#else
class TrainAudioSystem {
public:
    bool initialize() { std::cout << "Train sounds require an OpenAL-enabled build" << std::endl; return false; }
    bool startLoopingEngine(Model&, const fs::path&) { return false; }
    bool loadOneShotBuffer(const fs::path&, unsigned int&, float&) const { return false; }
    unsigned int playOneShot(unsigned int, const glm::vec3&, float, float) const { return 0; }
    void cleanupStoppedSources(std::vector<unsigned int>&) const {}
    void releaseSources(std::vector<unsigned int>&) const {}
    void releaseBuffer(unsigned int&) const {}
    void updateListener(const glm::vec3&, const glm::vec3&, const glm::vec3&) {}
    void updateSource(const Model&) const {}
    void updateEngine(const Model&, const glm::vec3&) const {}
    void release(Model&) const {}
    void shutdown() {}
};
#endif

struct TrackSegment {
    glm::mat4 transform;
    glm::vec3 start;
    glm::vec3 end;
    float startHeadingRadians = 0.0f;
    float curvatureRadiansPerMeter = 0.0f;
    float length = 0.0f;
    float distanceFromRouteStart = 0.0f;
};

std::vector<TrackSegment> trackSegments;

struct TrackJointSound {
    glm::vec3 position;
    float routePosition = 0.0f;
};

std::vector<TrackJointSound> trackJointSounds;
std::vector<unsigned int> activeJointSoundSources;
unsigned int jointSoundBuffer = 0;
float jointSoundDurationSeconds = 0.0f;

constexpr float jointSoundIntervalMeters = 25.0f;
constexpr float jointTwoAxleDistanceMeters = 2.7f;
constexpr float jointSoundPaddingSeconds = 0.08f;

struct RouteSample {
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 direction = glm::vec3(0.0f, 0.0f, 1.0f);
};

struct RoutePoint {
    glm::vec3 position;
    size_t segmentIndex = 0;
    float distanceAlongSegment = 0.0f;
};



enum class TrackBuildTool { Straight, Curve };

bool trackBuildMode = false;
TrackBuildTool trackBuildTool = TrackBuildTool::Straight;
std::optional<glm::vec3> trackBuildStart;
std::optional<float> trackBuildStartHeading;
bool trackBuildStartIsConnected = false;
bool previousHPressed = false;
bool previousIPressed = false;
bool previousJPressed = false;
bool previousLeftMousePressed = false;
bool previousRightMousePressed = false;
bool routeBuildMode = false;
bool previousPPressed = false;
bool previousXPressed = false;
bool previousRouteLeftMousePressed = false;
bool customRouteClosed = false;
bool customRouteChanged = false;
std::vector<glm::vec3> customRoutePoints;
// Every completed route waypoint is an operational stop before the train
// continues to the following waypoint.
std::vector<float> customRouteStopPositions;
std::optional<RoutePoint> customRouteFirstTrackPoint;
std::optional<RoutePoint> customRouteLastTrackPoint;

constexpr float trackSnapDistanceMeters = 1.25f;
constexpr float maximumStraightJoinAngleRadians = glm::radians(5.0f);
constexpr float maximumCurveTurnRadians = glm::radians(90.0f);
constexpr float minimumCurveRadiusMeters = 20.0f;
constexpr float routeCloseSnapDistanceMeters = 1.25f;
// Track pieces created at the same endpoint can differ slightly because of
// floating point curve calculations.  This is deliberately much smaller than
// the editor snapping radius, so nearby but unconnected rails stay separate.
constexpr float routeJunctionToleranceMeters = 0.05f;
constexpr float routeReversalStopDurationSeconds = 2.0f;

bool saveTrackMap() {
    const fs::path mapDirectory = "maps";
    const fs::path mapPath = mapDirectory / "latest_track_map.json";
    std::error_code error;
    fs::create_directories(mapDirectory, error);
    if (error) {
        std::cout << "Cannot create map directory " << mapDirectory << ": "
                  << error.message() << std::endl;
        return false;
    }

    nlohmann::json map = {
        {"format", "train-simulator-track-map"},
        {"version", 1},
        {"segments", nlohmann::json::array()}
    };
    for (const TrackSegment& segment : trackSegments) {
        map["segments"].push_back({
            {"start", {segment.start.x, segment.start.y, segment.start.z}},
            {"end", {segment.end.x, segment.end.y, segment.end.z}},
            {"start_heading_radians", segment.startHeadingRadians},
            {"curvature_radians_per_meter", segment.curvatureRadiansPerMeter},
            {"length_meters", segment.length}
        });
    }

    std::ofstream output(mapPath);
    if (!output) {
        std::cout << "Cannot save track map to " << mapPath << std::endl;
        return false;
    }
    output << map.dump(2) << '\n';
    std::cout << "Track map saved to " << mapPath << " ("
              << trackSegments.size() << " segments)" << std::endl;
    return true;
}

// Обработчики GLFW
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
    if (trackBuildMode) return;
    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

// Загрузка текстуры из памяти
unsigned int loadTextureFromMemory(const unsigned char* data, int width, int height, int channels) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    GLenum format;
    if (channels == 1)
        format = GL_RED;
    else if (channels == 3)
        format = GL_RGB;
    else if (channels == 4)
        format = GL_RGBA;

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return textureID;
}

// Загрузка embedded текстуры из Assimp
unsigned int loadEmbeddedTexture(const aiTexture* embeddedTexture) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    int width, height, channels;
    unsigned char* data = nullptr;

    if (embeddedTexture->mHeight == 0) {
        data = stbi_load_from_memory(
            reinterpret_cast<const unsigned char*>(embeddedTexture->pcData),
            embeddedTexture->mWidth,
            &width, &height, &channels, 0
        );
    } else {
        data = stbi_load_from_memory(
            reinterpret_cast<const unsigned char*>(embeddedTexture->pcData),
            embeddedTexture->mWidth * embeddedTexture->mHeight * 4,
            &width, &height, &channels, 0
        );
    }

    if (data) {
        GLenum format;
        if (channels == 1)
            format = GL_RED;
        else if (channels == 3)
            format = GL_RGB;
        else if (channels == 4)
            format = GL_RGBA;

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
        std::cout << "Loaded embedded texture" << std::endl;
        return textureID;
    } else {
        std::cout << "Failed to load embedded texture" << std::endl;
        stbi_image_free(data);
        glDeleteTextures(1, &textureID);
        return 0;
    }
}

// Функция загрузки текстур из материала
std::vector<Texture> loadMaterialTextures(aiMaterial* mat, aiTextureType type, std::string typeName,
                                          const aiScene* scene, const std::string& directory) {
    std::vector<Texture> textures;

    for (unsigned int i = 0; i < mat->GetTextureCount(type); i++) {
        aiString str;
        mat->GetTexture(type, i, &str);

        bool skip = false;
        for (unsigned int j = 0; j < textures.size(); j++) {
            if (std::strcmp(textures[j].path.c_str(), str.C_Str()) == 0) {
                skip = true;
                break;
            }
        }

        if (!skip) {
            Texture texture;
            texture.type = typeName;
            texture.path = str.C_Str();

            if (str.C_Str()[0] == '*') {
                int textureIndex = std::stoi(str.C_Str() + 1);

                if (scene->mTextures && scene->mNumTextures > textureIndex) {
                    const aiTexture* embeddedTexture = scene->mTextures[textureIndex];
                    unsigned int id = loadEmbeddedTexture(embeddedTexture);
                    if (id != 0) {
                        texture.id = id;
                        textures.push_back(texture);
                        std::cout << "Loaded embedded texture: " << str.C_Str() << std::endl;
                    }
                }
            }
        }
    }

    return textures;
}

// Процессинг меша с текстурами
Mesh processMesh(aiMesh* mesh, const aiScene* scene, const std::string& directory) {
    Mesh result;

    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        result.vertices.push_back(glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z));
        if (mesh->HasNormals()) {
            result.normals.push_back(glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z));
        }
        if (mesh->mTextureCoords[0]) {
            result.texCoords.push_back(glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y));
        }
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            result.indices.push_back(face.mIndices[j]);
        }
    }

    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

        std::vector<Texture> baseColorMaps = loadMaterialTextures(
            material, aiTextureType_BASE_COLOR, "diffuse", scene, directory);
        if (baseColorMaps.empty()) {
            baseColorMaps = loadMaterialTextures(
                material, aiTextureType_DIFFUSE, "diffuse", scene, directory);
        }
        result.textures.insert(result.textures.end(), baseColorMaps.begin(), baseColorMaps.end());

        std::vector<Texture> specularMaps = loadMaterialTextures(material, aiTextureType_SPECULAR, "specular", scene, directory);
        result.textures.insert(result.textures.end(), specularMaps.begin(), specularMaps.end());

        std::vector<Texture> normalMaps = loadMaterialTextures(material, aiTextureType_NORMALS, "normal", scene, directory);
        result.textures.insert(result.textures.end(), normalMaps.begin(), normalMaps.end());
    }

    result.setup();
    return result;
}

Model loadModel(const std::string& path, const std::string& name) {
    Model model;
    model.name = name;

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs |
        aiProcess_CalcTangentSpace);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << std::endl;
        return model;
    }

    std::string directory = fs::path(path).parent_path().string();

    auto toGlm = [](const aiMatrix4x4& matrix) {
        return glm::mat4(
            matrix.a1, matrix.b1, matrix.c1, matrix.d1,
            matrix.a2, matrix.b2, matrix.c2, matrix.d2,
            matrix.a3, matrix.b3, matrix.c3, matrix.d3,
            matrix.a4, matrix.b4, matrix.c4, matrix.d4);
    };
    bool foundMeshNode = false;
    std::function<void(const aiNode*, const glm::mat4&)> readNodes;
    readNodes = [&](const aiNode* node, const glm::mat4& parentTransform) {
        const glm::mat4 nodeTransform = parentTransform * toGlm(node->mTransformation);
        if (node->mNumMeshes > 0 && !foundMeshNode) {
            model.sourceTransform = nodeTransform;
            foundMeshNode = true;
        }

        std::string markerName = node->mName.C_Str();
        std::transform(markerName.begin(), markerName.end(), markerName.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (markerName == "track_start" || markerName == "track_end" ||
            markerName == "pivot_front" || markerName == "pivot_back") {
            model.markers[markerName] = glm::vec3(nodeTransform[3]);
        }

        for (unsigned int i = 0; i < node->mNumChildren; ++i) {
            readNodes(node->mChildren[i], nodeTransform);
        }
    };
    readNodes(scene->mRootNode, glm::mat4(1.0f));

    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        model.meshes.push_back(processMesh(scene->mMeshes[i], scene, directory));
    }

    std::cout << "Loaded model: " << name << " (" << model.meshes.size() << " meshes)" << std::endl;
    return model;
}

// Шейдеры
const char* modelVertexShader = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec3 aNormal;
    layout (location = 2) in vec2 aTexCoord;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;
    uniform vec3 lightPos;

    out vec3 Normal;
    out vec3 FragPos;
    out vec2 TexCoord;

    void main() {
        gl_Position = projection * view * model * vec4(aPos, 1.0);
        FragPos = vec3(model * vec4(aPos, 1.0));
        Normal = mat3(transpose(inverse(model))) * aNormal;
        TexCoord = aTexCoord;
    }
)";

const char* modelFragmentShader = R"(
    #version 330 core
    in vec3 Normal;
    in vec3 FragPos;
    in vec2 TexCoord;
    out vec4 FragColor;

    uniform vec3 lightPos;
    uniform vec3 viewPos;
    uniform sampler2D texture_diffuse1;
    uniform bool hasTexture;

    void main() {
        vec3 color;
        if (hasTexture) {
            color = texture(texture_diffuse1, TexCoord).rgb;
        } else {
            color = vec3(0.8f, 0.2f, 0.2f);
        }

        float ambientStrength = 0.3f;
        vec3 ambient = ambientStrength * color;

        vec3 norm = normalize(Normal);
        vec3 lightDir = normalize(lightPos - FragPos);
        float diff = max(dot(norm, lightDir), 0.0f);
        vec3 diffuse = diff * color;

        float specularStrength = 0.5f;
        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0f), 32);
        vec3 specular = specularStrength * spec * vec3(1.0f, 1.0f, 1.0f);

        vec3 result = ambient + diffuse + specular;
        FragColor = vec4(result, 1.0f);
    }
)";

const char* planeVertexShader = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 2) in vec2 aTexCoord;

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    out vec2 TexCoord;

    void main() {
        gl_Position = projection * view * model * vec4(aPos, 1.0);
        TexCoord = aTexCoord;
    }
)";

const char* planeFragmentShader = R"(
    #version 330 core
    in vec2 TexCoord;
    out vec4 FragColor;

    void main() {
        float lineWidth = 0.03f;
        vec2 grid = abs(fract(TexCoord * 10.0f - 0.5f) - 0.5f);
        float lineX = step(grid.x, lineWidth);
        float lineY = step(grid.y, lineWidth);
        float gridLine = max(lineX, lineY);

        vec3 baseColor = vec3(0.2f, 0.6f, 0.2f);
        vec3 lineColor = vec3(0.1f, 0.4f, 0.1f);

        vec3 finalColor = mix(baseColor, lineColor, gridLine);
        FragColor = vec4(finalColor, 1.0f);
    }
)";

const char* previewVertexShader = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    uniform mat4 view;
    uniform mat4 projection;
    void main() {
        gl_Position = projection * view * vec4(aPos, 1.0);
    }
)";

const char* previewFragmentShader = R"(
    #version 330 core
    out vec4 FragColor;
    uniform vec3 previewColor;
    void main() {
        FragColor = vec4(previewColor, 1.0);
    }
)";

unsigned int createShaderProgram(const char* vertexSource, const char* fragmentSource) {
    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

// Плоскость
unsigned int planeVAO, planeVBO, planeEBO;
bool planeInitialized = false;

void initPlane() {
    if (planeInitialized) return;

    float vertices[] = {
        -30.0f, 0.0f, -30.0f,   0.0f, 0.0f,
         30.0f, 0.0f, -30.0f,   1.0f, 0.0f,
         30.0f, 0.0f,  30.0f,   1.0f, 1.0f,
        -30.0f, 0.0f,  30.0f,   0.0f, 1.0f
    };

    unsigned int indices[] = {0, 1, 2, 0, 2, 3};

    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);
    glGenBuffers(1, &planeEBO);

    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, planeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);

    planeInitialized = true;
}

void drawPlane() {
    if (!planeInitialized) initPlane();
    glBindVertexArray(planeVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}


unsigned int previewVAO = 0;
unsigned int previewVBO = 0;

void drawTrackPreview(unsigned int shaderProgram, const std::vector<glm::vec3>& points,
                      const glm::mat4& view, const glm::mat4& projection,
                      const glm::vec3& color) {
    if (points.empty()) return;
    if (previewVAO == 0) {
        glGenVertexArrays(1, &previewVAO);
        glGenBuffers(1, &previewVBO);
    }

    glUseProgram(shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(glGetUniformLocation(shaderProgram, "previewColor"), 1, glm::value_ptr(color));
    glBindVertexArray(previewVAO);
    glBindBuffer(GL_ARRAY_BUFFER, previewVBO);
    glBufferData(GL_ARRAY_BUFFER, points.size() * sizeof(glm::vec3), points.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glEnableVertexAttribArray(0);
    if (points.size() == 1) {
        glPointSize(12.0f);
        glDrawArrays(GL_POINTS, 0, 1);
    } else {
        glLineWidth(4.0f);
        glDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(points.size()));
    }
    glBindVertexArray(0);
}

std::vector<fs::path> findTrainModelPaths() {
    std::vector<fs::path> modelPaths;
    if (!fs::exists("trains")) {
        return modelPaths;
    }

    for (const auto& entry : fs::directory_iterator("trains")) {
        if (entry.is_directory()) {
            for (const auto& fileEntry : fs::directory_iterator(entry.path())) {
                if (fileEntry.is_regular_file()) {
                    std::string ext = fileEntry.path().extension().string();
                    std::string extLower = ext;
                    for (char& c : extLower) c = std::tolower(c);

                    if (extLower == ".glb" || extLower == ".gltf" || extLower == ".obj") {
                        modelPaths.push_back(fileEntry.path());
                    }
                }
            }
        }
    }
    std::sort(modelPaths.begin(), modelPaths.end());
    return modelPaths;
}

RouteSample sampleTrackRoute(float routePosition) {
    if (trackSegments.empty()) {
        return {};
    }

    const TrackSegment& lastSegment = trackSegments.back();
    const float routeLength = lastSegment.distanceFromRouteStart + lastSegment.length;
    const float clampedPosition = std::clamp(routePosition, 0.0f, routeLength);

    for (const TrackSegment& segment : trackSegments) {
        const float segmentEndDistance = segment.distanceFromRouteStart + segment.length;
        if (clampedPosition <= segmentEndDistance) {
            const float localDistance = clampedPosition - segment.distanceFromRouteStart;
            const float heading = segment.startHeadingRadians +
                segment.curvatureRadiansPerMeter * localDistance;
            glm::vec3 position;

            if (std::abs(segment.curvatureRadiansPerMeter) < 0.000001f) {
                position = segment.start + glm::vec3(
                    std::sin(segment.startHeadingRadians), 0.0f,
                    std::cos(segment.startHeadingRadians)) * localDistance;
            } else {
                const float inverseCurvature = 1.0f / segment.curvatureRadiansPerMeter;
                position = segment.start + glm::vec3(
                    (std::cos(segment.startHeadingRadians) - std::cos(heading)) * inverseCurvature,
                    0.0f,
                    (std::sin(heading) - std::sin(segment.startHeadingRadians)) * inverseCurvature);
            }

            return {
                position,
                glm::vec3(std::sin(heading), 0.0f, std::cos(heading))
            };
        }
    }

    const float lastHeading = lastSegment.startHeadingRadians +
        lastSegment.curvatureRadiansPerMeter * lastSegment.length;
    return {lastSegment.end, glm::vec3(std::sin(lastHeading), 0.0f, std::cos(lastHeading))};
}

glm::mat4 createTrackAlignedTransform(
    const glm::vec3& position,
    const glm::vec3& direction,
    float scale
) {
    const float yaw = std::atan2(direction.x, direction.z);
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);
    transform = glm::rotate(transform, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
    transform = glm::scale(transform, glm::vec3(scale));
    return transform;
}

bool loadTrackMap(float assetScale) {
    const fs::path mapPath = fs::path("maps") / "latest_track_map.json";
    if (!fs::exists(mapPath)) return false;

    try {
        std::ifstream input(mapPath);
        nlohmann::json map;
        input >> map;
        if (map.value("format", "") != "train-simulator-track-map" ||
            map.value("version", 0) != 1 || !map.contains("segments") ||
            !map["segments"].is_array()) {
            std::cout << "Ignoring invalid track map " << mapPath << std::endl;
            return false;
        }

        std::vector<TrackSegment> loadedSegments;
        float routeDistance = 0.0f;
        for (const nlohmann::json& entry : map["segments"]) {
            const auto& start = entry.at("start");
            const auto& end = entry.at("end");
            const float length = entry.at("length_meters").get<float>();
            if (start.size() != 3 || end.size() != 3 || length <= 0.0f) continue;
            const glm::vec3 startPoint(start[0].get<float>(), start[1].get<float>(), start[2].get<float>());
            const glm::vec3 endPoint(end[0].get<float>(), end[1].get<float>(), end[2].get<float>());
            const float heading = entry.at("start_heading_radians").get<float>();
            const float curvature = entry.at("curvature_radians_per_meter").get<float>();
            const float midpointHeading = heading + curvature * length * 0.5f;
            glm::vec3 center = (startPoint + endPoint) * 0.5f;
            if (std::abs(curvature) >= 0.000001f) {
                const float inverseCurvature = 1.0f / curvature;
                center = startPoint + glm::vec3(
                    (std::cos(heading) - std::cos(midpointHeading)) * inverseCurvature, 0.0f,
                    (std::sin(midpointHeading) - std::sin(heading)) * inverseCurvature);
            }
            glm::mat4 transform = createTrackAlignedTransform(
                center, glm::vec3(std::sin(midpointHeading), 0.0f, std::cos(midpointHeading)), assetScale);
            transform = glm::scale(transform, glm::vec3(1.0f, 1.0f, length / 5.0f));
            loadedSegments.push_back({transform, startPoint, endPoint, heading, curvature, length, routeDistance});
            routeDistance += length;
        }
        if (loadedSegments.empty()) return false;
        trackSegments = std::move(loadedSegments);
        std::cout << "Loaded track map " << mapPath << " (" << trackSegments.size() << " segments)" << std::endl;
        return true;
    } catch (const std::exception& error) {
        std::cout << "Cannot load track map " << mapPath << ": " << error.what() << std::endl;
        return false;
    }
}

struct TrackConnection {
    glm::vec3 position;
    std::optional<float> heading;
    bool isConnected = false;
};

TrackConnection snapTrackConnection(const glm::vec3& position) {
    TrackConnection closest{position, std::nullopt, false};
    float closestDistance = trackSnapDistanceMeters; // <-- было routeSnapDistanceMeters
    for (const TrackSegment& segment : trackSegments) {
        const float endHeading = segment.startHeadingRadians +
            segment.curvatureRadiansPerMeter * segment.length;
        const std::array<TrackConnection, 2> endpoints{{
            {segment.start, segment.startHeadingRadians + glm::pi<float>(), true},
            {segment.end, endHeading, true}
        }};
        for (const TrackConnection& endpoint : endpoints) {
            const float distance = glm::length(position - endpoint.position);
            if (distance < closestDistance) {
                closest = endpoint;
                closestDistance = distance;
            }
        }
    }
    return closest;
}

float absoluteAngleDifference(float first, float second) {
    return std::abs(std::atan2(std::sin(first - second), std::cos(first - second)));
}

std::vector<TrackSegment> makeTrackPieces(const glm::vec3& start, const glm::vec3& end,
                                          std::optional<float> startHeading, TrackBuildTool tool,
                                          std::optional<float> endHeading,
                                          float assetScale, float routeDistance) {
    const glm::vec3 offset = end - start;
    const float chordLength = glm::length(glm::vec2(offset.x, offset.z));
    if (chordLength < 0.05f) return {};

    float heading = startHeading.value_or(std::atan2(offset.x, offset.z));
    float curvature = 0.0f;
    float totalLength = chordLength;
    const float chordHeading = std::atan2(offset.x, offset.z);
    if (tool == TrackBuildTool::Straight && startHeading &&
        absoluteAngleDifference(heading, chordHeading) > maximumStraightJoinAngleRadians) {
        return {};
    }
    if (tool == TrackBuildTool::Curve) {
        if (endHeading) {
            const float reversedHeading = *endHeading + glm::pi<float>();
            const float selectedEndHeading = std::abs(std::atan2(
                std::sin(*endHeading - chordHeading), std::cos(*endHeading - chordHeading)))
                <= std::abs(std::atan2(std::sin(reversedHeading - chordHeading),
                                       std::cos(reversedHeading - chordHeading)))
                ? *endHeading : reversedHeading;
            const float reverseHeading = selectedEndHeading + glm::pi<float>();
            const glm::vec3 reverseOffset = start - end;
            const glm::vec3 reverseNormal(std::cos(reverseHeading), 0.0f, -std::sin(reverseHeading));
            const float reverseCurvature = 2.0f * glm::dot(reverseOffset, reverseNormal) /
                (chordLength * chordLength);
            if (std::abs(reverseCurvature) > 0.0001f) {
                const glm::vec3 center = end + reverseNormal / reverseCurvature;
                const glm::vec3 fromCenterEnd = end - center;
                const glm::vec3 fromCenterStart = start - center;
                const float reverseTurn = std::atan2(
                    fromCenterEnd.x * fromCenterStart.z - fromCenterEnd.z * fromCenterStart.x,
                    glm::dot(fromCenterEnd, fromCenterStart));
                totalLength = std::abs(reverseTurn / reverseCurvature);
                curvature = -reverseCurvature;
                heading = selectedEndHeading - curvature * totalLength;
            }
        } else {
            const glm::vec3 normal(std::cos(heading), 0.0f, -std::sin(heading));
            curvature = 2.0f * glm::dot(offset, normal) / (chordLength * chordLength);
            if (std::abs(curvature) > 0.0001f) {
                const glm::vec3 center = start + normal / curvature;
                const glm::vec3 fromCenterStart = start - center;
                const glm::vec3 fromCenterEnd = end - center;
                const float turn = std::atan2(
                    fromCenterStart.x * fromCenterEnd.z - fromCenterStart.z * fromCenterEnd.x,
                    glm::dot(fromCenterStart, fromCenterEnd));
                totalLength = std::abs(turn / curvature);
            }
        }
        if (totalLength < 0.05f) curvature = 0.0f;
        const float curveTurn = std::abs(curvature * totalLength);
        if (std::abs(curvature) > 1.0f / minimumCurveRadiusMeters ||
            curveTurn > maximumCurveTurnRadians) {
            return {};
        }
    }

    const int pieceCount = std::max(1, static_cast<int>(std::ceil(totalLength / 5.0f)));
    const float pieceLength = totalLength / pieceCount;
    std::vector<TrackSegment> pieces;
    pieces.reserve(pieceCount);
    glm::vec3 cursor = start;
    for (int i = 0; i < pieceCount; ++i) {
        const float endHeading = heading + curvature * pieceLength;
        const float midpointHeading = (heading + endHeading) * 0.5f;
        glm::vec3 pieceEnd;
        glm::vec3 center;
        if (std::abs(curvature) < 0.000001f) {
            const glm::vec3 direction(std::sin(heading), 0.0f, std::cos(heading));
            pieceEnd = cursor + direction * pieceLength;
            center = cursor + direction * (pieceLength * 0.5f);
        } else {
            const float inverseCurvature = 1.0f / curvature;
            pieceEnd = cursor + glm::vec3(
                (std::cos(heading) - std::cos(endHeading)) * inverseCurvature, 0.0f,
                (std::sin(endHeading) - std::sin(heading)) * inverseCurvature);
            center = cursor + glm::vec3(
                (std::cos(heading) - std::cos(midpointHeading)) * inverseCurvature, 0.0f,
                (std::sin(midpointHeading) - std::sin(heading)) * inverseCurvature);
        }
        const glm::vec3 midpointDirection(std::sin(midpointHeading), 0.0f, std::cos(midpointHeading));
        glm::mat4 transform = createTrackAlignedTransform(center, midpointDirection, assetScale);
        transform = glm::scale(transform, glm::vec3(1.0f, 1.0f, pieceLength / 5.0f));
        pieces.push_back({transform, cursor, pieceEnd, heading, curvature, pieceLength,
                          routeDistance + i * pieceLength});
        cursor = pieceEnd;
        heading = endHeading;
    }
    return pieces;
}

std::optional<glm::vec3> cursorGroundPosition(GLFWwindow* window, const glm::mat4& view,
                                              const glm::mat4& projection) {
    double cursorX, cursorY;
    glfwGetCursorPos(window, &cursorX, &cursorY);
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    if (width == 0 || height == 0) return std::nullopt;
    const float x = 2.0f * static_cast<float>(cursorX) / width - 1.0f;
    const float y = 1.0f - 2.0f * static_cast<float>(cursorY) / height;
    const glm::mat4 inverseViewProjection = glm::inverse(projection * view);
    glm::vec4 nearPoint = inverseViewProjection * glm::vec4(x, y, -1.0f, 1.0f);
    glm::vec4 farPoint = inverseViewProjection * glm::vec4(x, y, 1.0f, 1.0f);
    nearPoint /= nearPoint.w;
    farPoint /= farPoint.w;
    const glm::vec3 rayDirection = glm::normalize(glm::vec3(farPoint - nearPoint));
    if (std::abs(rayDirection.y) < 0.0001f) return std::nullopt;
    const float distance = -nearPoint.y / rayDirection.y;
    if (distance < 0.0f) return std::nullopt;
    return glm::vec3(nearPoint) + rayDirection * distance;
}

float customRouteLength() {
    if (customRoutePoints.size() < 2) return 0.0f;
    float length = 0.0f;
    for (size_t i = 1; i < customRoutePoints.size(); ++i) {
        length += glm::length(customRoutePoints[i] - customRoutePoints[i - 1]);
    }
    if (customRouteClosed) length += glm::length(customRoutePoints.front() - customRoutePoints.back());
    return length;
}

RouteSample sampleCustomRoute(float routePosition) {
    const float length = customRouteLength();
    if (length <= 0.0f) return {};
    float remaining = customRouteClosed ? std::fmod(std::max(routePosition, 0.0f), length)
                                        : std::clamp(routePosition, 0.0f, length);
    const size_t segmentCount = customRoutePoints.size() - 1 + (customRouteClosed ? 1 : 0);
    for (size_t i = 0; i < segmentCount; ++i) {
        const glm::vec3& start = customRoutePoints[i];
        const glm::vec3& end = customRoutePoints[(i + 1) % customRoutePoints.size()];
        const glm::vec3 offset = end - start;
        const float segmentLength = glm::length(offset);
        if (segmentLength <= 0.0001f) continue;
        if (remaining <= segmentLength || i + 1 == segmentCount) {
            return {start + offset * (remaining / segmentLength), offset / segmentLength};
        }
        remaining -= segmentLength;
    }
    return {customRoutePoints.back(), glm::normalize(customRoutePoints.back() - customRoutePoints.front())};
}

RouteSample sampleActiveRoute(float routePosition) {
    return customRoutePoints.size() >= 2 ? sampleCustomRoute(routePosition)
                                         : sampleTrackRoute(routePosition);
}

std::optional<RoutePoint> snapRoutePoint(const glm::vec3& position) {
    std::optional<RoutePoint> closest;
    float closestDistance = trackSnapDistanceMeters;
    for (size_t segmentIndex = 0; segmentIndex < trackSegments.size(); ++segmentIndex) {
        const TrackSegment& segment = trackSegments[segmentIndex];
        const glm::vec3 offset = segment.end - segment.start;
        const float squaredLength = glm::dot(offset, offset);
        if (squaredLength <= 0.0001f) continue;
        const float t = std::clamp(glm::dot(position - segment.start, offset) / squaredLength, 0.0f, 1.0f);
        const glm::vec3 projected = segment.start + offset * t;
        const float distance = glm::length(position - projected);
        if (distance < closestDistance) {
            closest = {projected, segmentIndex, t * segment.length};
            closestDistance = distance;
        }
    }
    return closest;
}

void appendRoutePoint(std::vector<glm::vec3>& points, const glm::vec3& point) {
    if (points.empty() || glm::length(points.back() - point) > 0.001f) points.push_back(point);
}

void appendSegmentPath(std::vector<glm::vec3>& points, size_t segmentIndex,
                       float fromDistance, float toDistance) {
    const TrackSegment& segment = trackSegments[segmentIndex];
    const float distance = std::abs(toDistance - fromDistance);
    const int stepCount = std::max(1, static_cast<int>(std::ceil(distance)));
    for (int step = 0; step <= stepCount; ++step) {
        const float fraction = static_cast<float>(step) / stepCount;
        const float localDistance = fromDistance + (toDistance - fromDistance) * fraction;
        const float heading = segment.startHeadingRadians +
            segment.curvatureRadiansPerMeter * localDistance;
        glm::vec3 point;
        if (std::abs(segment.curvatureRadiansPerMeter) < 0.000001f) {
            point = segment.start + glm::vec3(std::sin(heading), 0.0f, std::cos(heading)) * localDistance;
        } else {
            const float inverseCurvature = 1.0f / segment.curvatureRadiansPerMeter;
            point = segment.start + glm::vec3(
                (std::cos(segment.startHeadingRadians) - std::cos(heading)) * inverseCurvature, 0.0f,
                (std::sin(heading) - std::sin(segment.startHeadingRadians)) * inverseCurvature);
        }
        appendRoutePoint(points, point);
    }
}

// Finds the shortest connected rail path instead of relying on creation order.
// Each segment endpoint is a graph node; coincident endpoints form a junction.
bool appendTrackPath(std::vector<glm::vec3>& points, const RoutePoint& from, const RoutePoint& to) {
    if (from.segmentIndex >= trackSegments.size() || to.segmentIndex >= trackSegments.size()) return false;
    if (from.segmentIndex == to.segmentIndex) {
        appendSegmentPath(points, from.segmentIndex, from.distanceAlongSegment, to.distanceAlongSegment);
        return true;
    }

    struct Edge { size_t node; float cost; std::optional<size_t> segmentIndex; };
    const size_t nodeCount = trackSegments.size() * 2;
    std::vector<std::vector<Edge>> graph(nodeCount);
    for (size_t i = 0; i < trackSegments.size(); ++i) {
        const size_t startNode = i * 2, endNode = startNode + 1;
        graph[startNode].push_back({endNode, trackSegments[i].length, i});
        graph[endNode].push_back({startNode, trackSegments[i].length, i});
    }
    for (size_t first = 0; first < nodeCount; ++first) {
        const glm::vec3 firstPosition = first % 2 == 0 ? trackSegments[first / 2].start : trackSegments[first / 2].end;
        for (size_t second = first + 1; second < nodeCount; ++second) {
            const glm::vec3 secondPosition = second % 2 == 0 ? trackSegments[second / 2].start : trackSegments[second / 2].end;
            if (glm::length(firstPosition - secondPosition) <= routeJunctionToleranceMeters) {
                graph[first].push_back({second, 0.0f, std::nullopt});
                graph[second].push_back({first, 0.0f, std::nullopt});
            }
        }
    }

    const TrackSegment& fromSegment = trackSegments[from.segmentIndex];
    const TrackSegment& toSegment = trackSegments[to.segmentIndex];
    const float infinity = std::numeric_limits<float>::infinity();
    std::vector<float> distances(nodeCount, infinity);
    std::vector<size_t> previous(nodeCount, nodeCount);
    std::vector<std::optional<size_t>> previousSegment(nodeCount);
    using QueueEntry = std::pair<float, size_t>;
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, std::greater<QueueEntry>> queue;
    const std::array<std::pair<size_t, float>, 2> sources{{
        {from.segmentIndex * 2, from.distanceAlongSegment},
        {from.segmentIndex * 2 + 1, fromSegment.length - from.distanceAlongSegment}
    }};
    for (const auto& [node, cost] : sources) { distances[node] = cost; queue.push({cost, node}); }
    while (!queue.empty()) {
        const auto [distance, node] = queue.top(); queue.pop();
        if (distance != distances[node]) continue;
        for (const Edge& edge : graph[node]) {
            if (distance + edge.cost < distances[edge.node]) {
                distances[edge.node] = distance + edge.cost;
                previous[edge.node] = node;
                previousSegment[edge.node] = edge.segmentIndex;
                queue.push({distances[edge.node], edge.node});
            }
        }
    }
    const size_t toStart = to.segmentIndex * 2, toEnd = toStart + 1;
    const float viaStart = distances[toStart] + to.distanceAlongSegment;
    const float viaEnd = distances[toEnd] + toSegment.length - to.distanceAlongSegment;
    const size_t destination = viaStart <= viaEnd ? toStart : toEnd;
    if (!std::isfinite(distances[destination])) return false;

    std::vector<size_t> nodes;
    for (size_t node = destination; node != nodeCount; node = previous[node]) nodes.push_back(node);
    std::reverse(nodes.begin(), nodes.end());
    appendSegmentPath(points, from.segmentIndex, from.distanceAlongSegment,
                      nodes.front() % 2 == 0 ? 0.0f : fromSegment.length);
    for (size_t i = 1; i < nodes.size(); ++i) {
        if (previousSegment[nodes[i]]) {
            const size_t segmentIndex = *previousSegment[nodes[i]];
            appendSegmentPath(points, segmentIndex,
                              nodes[i - 1] % 2 == 0 ? 0.0f : trackSegments[segmentIndex].length,
                              nodes[i] % 2 == 0 ? 0.0f : trackSegments[segmentIndex].length);
        } else {
            appendRoutePoint(points, nodes[i] % 2 == 0 ? trackSegments[nodes[i] / 2].start
                                                       : trackSegments[nodes[i] / 2].end);
        }
    }
    appendSegmentPath(points, to.segmentIndex, destination % 2 == 0 ? 0.0f : toSegment.length,
                      to.distanceAlongSegment);
    return true;
}


// Finds the shortest connected rail path instead of relying on creation order.
// Each segment endpoint is a graph node; coincident endpoints form a junction.

void clearCustomRoute() {
    customRoutePoints.clear();
    customRouteStopPositions.clear();
    customRouteClosed = false;
    customRouteFirstTrackPoint.reset();
    customRouteLastTrackPoint.reset();
    customRouteChanged = true;
}

void applyCustomRouteToTrains() {
    if (!customRouteChanged) return;
    const bool hasCustomRoute = customRoutePoints.size() >= 2;
    const float length = hasCustomRoute ? customRouteLength()
        : (trackSegments.empty() ? 0.0f : trackSegments.back().distanceFromRouteStart + trackSegments.back().length);
    for (Model& train : trains) {
        train.routeDistance = length;
        train.routePosition = 0.0f;
        train.previousRoutePosition = 0.0f;
        train.routeVelocity = 0.0f;
        train.motionDirection = 1.0f;
        train.routeStopTimer = 0.0f;
        if (length > 0.0f) {
            const RouteSample sample = hasCustomRoute ? sampleCustomRoute(0.0f) : sampleTrackRoute(0.0f);
            train.routeDirection = sample.direction;
            train.position = sample.position;
            train.position.y = train.routeStart.y;
            train.transform = createTrackAlignedTransform(train.position, train.routeDirection, train.scale);
            train.transform *= train.sourceTransform;
        }
    }
    customRouteChanged = false;
}

void updateTrainMotion(Model& train, float dt) {
    if (train.routeDistance <= 0.0f || dt <= 0.0f) return;

    const float fallbackMaxSpeed = 12.0f;
    const float maxSpeed = train.maxSpeedKmh > 0.0f ? train.maxSpeedKmh / 3.6f : fallbackMaxSpeed;
    const float acceleration = train.accelerationMs2 > 0.0f ? train.accelerationMs2 : 0.5f;
    const float brakeDeceleration = train.brakeDecelerationMs2 > 0.0f ? train.brakeDecelerationMs2 : acceleration;
    const bool isClosedRoute = customRouteClosed && customRoutePoints.size() >= 3;
    if (train.routeStopTimer > 0.0f) {
        train.previousRoutePosition = train.routePosition;
        train.routeVelocity = 0.0f;
        train.routeStopTimer = std::max(0.0f, train.routeStopTimer - dt);
        train.engineThrottle = 0.0f;
        return;
    }

    float targetPosition = train.motionDirection > 0.0f ? train.routeDistance : 0.0f;
    bool targetIsReversalStop = false;
    if (!isClosedRoute) {
        if (train.motionDirection > 0.0f) {
            for (const float stopPosition : customRouteStopPositions) {
                if (stopPosition > train.routePosition + 0.001f) {
                    targetPosition = stopPosition;
                    targetIsReversalStop = true;
                    break;
                }
            }
        } else {
            for (auto stop = customRouteStopPositions.rbegin(); stop != customRouteStopPositions.rend(); ++stop) {
                if (*stop < train.routePosition - 0.001f) {
                    targetPosition = *stop;
                    targetIsReversalStop = true;
                    break;
                }
            }
        }
    }
    const float distanceToTarget = std::abs(targetPosition - train.routePosition);
    const float stoppingDistance = (train.routeVelocity * train.routeVelocity) / (2.0f * brakeDeceleration);

    float signedAcceleration = 0.0f;
    if (isClosedRoute) {
        if (std::abs(train.routeVelocity) < maxSpeed) {
            signedAcceleration = acceleration * train.motionDirection;
        }
    } else if (distanceToTarget <= 0.02f && std::abs(train.routeVelocity) < 0.05f) {
        train.routePosition = targetPosition;
        train.routeVelocity = 0.0f;
        if (targetIsReversalStop) {
            train.routeStopTimer = routeReversalStopDurationSeconds;
        } else {
            train.motionDirection *= -1.0f;
        }
    } else if (stoppingDistance >= distanceToTarget) {
        signedAcceleration = -brakeDeceleration * train.motionDirection;
    } else if (std::abs(train.routeVelocity) < maxSpeed) {
        signedAcceleration = acceleration * train.motionDirection;
    }

    train.routeVelocity += signedAcceleration * dt;
    if (train.routeVelocity * train.motionDirection < 0.0f) train.routeVelocity = 0.0f;
    train.routeVelocity = std::clamp(train.routeVelocity, -maxSpeed, maxSpeed);
    train.previousRoutePosition = train.routePosition;
    train.routePosition += train.routeVelocity * dt;
    if (targetIsReversalStop &&
        ((train.motionDirection > 0.0f && train.routePosition >= targetPosition) ||
         (train.motionDirection < 0.0f && train.routePosition <= targetPosition))) {
        train.routePosition = targetPosition;
        train.routeVelocity = 0.0f;
        train.routeStopTimer = routeReversalStopDurationSeconds;
    }
    if (isClosedRoute) {
        train.routePosition = std::fmod(train.routePosition, train.routeDistance);
        if (train.routePosition < 0.0f) train.routePosition += train.routeDistance;
    } else {
        train.routePosition = std::clamp(train.routePosition, 0.0f, train.routeDistance);
    }
    train.engineThrottle = signedAcceleration * train.motionDirection > 0.0f ? 1.0f
        : (signedAcceleration * train.motionDirection < 0.0f ? 0.15f : 0.45f);

    const RouteSample sample = sampleActiveRoute(train.routePosition);
    train.routeDirection = sample.direction;
    train.position = sample.position;
    train.position.y = train.routeStart.y;
    train.transform = createTrackAlignedTransform(train.position, train.routeDirection, train.scale);
    train.transform *= train.sourceTransform;
}

void updateTrackJointSounds(const TrainAudioSystem& audioSystem) {
    if (jointSoundBuffer == 0 || jointSoundDurationSeconds <= 0.0f) return;

    audioSystem.cleanupStoppedSources(activeJointSoundSources);

    for (const Model& train : trains) {
        if (train.routeDistance <= 0.0f || train.previousRoutePosition == train.routePosition) continue;

        const float from = std::min(train.previousRoutePosition, train.routePosition);
        const float to = std::max(train.previousRoutePosition, train.routePosition);
        const float speedMs = std::abs(train.routeVelocity);
        const float speedKmh = speedMs * 3.6f;

        for (const TrackJointSound& joint : trackJointSounds) {
            if (joint.routePosition < from || joint.routePosition > to) continue;

            // РЕАЛИСТИЧНЫЙ ЗВУК СТЫКОВ

            // 1. Громкость: растет со скоростью, но с насыщением
            float volume = 0.0f;
            if (speedKmh < 5.0f) {
                volume = 0.05f;
            } else if (speedKmh < 80.0f) {
                volume = 0.1f + (speedKmh - 5.0f) / 75.0f * 0.8f;
            } else {
                volume = 0.9f;
            }
            volume = std::clamp(volume, 0.0f, 1.0f);

            // 2. Pitch: почти не меняется
            float pitch = 1.0f;

            // 3. Случайность
            static unsigned int seed = 12345;
            seed = (seed * 1103515245 + 12345) & 0x7fffffff;
            float randomFactor = 0.85f + (seed % 30) / 100.0f;
            pitch *= randomFactor;

            // 4. Эффект Доплера
            if (speedKmh > 50.0f) {
                pitch += (speedKmh - 50.0f) / 200.0f * 0.05f;
            }

            // Воспроизводим основной звук
            activeJointSoundSources.push_back(
                audioSystem.playOneShot(jointSoundBuffer, joint.position, pitch, volume)
            );
        }
    }
}

int main() {
    // Инициализация GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Train Sim Engine", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.53f, 0.81f, 0.92f, 1.0f);

    TrainAudioSystem trainAudio;
    const bool audioAvailable = trainAudio.initialize();

    unsigned int modelShader = createShaderProgram(modelVertexShader, modelFragmentShader);
    unsigned int planeShader = createShaderProgram(planeVertexShader, planeFragmentShader);
    unsigned int previewShader = createShaderProgram(previewVertexShader, previewFragmentShader);

    initPlane();

    // Загрузка модели рельсов
    constexpr int trackPieceCount = 100;
    constexpr float trackPieceLength = 5.0f;
    constexpr float assetScale = 0.5f;
    Model rail = loadModel("rails/5m_track.glb", "5m_track");

    if (rail.meshes.empty() || rail.markers.count("track_start") == 0 ||
        rail.markers.count("track_end") == 0) {
        std::cout << "Rail model or its markers were not found" << std::endl;
    } else if (!loadTrackMap(assetScale)) {
        glm::vec3 cursor(0.0f, 0.0f, 0.0f);
        float headingRadians = 0.0f;
        float routeDistance = 0.0f;
        const float turnRadiansPerPiece = 0.04f;
        const int curveStart = 30;
        const int curveEnd = 70;

        for (int i = 0; i < trackPieceCount; ++i) {
            const float curvature = (i >= curveStart && i < curveEnd)
                ? turnRadiansPerPiece / trackPieceLength : 0.0f;
            const float endHeadingRadians = headingRadians + curvature * trackPieceLength;
            const float midpointHeadingRadians = (headingRadians + endHeadingRadians) * 0.5f;
            const glm::vec3 start = cursor;
            glm::vec3 end;
            glm::vec3 center;

            if (curvature == 0.0f) {
                const glm::vec3 direction(std::sin(headingRadians), 0.0f, std::cos(headingRadians));
                end = start + direction * trackPieceLength;
                center = start + direction * (0.5f * trackPieceLength);
            } else {
                const float inverseCurvature = 1.0f / curvature;
                end = start + glm::vec3(
                    (std::cos(headingRadians) - std::cos(endHeadingRadians)) * inverseCurvature,
                    0.0f,
                    (std::sin(endHeadingRadians) - std::sin(headingRadians)) * inverseCurvature);
                center = start + glm::vec3(
                    (std::cos(headingRadians) - std::cos(midpointHeadingRadians)) * inverseCurvature,
                    0.0f,
                    (std::sin(midpointHeadingRadians) - std::sin(headingRadians)) * inverseCurvature);
            }

            const glm::vec3 midpointDirection(
                std::sin(midpointHeadingRadians), 0.0f, std::cos(midpointHeadingRadians));
            const glm::mat4 instanceTransform = createTrackAlignedTransform(center, midpointDirection, assetScale);

            trackSegments.push_back({
                instanceTransform,
                start,
                end,
                headingRadians,
                curvature,
                trackPieceLength,
                routeDistance
            });

            cursor = end;
            routeDistance += trackPieceLength;
            headingRadians = endHeadingRadians;
        }

        std::cout << "Built test track: " << routeDistance
                  << " m (" << trackSegments.size() << " connected pieces)" << std::endl;

    }

    if (!trackSegments.empty()) {
        const float routeDistance = trackSegments.back().distanceFromRouteStart + trackSegments.back().length;
        for (float routePosition = jointSoundIntervalMeters;
             routePosition < routeDistance;
             routePosition += jointSoundIntervalMeters) {
            const RouteSample jointSample = sampleTrackRoute(routePosition);
            trackJointSounds.push_back({jointSample.position, routePosition});
        }
        std::cout << "Prepared " << trackJointSounds.size() << " rail joint sound points" << std::endl;
    }

    if (audioAvailable) {
        const fs::path jointSoundPath = fs::exists("rails/joint.wav")
            ? fs::path("rails/joint.wav")
            : fs::path("rails/joint.mp3");
        trainAudio.loadOneShotBuffer(jointSoundPath, jointSoundBuffer, jointSoundDurationSeconds);
    }

    // Загрузка поездов
    const std::vector<fs::path> modelPaths = findTrainModelPaths();
    if (!modelPaths.empty()) {
        for (const fs::path& modelPath : modelPaths) {
            const fs::path trainDirectory = modelPath.parent_path();
            const auto configuration = loadTrainConfiguration(trainDirectory);
            const std::string trainName = configuration ? configuration->name : trainDirectory.filename().string();
            std::cout << "Loading model from: " << modelPath << std::endl;
            Model train = loadModel(modelPath.string(), trainName);

            if (train.meshes.empty()) continue;
            if (configuration) {
                train.type = configuration->type;
                train.description = configuration->description;
                train.weightTonnes = configuration->weightTonnes;
                train.maxSpeedKmh = configuration->maxSpeedKmh;
                train.accelerationMs2 = configuration->accelerationMs2;
                train.brakeDecelerationMs2 = configuration->brakeDecelerationMs2;
                train.enginePowerKw = configuration->enginePowerKw;
                train.tractionForceKn = configuration->tractionForceKn;
                train.fuelCapacityL = configuration->fuelCapacityL;
                train.fuelConsumptionLPerKm = configuration->fuelConsumptionLPerKm;
                std::cout << "  Configuration: " << configuration->type << ", "
                          << configuration->weightTonnes << " t, maximum "
                          << configuration->maxSpeedKmh << " km/h" << std::endl;
            }

            const glm::vec3 pivotBack = train.markers.count("pivot_back")
                ? train.markers["pivot_back"] : glm::vec3(0.0f);
            const float lineHeight = trackSegments.empty() ? 0.0f : trackSegments.front().start.y;
            train.routeStart = trackSegments.empty() ? glm::vec3(0.0f) : trackSegments.front().start;
            train.routeEnd = trackSegments.size() > 1 ? trackSegments.back().end
                : (trackSegments.empty() ? glm::vec3(0.0f, 0.0f, trackPieceLength) : trackSegments.back().end);
            train.routeStart.y = lineHeight - assetScale * pivotBack.y;
            train.routeEnd.y = train.routeStart.y;
            train.routeDistance = trackSegments.empty()
                ? 0.0f
                : trackSegments.back().distanceFromRouteStart + trackSegments.back().length;
            train.routePosition = 0.0f;
            train.previousRoutePosition = train.routePosition;

            const RouteSample initialSample = sampleTrackRoute(train.routePosition);
            train.routeDirection = initialSample.direction;
            train.position = initialSample.position;
            train.position.y = train.routeStart.y;
            train.scale = assetScale;
            train.transform = createTrackAlignedTransform(train.position, train.routeDirection, train.scale);
            train.transform *= train.sourceTransform;

            if (audioAvailable && configuration && !configuration->engineSound.empty()) {
                const fs::path soundPath = trainDirectory / "sounds" / configuration->engineSound;
                trainAudio.startLoopingEngine(train, soundPath);
            }
            trains.push_back(std::move(train));
            std::cout << "✓ Successfully loaded train: " << trainName << std::endl;
        }
    } else {
        std::cout << "No train model found in trains/ folder!" << std::endl;
    }

    std::cout << "=== TRAIN SIMULATOR ENGINE ===" << std::endl;
    std::cout << "Controls: WASD - move, Mouse - look, E/Q - up/down, H - track builder, ESC - exit" << std::endl;
    std::cout << "Loaded " << trains.size() << " train(s)" << std::endl;

    // Главный цикл
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Управление камерой
        float speed = 10.0f * deltaTime;
        glm::vec3 cameraFrontHorizontal = glm::normalize(glm::vec3(cameraFront.x, 0.0f, cameraFront.z));
        glm::vec3 cameraRight = glm::normalize(glm::cross(cameraFrontHorizontal, cameraUp));

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            cameraPos += speed * cameraFrontHorizontal;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            cameraPos -= speed * cameraFrontHorizontal;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            cameraPos -= speed * cameraRight;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            cameraPos += speed * cameraRight;
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
            cameraPos.y += speed;
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
            cameraPos.y -= speed;
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            saveTrackMap();
            glfwSetWindowShouldClose(window, true);
        }

        const bool hPressed = glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS;
        if (hPressed && !previousHPressed) {
            trackBuildMode = !trackBuildMode;
            if (trackBuildMode) routeBuildMode = false;
            trackBuildStart.reset();
            trackBuildStartHeading.reset();
            trackBuildStartIsConnected = false;
            glfwSetInputMode(window, GLFW_CURSOR,
                             trackBuildMode ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
            firstMouse = true;
            std::cout << (trackBuildMode ? "Track builder: I - straight, J - curve, left click - select/build, right click - cancel"
                                         : "Track builder closed") << std::endl;
        }
        previousHPressed = hPressed;

        const bool pPressed = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
        if (pPressed && !previousPPressed) {
            routeBuildMode = !routeBuildMode;
            if (routeBuildMode) trackBuildMode = false;
            glfwSetInputMode(window, GLFW_CURSOR,
                             (trackBuildMode || routeBuildMode) ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
            firstMouse = true;
            std::cout << (routeBuildMode
                ? "Route builder: left click on rails to trace them; click the first point to close; X clears the route"
                : "Route builder closed") << std::endl;
        }
        previousPPressed = pPressed;

        const bool xPressed = glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS;
        if (xPressed && !previousXPressed) {
            clearCustomRoute();
            std::cout << "Custom route cleared" << std::endl;
        }
        previousXPressed = xPressed;

        if (trackBuildMode) {
            const bool iPressed = glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS;
            const bool jPressed = glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS;
            if (iPressed && !previousIPressed) trackBuildTool = TrackBuildTool::Straight;
            if (jPressed && !previousJPressed) trackBuildTool = TrackBuildTool::Curve;
            previousIPressed = iPressed;
            previousJPressed = jPressed;
        }

        // Рендеринг
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        trainAudio.updateListener(cameraPos, cameraFront, cameraUp);

        applyCustomRouteToTrains();
        for (auto& train : trains) {
            updateTrainMotion(train, deltaTime);
            trainAudio.updateEngine(train, cameraPos);
        }
        updateTrackJointSounds(trainAudio);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 800.0f);

        std::vector<glm::vec3> previewPoints;
        // The blue line is an editor aid, not part of the normal simulation.
        // Do not leave it over the rails and train after P closes the editor.
        std::vector<glm::vec3> routePreviewPoints;
        if (routeBuildMode) {
            routePreviewPoints = customRoutePoints;
            const auto cursorPosition = cursorGroundPosition(window, view, projection);
            const bool leftPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            const bool rightPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
            if (rightPressed && !previousRightMousePressed && !customRoutePoints.empty()) {
                clearCustomRoute();
            }
            if (cursorPosition) {
                const auto snappedPoint = snapRoutePoint(*cursorPosition);
                if (snappedPoint) {
                    if (leftPressed && !previousRouteLeftMousePressed) {
                        if (!customRouteClosed && customRouteFirstTrackPoint && customRouteLastTrackPoint &&
                            glm::length(snappedPoint->position - customRoutePoints.front()) < routeCloseSnapDistanceMeters) {
                            if (appendTrackPath(customRoutePoints, *customRouteLastTrackPoint,
                                                *customRouteFirstTrackPoint)) {
                                customRouteClosed = true;
                                customRouteChanged = true;
                            } else {
                                std::cout << "Cannot close route: rails are not connected" << std::endl;
                            }
                        } else {
                            // Opening the editor must not erase a finished route.  Start a
                            // replacement only after the user actually chooses its first point.
                            if (customRouteClosed) clearCustomRoute();
                            if (!customRouteLastTrackPoint) {
                                customRoutePoints.push_back(snappedPoint->position);
                                customRouteFirstTrackPoint = snappedPoint;
                                customRouteLastTrackPoint = snappedPoint;
                                customRouteChanged = true;
                            } else {
                                const size_t routePointCount = customRoutePoints.size();
                                const float connectionPosition = customRouteLength();
                                if (!appendTrackPath(customRoutePoints, *customRouteLastTrackPoint,
                                                     *snappedPoint)) {
                                    std::cout << "Cannot extend route: rails are not connected" << std::endl;
                                } else {
                                    customRouteLastTrackPoint = snappedPoint;
                                    customRouteChanged = true;
                                    if (customRoutePoints.size() > routePointCount) {
                                        customRouteStopPositions.push_back(connectionPosition);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            previousRouteLeftMousePressed = leftPressed;
            previousRightMousePressed = rightPressed;
        } else {
            previousRouteLeftMousePressed = false;
        }
        if (trackBuildMode) {
            const auto cursorPosition = cursorGroundPosition(window, view, projection);
            const bool leftPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            const bool rightPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
            if (rightPressed && !previousRightMousePressed) {
                trackBuildStart.reset();
                trackBuildStartHeading.reset();
                trackBuildStartIsConnected = false;
            }
            if (cursorPosition) {
                const TrackConnection target = snapTrackConnection(*cursorPosition);
                if (leftPressed && !previousLeftMousePressed) {
                    if (!trackBuildStart) {
                        trackBuildStart = target.position;
                        trackBuildStartHeading = target.heading;
                        trackBuildStartIsConnected = target.isConnected;
                    } else {
                        const float routeDistance = trackSegments.empty() ? 0.0f
                            : trackSegments.back().distanceFromRouteStart + trackSegments.back().length;
                        std::vector<TrackSegment> pieces;
                        if (!(trackBuildTool == TrackBuildTool::Straight &&
                              trackBuildStartIsConnected && target.isConnected)) {
                            pieces = makeTrackPieces(
                                *trackBuildStart, target.position, trackBuildStartHeading,
                                trackBuildTool, target.heading, assetScale, routeDistance);
                        }
                        if (!pieces.empty()) {
                            const TrackSegment& lastPiece = pieces.back();
                            trackBuildStart = lastPiece.end;
                            trackBuildStartHeading = lastPiece.startHeadingRadians +
                                lastPiece.curvatureRadiansPerMeter * lastPiece.length;
                            trackBuildStartIsConnected = true;
                            trackSegments.insert(trackSegments.end(), pieces.begin(), pieces.end());
                        }
                    }
                }
                if (trackBuildStart) {
                    const float routeDistance = trackSegments.empty() ? 0.0f
                        : trackSegments.back().distanceFromRouteStart + trackSegments.back().length;
                    const std::vector<TrackSegment> previewPieces =
                        trackBuildTool == TrackBuildTool::Straight && trackBuildStartIsConnected &&
                            target.isConnected
                        ? std::vector<TrackSegment>{}
                        : makeTrackPieces(*trackBuildStart, target.position, trackBuildStartHeading,
                                          trackBuildTool, target.heading, assetScale, routeDistance);
                    for (const TrackSegment& piece : previewPieces) {
                        if (previewPoints.empty()) previewPoints.push_back(piece.start + glm::vec3(0.0f, 0.03f, 0.0f));
                        previewPoints.push_back(piece.end + glm::vec3(0.0f, 0.03f, 0.0f));
                    }
                }
            }
            previousLeftMousePressed = leftPressed;
            previousRightMousePressed = rightPressed;
        } else {
            previousLeftMousePressed = false;
            previousRightMousePressed = false;
        }

        // Плоскость
        glUseProgram(planeShader);
        glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(planeShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(planeShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(planeShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        drawPlane();

        // Рельсы и поезда
        glUseProgram(modelShader);
        glm::vec3 lightPos = glm::vec3(5.0f, 20.0f, 5.0f);
        glUniform3fv(glGetUniformLocation(modelShader, "lightPos"), 1, glm::value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(modelShader, "viewPos"), 1, glm::value_ptr(cameraPos));
        glUniformMatrix4fv(glGetUniformLocation(modelShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(modelShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        // Рисуем рельсы
        for (const auto& segment : trackSegments) {
            const glm::mat4 railTransform = segment.transform * rail.sourceTransform;
            glUniformMatrix4fv(glGetUniformLocation(modelShader, "model"), 1, GL_FALSE,
                               glm::value_ptr(railTransform));
            rail.draw(modelShader);
        }


        drawTrackPreview(previewShader, previewPoints, view, projection, glm::vec3(0.1f, 1.0f, 0.15f));
        if (!routePreviewPoints.empty()) {
            if (customRouteClosed) routePreviewPoints.push_back(routePreviewPoints.front());
            for (glm::vec3& point : routePreviewPoints) point.y += 0.12f;
            drawTrackPreview(previewShader, routePreviewPoints, view, projection, glm::vec3(0.1f, 0.4f, 1.0f));
        }

        // Рисуем поезда
        for (auto& train : trains) {
            glUniformMatrix4fv(glGetUniformLocation(modelShader, "model"), 1, GL_FALSE, glm::value_ptr(train.transform));
            train.draw(modelShader);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Очистка
    for (auto& train : trains) {
        trainAudio.release(train);
        train.cleanup();
    }
    trainAudio.releaseSources(activeJointSoundSources);
    trainAudio.releaseBuffer(jointSoundBuffer);
    trainAudio.shutdown();
    rail.cleanup();
    glDeleteVertexArrays(1, &planeVAO);
    glDeleteBuffers(1, &planeVBO);
    glDeleteBuffers(1, &planeEBO);
    glDeleteProgram(modelShader);
    glDeleteProgram(planeShader);
    glDeleteProgram(previewShader);
    glDeleteVertexArrays(1, &previewVAO);
    glDeleteBuffers(1, &previewVBO);

    glfwTerminate();
    return 0;
}
