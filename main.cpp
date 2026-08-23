#define GLAD_GL_IMPLEMENTATION
#include "include/glad/glad.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
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
        // The GLB base-colour map is the diffuse texture.  Always bind it to
        // the sampler the shader actually uses, rather than relying on the
        // OpenGL default texture unit left by a previous mesh.
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
    // Transform of the mesh node in the source glTF.  The track mesh is
    // offset from its start/end helper nodes, so dropping this transform
    // makes the visible rail disagree with the route.
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
    float engineThrottle = 0.0f;
    float scale = 0.5f;
    unsigned int engineSoundSource = 0;
    unsigned int engineSoundBuffer = 0;

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

// A configuration lives next to a model so every train folder is self-contained.
// Keep the historic `confg.json` spelling as a supported alias for existing assets.
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
        if (!readWave(soundPath, pcm, format, sampleRate)) {
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

    static bool readWave(const fs::path& path, std::vector<char>& pcm, ALenum& format, ALsizei& sampleRate) {
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
        // OpenAL only spatializes mono buffers.  Downmix supplied stereo idle
        // recordings so the source really belongs to the locomotive and is
        // attenuated as the camera moves away.
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
        return true;
    }
};
#else
class TrainAudioSystem {
public:
    bool initialize() { std::cout << "Train sounds require an OpenAL-enabled build" << std::endl; return false; }
    bool startLoopingEngine(Model&, const fs::path&) { return false; }
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
};

std::vector<TrackSegment> trackSegments;

// Обработчики GLFW
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn) {
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

    // PNG/JPEG rows are tightly packed.  Without this, RGB images whose row
    // width is not a multiple of four get corrupted when uploaded to OpenGL.
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
        // Сжатая текстура (PNG, JPG и т.д.)
        data = stbi_load_from_memory(
            reinterpret_cast<const unsigned char*>(embeddedTexture->pcData),
            embeddedTexture->mWidth,
            &width, &height, &channels, 0
        );
    } else {
        // Несжатая текстура
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

        // Проверяем, не загружена ли уже эта текстура
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

            // Проверяем, встроенная ли текстура
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

    // Вершины
    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        result.vertices.push_back(glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z));
        if (mesh->HasNormals()) {
            result.normals.push_back(glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z));
        }
        if (mesh->mTextureCoords[0]) {
            result.texCoords.push_back(glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y));
        }
    }

    // Индексы
    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++) {
            result.indices.push_back(face.mIndices[j]);
        }
    }

    // Материалы и текстуры
    if (mesh->mMaterialIndex >= 0) {
        aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];

        // glTF stores Blender's Principled BSDF colour map in the PBR
        // base-colour slot, not in Assimp's legacy diffuse slot.  Reading the
        // base-colour slot first keeps every primitive paired with the texture
        // assigned to its material in Blender.  Keep the diffuse lookup as a
        // fallback for OBJ and older importers.
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

// Загрузка модели из файла
Model loadModel(const std::string& path, const std::string& name) {
    Model model;
    model.name = name;

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        // Assimp converts the glTF texture-coordinate convention on import;
        // flip it back for OpenGL's texture upload convention.
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

// Шейдер для модели с текстурами
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
    uniform sampler2D texture_specular1;
    uniform sampler2D texture_normal1;
    uniform bool hasTexture;

    void main() {
        vec3 color;
        if (hasTexture) {
            color = texture(texture_diffuse1, TexCoord).rgb;
        } else {
            color = vec3(0.8f, 0.2f, 0.2f);
        }

        // Ambient
        float ambientStrength = 0.3f;
        vec3 ambient = ambientStrength * color;

        // Diffuse
        vec3 norm = normalize(Normal);
        vec3 lightDir = normalize(lightPos - FragPos);
        float diff = max(dot(norm, lightDir), 0.0f);
        vec3 diffuse = diff * color;

        // Specular
        float specularStrength = 0.5f;
        vec3 viewDir = normalize(viewPos - FragPos);
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0f), 32);
        vec3 specular = specularStrength * spec * vec3(1.0f, 1.0f, 1.0f);

        vec3 result = ambient + diffuse + specular;
        FragColor = vec4(result, 1.0f);
    }
)";

// Шейдер для плоскости
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

// Глобальные переменные для плоскости
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

// Finds every train model, rather than choosing whichever directory happens to
// be returned first by the filesystem.
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

