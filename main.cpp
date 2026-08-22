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
    std::vector<Mesh> meshes;
    glm::mat4 transform = glm::mat4(1.0f);
    glm::vec3 position = glm::vec3(0.0f);
    float scale = 0.5f;

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
        
        std::vector<Texture> diffuseMaps = loadMaterialTextures(material, aiTextureType_DIFFUSE, "diffuse", scene, directory);
        result.textures.insert(result.textures.end(), diffuseMaps.begin(), diffuseMaps.end());
        
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
        aiProcess_CalcTangentSpace);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cout << "ERROR::ASSIMP:: " << importer.GetErrorString() << std::endl;
        return model;
    }

    std::string directory = fs::path(path).parent_path().string();

    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        model.meshes.push_back(processMesh(scene->mMeshes[i], scene, directory));
    }

    std::cout << "Loaded train: " << name << " (" << model.meshes.size() << " meshes)" << std::endl;
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
        -15.0f, 0.0f, -15.0f,   0.0f, 0.0f,
         15.0f, 0.0f, -15.0f,   1.0f, 0.0f,
         15.0f, 0.0f,  15.0f,   1.0f, 1.0f,
        -15.0f, 0.0f,  15.0f,   0.0f, 1.0f
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

// Функция поиска модели
std::string findModelPath() {
    if (!fs::exists("trains")) {
        return "";
    }

    for (const auto& entry : fs::directory_iterator("trains")) {
        if (entry.is_directory()) {
            for (const auto& fileEntry : fs::directory_iterator(entry.path())) {
                if (fileEntry.is_regular_file()) {
                    std::string ext = fileEntry.path().extension().string();
                    std::string extLower = ext;
                    for (char& c : extLower) c = std::tolower(c);
                    
                    if (extLower == ".glb" || extLower == ".gltf" || extLower == ".obj") {
                        return fileEntry.path().string();
                    }
                }
            }
        }
    }
    return "";
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

    // Создаем шейдеры
    unsigned int modelShader = createShaderProgram(modelVertexShader, modelFragmentShader);
    unsigned int planeShader = createShaderProgram(planeVertexShader, planeFragmentShader);

    // Инициализируем плоскость
    initPlane();

    // Загружаем модель
    std::string modelPath = findModelPath();
    
    if (!modelPath.empty()) {
        std::string trainName = fs::path(modelPath).parent_path().filename().string();
        std::cout << "Loading model from: " << modelPath << std::endl;
        Model train = loadModel(modelPath, trainName);
        
        if (!train.meshes.empty()) {
            train.position = glm::vec3(0.0f, 0.0f, 0.0f);
            train.transform = glm::translate(glm::mat4(1.0f), train.position);
            train.scale = 0.5f;
            train.transform = glm::scale(train.transform, glm::vec3(train.scale));
            trains.push_back(train);
            std::cout << "✓ Successfully loaded textured train: " << trainName << std::endl;
            
            // Выводим информацию о текстурах
            for (size_t i = 0; i < train.meshes.size(); i++) {
                std::cout << "  Mesh " << i << " has " << train.meshes[i].textures.size() << " textures" << std::endl;
                for (size_t j = 0; j < train.meshes[i].textures.size(); j++) {
                    std::cout << "    Texture " << j << ": " << train.meshes[i].textures[j].type 
                              << " ID=" << train.meshes[i].textures[j].id << std::endl;
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
        train.cleanup();
    }
    glDeleteVertexArrays(1, &planeVAO);
    glDeleteBuffers(1, &planeVBO);
    glDeleteBuffers(1, &planeEBO);
    glDeleteProgram(modelShader);
    glDeleteProgram(planeShader);

    glfwTerminate();
    return 0;
}