void updateTrainMotion(Model& train, float dt) {
    if (train.routeDistance <= 0.0f || dt <= 0.0f) return;

    const float fallbackMaxSpeed = 12.0f;
    const float maxSpeed = train.maxSpeedKmh > 0.0f ? train.maxSpeedKmh / 3.6f : fallbackMaxSpeed;
    const float acceleration = train.accelerationMs2 > 0.0f ? train.accelerationMs2 : 0.5f;
    const float brakeDeceleration = train.brakeDecelerationMs2 > 0.0f ? train.brakeDecelerationMs2 : acceleration;
    const float targetPosition = train.motionDirection > 0.0f ? train.routeDistance : 0.0f;
    const float distanceToTarget = std::abs(targetPosition - train.routePosition);
    const float stoppingDistance = (train.routeVelocity * train.routeVelocity) / (2.0f * brakeDeceleration);

    float signedAcceleration = 0.0f;
    if (distanceToTarget <= 0.02f && std::abs(train.routeVelocity) < 0.05f) {
        train.routePosition = targetPosition;
        train.routeVelocity = 0.0f;
        train.motionDirection *= -1.0f;
    } else if (stoppingDistance >= distanceToTarget) {
        signedAcceleration = -brakeDeceleration * train.motionDirection;
    } else if (std::abs(train.routeVelocity) < maxSpeed) {
        signedAcceleration = acceleration * train.motionDirection;
    }

    train.routeVelocity += signedAcceleration * dt;
    if (train.routeVelocity * train.motionDirection < 0.0f) train.routeVelocity = 0.0f;
    train.routeVelocity = std::clamp(train.routeVelocity, -maxSpeed, maxSpeed);
    train.routePosition = std::clamp(train.routePosition + train.routeVelocity * dt, 0.0f, train.routeDistance);
    train.engineThrottle = signedAcceleration * train.motionDirection > 0.0f ? 1.0f
        : (signedAcceleration * train.motionDirection < 0.0f ? 0.15f : 0.45f);

    train.position = train.routeStart + train.routeDirection * train.routePosition;
    train.transform = glm::translate(glm::mat4(1.0f), train.position);
    if (train.motionDirection < 0.0f) {
        train.transform = glm::rotate(train.transform, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    }
    train.transform = glm::scale(train.transform, glm::vec3(train.scale));
    train.transform *= train.sourceTransform;
}

int main() {
    // Инициализация GLFW
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Train Sim Engine - Textured Train", NULL, NULL);
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
    //glEnable(GL_CULL_FACE);
    glClearColor(0.53f, 0.81f, 0.92f, 1.0f);

    TrainAudioSystem trainAudio;
    const bool audioAvailable = trainAudio.initialize();

    // Создаем шейдеры
    unsigned int modelShader = createShaderProgram(modelVertexShader, modelFragmentShader);
    unsigned int planeShader = createShaderProgram(planeVertexShader, planeFragmentShader);

    // Инициализируем плоскость
    initPlane();

    // Blender assets use two source units per metre.  A 5 m rail piece has
    // endpoints 10 source units apart, therefore the common scale below
    // produces a 50 m route from ten connected pieces.
    constexpr int trackPieceCount = 10;
    constexpr float trackPieceLength = 5.0f;
    constexpr float assetScale = 0.5f;
    Model rail = loadModel("rails/5m_track.glb", "5m_track");
    if (rail.meshes.empty() || rail.markers.count("track_start") == 0 ||
        rail.markers.count("track_end") == 0) {
        std::cout << "Rail model or its track_start/track_end markers were not found" << std::endl;
    } else {
        const glm::vec3 firstPieceCenter(0.0f, 0.0f,
            -0.5f * trackPieceLength * (trackPieceCount - 1));
        for (int i = 0; i < trackPieceCount; ++i) {
            const glm::vec3 center = firstPieceCenter + glm::vec3(0.0f, 0.0f, i * trackPieceLength);
            const glm::mat4 instanceTransform = glm::scale(
                glm::translate(glm::mat4(1.0f), center), glm::vec3(assetScale));
            trackSegments.push_back({
                instanceTransform,
                glm::vec3(instanceTransform * glm::vec4(rail.markers["track_start"], 1.0f)),
                glm::vec3(instanceTransform * glm::vec4(rail.markers["track_end"], 1.0f))
            });
        }
        std::cout << "Built test track: " << trackSegments.size() * trackPieceLength
                  << " m (" << trackSegments.size() << " connected pieces)" << std::endl;
    }

    // Each directory below trains/ is an independent train package.  Its model,
    // configuration and sound are therefore applied to all loaded locomotives.
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
            // Put the locomotive onto the route by matching its Pivot_back to
            // the straight line defined by the rail endpoint markers.
            const glm::vec3 pivotBack = train.markers.count("pivot_back")
                ? train.markers["pivot_back"] : glm::vec3(0.0f);
            const float lineHeight = trackSegments.empty() ? 0.0f : trackSegments.front().start.y;
            train.routeStart = trackSegments.empty() ? glm::vec3(0.0f) : trackSegments.front().start;
            train.routeEnd = trackSegments.size() > 1 ? trackSegments[trackSegments.size() - 2].end
                : (trackSegments.empty() ? glm::vec3(0.0f, 0.0f, trackPieceLength) : trackSegments.back().end);
            train.routeStart.y = lineHeight - assetScale * pivotBack.y;
            train.routeEnd.y = train.routeStart.y;
            train.routeDistance = glm::length(train.routeEnd - train.routeStart);
            train.routeDirection = train.routeDistance > 0.0f ? glm::normalize(train.routeEnd - train.routeStart) : glm::vec3(0.0f, 0.0f, 1.0f);
            train.routePosition = 0.0f;
            train.position = train.routeStart;
            train.transform = glm::translate(glm::mat4(1.0f), train.position);
            train.scale = assetScale;
            train.transform = glm::scale(train.transform, glm::vec3(train.scale));
            train.transform *= train.sourceTransform;
            if (audioAvailable && configuration && !configuration->engineSound.empty()) {
                const fs::path soundPath = trainDirectory / "sounds" / configuration->engineSound;
                trainAudio.startLoopingEngine(train, soundPath);
            }
            trains.push_back(std::move(train));
            std::cout << "✓ Successfully loaded textured train on the track: " << trainName << std::endl;

            // Выводим информацию о текстурах
            const Model& loadedTrain = trains.back();
            for (size_t i = 0; i < loadedTrain.meshes.size(); i++) {
                std::cout << "  Mesh " << i << " has " << loadedTrain.meshes[i].textures.size() << " textures" << std::endl;
                for (size_t j = 0; j < loadedTrain.meshes[i].textures.size(); j++) {
                    std::cout << "    Texture " << j << ": " << loadedTrain.meshes[i].textures[j].type
                              << " ID=" << loadedTrain.meshes[i].textures[j].id << std::endl;
                }
            }
        }
    } else {
        std::cout << "No train model found in trains/ folder!" << std::endl;
        std::cout << "Please put your .glb file in: trains/your_train_name/model.glb" << std::endl;
    }

    std::cout << "=== TRAIN SIMULATOR ENGINE ===" << std::endl;
    std::cout << "Controls: WASD - move, Mouse - look, E/Q - up/down, ESC - exit" << std::endl;
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
            glfwSetWindowShouldClose(window, true);
        }

        // Рендеринг
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        trainAudio.updateListener(cameraPos, cameraFront, cameraUp);
        for (auto& train : trains) {
            updateTrainMotion(train, deltaTime);
            trainAudio.updateEngine(train, cameraPos);
        }
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);

        // === РИСУЕМ ПЛОСКОСТЬ ===
        glUseProgram(planeShader);
        glm::mat4 model = glm::mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(planeShader, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(planeShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(planeShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        drawPlane();

        // === РИСУЕМ ПОЕЗД С ТЕКСТУРОЙ ===
        glUseProgram(modelShader);
        glm::vec3 lightPos = glm::vec3(5.0f, 10.0f, 5.0f);
        glUniform3fv(glGetUniformLocation(modelShader, "lightPos"), 1, glm::value_ptr(lightPos));
        glUniform3fv(glGetUniformLocation(modelShader, "viewPos"), 1, glm::value_ptr(cameraPos));
        glUniformMatrix4fv(glGetUniformLocation(modelShader, "view"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(modelShader, "projection"), 1, GL_FALSE, glm::value_ptr(projection));

        // === РИСУЕМ ТЕСТОВЫЙ ПУТЬ: 10 СЕКЦИЙ ПО 5 МЕТРОВ ===
        for (const auto& segment : trackSegments) {
            const glm::mat4 railTransform = segment.transform * rail.sourceTransform;
            glUniformMatrix4fv(glGetUniformLocation(modelShader, "model"), 1, GL_FALSE,
                               glm::value_ptr(railTransform));
            rail.draw(modelShader);
        }

        for (auto& train : trains) {
            glUniformMatrix4fv(glGetUniformLocation(modelShader, "model"), 1, GL_FALSE, glm::value_ptr(train.transform));

            // Each mesh selects its own material texture in Mesh::draw().
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
    trainAudio.shutdown();
    rail.cleanup();
    glDeleteVertexArrays(1, &planeVAO);
    glDeleteBuffers(1, &planeVBO);
    glDeleteBuffers(1, &planeEBO);
    glDeleteProgram(modelShader);
    glDeleteProgram(planeShader);

    glfwTerminate();
    return 0;
}
