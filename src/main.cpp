#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <array>
#include <vector>
#include <string>
#include <cmath>
#include <sstream>
#include <ctime>
#include "TextRenderer.h"
#include "AudioManager.h"

// ============================================
// GAME CONSTANTS
// ============================================
const unsigned int WINDOW_WIDTH = 900;
const unsigned int WINDOW_HEIGHT = 800;
const float PLAYER_MOVE_SPEED = 300.0f;
const float JOYSTICK_DEADZONE = 0.15f;
const unsigned int FONT_SIZE = 48;

// Speed settings for 6 seconds to cross screen
const float INITIAL_GAME_SPEED = 0.00556f;
const float SPEED_INCREASE = 0.00001f;
const float MAX_GAME_SPEED = 0.0133f;

// Game state enum
enum GameState { MENU, PLAYING, GAMEOVER };
GameState state = MENU;

// Player (in normalized coordinates -1 to 1)
float playerX = 0.0f, playerY = -0.8f;
const float PLAYER_RADIUS = 0.05f;

// Obstacle
const float OBSTACLE_WIDTH = 0.15f;
const float OBSTACLE_HEIGHT = 0.04f;
float obstacleY = 1.0f;
float obstacleX = 0.0f;

// Colors
float playerColor[3] = { 0.0f, 1.0f, 0.0f };
float obstacleColor[3] = { 1.0f, 0.0f, 0.0f };
float bgColor[3] = { 0.1f, 0.1f, 0.2f };

// Game variables
int score = 0;
float gameSpeed = INITIAL_GAME_SPEED;
int selectedPlayerColor = 1;
int selectedObstacleColor = 0;
int selectedBgColor = 0;

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;
int frameCount = 0;
float fpsTimer = 0.0f;
int currentFPS = 0;
bool joystickConnected = false;

// Window dimensions (updated on resize)
int windowWidth = WINDOW_WIDTH;
int windowHeight = WINDOW_HEIGHT;

// Projection matrix
glm::mat4 projection;

// Color palettes
float playerColors[4][3] = {
    {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f}
};

float obstacleColors[4][3] = {
    {1.0f, 0.5f, 0.0f}, {1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}
};

float bgColors[4][3] = {
    {0.1f, 0.1f, 0.2f}, {0.2f, 0.1f, 0.1f}, {0.1f, 0.2f, 0.1f}, {0.15f, 0.15f, 0.15f}
};

// OpenGL objects
unsigned int quadVAO = 0, quadVBO = 0;
unsigned int circleVAO = 0, circleVBO = 0;
unsigned int shaderProgram = 0;
int circleSegments = 32;

// Text renderer
TextRenderer* textRenderer = nullptr;

// Shader sources with projection matrix
const char* vertexShaderSource = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
uniform vec2 uPosition;
uniform vec2 uScale;
uniform mat4 uProjection;
void main() {
    gl_Position = uProjection * vec4(aPos * uScale + uPosition, 0.0, 1.0);
}
)";

const char* fragmentShaderSource = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 uColor;
uniform float uAlpha;
void main() {
    FragColor = vec4(uColor, uAlpha);
}
)";

// ============================================
// FUNCTION DECLARATIONS
// ============================================
unsigned int compileShader(unsigned int type, const char* source);
void initShaders();
void initQuad();
void initCircle();
void updateProjection(int width, int height);
void drawRectangle(float x, float y, float width, float height, float color[3], float alpha = 1.0f);
void drawCircle(float x, float y, float radius, float color[3], float alpha = 1.0f);
void processInput(GLFWwindow* window);
void updateGame();
void renderMenu(GLFWwindow* window);
void renderPlaying(GLFWwindow* window);
void renderGameOver(GLFWwindow* window);
void render(GLFWwindow* window);
void errorCallback(int error, const char* description);
void framebufferSizeCallback(GLFWwindow* window, int width, int height);

// ============================================
// SHADER COMPILATION
// ============================================
unsigned int compileShader(unsigned int type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "Shader compilation error: " << infoLog << std::endl;
    }
    return shader;
}

void initShaders() {
    unsigned int vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    unsigned int fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vs);
    glAttachShader(shaderProgram, fs);
    glLinkProgram(shaderProgram);

    int success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "Program linking error: " << infoLog << std::endl;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
}

// ============================================
// PROJECTION UPDATE
// ============================================
void updateProjection(int width, int height) {
    windowWidth = width;
    windowHeight = height;

    // Calculate aspect ratio
    float aspectRatio = (float)width / (float)height;

    // Create projection matrix that maintains aspect ratio
    if (aspectRatio >= 1.0f) {
        // Wide window
        projection = glm::ortho(-aspectRatio, aspectRatio, -1.0f, 1.0f, -1.0f, 1.0f);
    }
    else {
        // Tall window
        projection = glm::ortho(-1.0f, 1.0f, -1.0f / aspectRatio, 1.0f / aspectRatio, -1.0f, 1.0f);
    }

    glViewport(0, 0, width, height);
}

// ============================================
// GEOMETRY INITIALIZATION
// ============================================
void initQuad() {
    float vertices[] = { -0.5f, -0.5f, 0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f };
    unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };
    unsigned int EBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

void initCircle() {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    vertices.push_back(0.0f);
    vertices.push_back(0.0f);
    for (int i = 0; i <= circleSegments; i++) {
        float angle = 2.0f * 3.14159f * i / circleSegments;
        vertices.push_back(cos(angle) * 0.5f);
        vertices.push_back(sin(angle) * 0.5f);
    }
    for (int i = 0; i < circleSegments; i++) {
        indices.push_back(0);
        indices.push_back(i + 1);
        indices.push_back(i + 2);
    }
    unsigned int EBO;
    glGenVertexArrays(1, &circleVAO);
    glGenBuffers(1, &circleVBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(circleVAO);
    glBindBuffer(GL_ARRAY_BUFFER, circleVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

// ============================================
// DRAWING FUNCTIONS (with projection)
// ============================================
void drawRectangle(float x, float y, float width, float height, float color[3], float alpha) {
    glUseProgram(shaderProgram);

    // Set projection matrix
    GLint projLoc = glGetUniformLocation(shaderProgram, "uProjection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projection[0][0]);

    glUniform2f(glGetUniformLocation(shaderProgram, "uPosition"), x, y);
    glUniform2f(glGetUniformLocation(shaderProgram, "uScale"), width, height);
    glUniform3f(glGetUniformLocation(shaderProgram, "uColor"), color[0], color[1], color[2]);
    glUniform1f(glGetUniformLocation(shaderProgram, "uAlpha"), alpha);

    glBindVertexArray(quadVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void drawCircle(float x, float y, float radius, float color[3], float alpha) {
    glUseProgram(shaderProgram);

    // Set projection matrix
    GLint projLoc = glGetUniformLocation(shaderProgram, "uProjection");
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, &projection[0][0]);

    glUniform2f(glGetUniformLocation(shaderProgram, "uPosition"), x, y);
    glUniform2f(glGetUniformLocation(shaderProgram, "uScale"), radius * 2.0f, radius * 2.0f);
    glUniform3f(glGetUniformLocation(shaderProgram, "uColor"), color[0], color[1], color[2]);
    glUniform1f(glGetUniformLocation(shaderProgram, "uAlpha"), alpha);

    glBindVertexArray(circleVAO);
    glDrawElements(GL_TRIANGLES, circleSegments * 3, GL_UNSIGNED_INT, 0);
}

// ============================================
// INPUT PROCESSING
// ============================================
void processInput(GLFWwindow* window) {
    static bool keyProcessed[512] = { false };

    if (state == MENU) {
        // Player color selection
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS && !keyProcessed[GLFW_KEY_1]) {
            selectedPlayerColor = 0;
            playerColor[0] = playerColors[0][0];
            playerColor[1] = playerColors[0][1];
            playerColor[2] = playerColors[0][2];
            keyProcessed[GLFW_KEY_1] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_RELEASE) keyProcessed[GLFW_KEY_1] = false;

        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS && !keyProcessed[GLFW_KEY_2]) {
            selectedPlayerColor = 1;
            playerColor[0] = playerColors[1][0];
            playerColor[1] = playerColors[1][1];
            playerColor[2] = playerColors[1][2];
            keyProcessed[GLFW_KEY_2] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_RELEASE) keyProcessed[GLFW_KEY_2] = false;

        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS && !keyProcessed[GLFW_KEY_3]) {
            selectedPlayerColor = 2;
            playerColor[0] = playerColors[2][0];
            playerColor[1] = playerColors[2][1];
            playerColor[2] = playerColors[2][2];
            keyProcessed[GLFW_KEY_3] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_RELEASE) keyProcessed[GLFW_KEY_3] = false;

        if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS && !keyProcessed[GLFW_KEY_4]) {
            selectedPlayerColor = 3;
            playerColor[0] = playerColors[3][0];
            playerColor[1] = playerColors[3][1];
            playerColor[2] = playerColors[3][2];
            keyProcessed[GLFW_KEY_4] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_4) == GLFW_RELEASE) keyProcessed[GLFW_KEY_4] = false;

        // Obstacle color selection
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS && !keyProcessed[GLFW_KEY_Q]) {
            selectedObstacleColor = 0;
            obstacleColor[0] = obstacleColors[0][0];
            obstacleColor[1] = obstacleColors[0][1];
            obstacleColor[2] = obstacleColors[0][2];
            keyProcessed[GLFW_KEY_Q] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_RELEASE) keyProcessed[GLFW_KEY_Q] = false;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS && !keyProcessed[GLFW_KEY_W]) {
            selectedObstacleColor = 1;
            obstacleColor[0] = obstacleColors[1][0];
            obstacleColor[1] = obstacleColors[1][1];
            obstacleColor[2] = obstacleColors[1][2];
            keyProcessed[GLFW_KEY_W] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_RELEASE) keyProcessed[GLFW_KEY_W] = false;

        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS && !keyProcessed[GLFW_KEY_E]) {
            selectedObstacleColor = 2;
            obstacleColor[0] = obstacleColors[2][0];
            obstacleColor[1] = obstacleColors[2][1];
            obstacleColor[2] = obstacleColors[2][2];
            keyProcessed[GLFW_KEY_E] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_RELEASE) keyProcessed[GLFW_KEY_E] = false;

        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS && !keyProcessed[GLFW_KEY_R]) {
            selectedObstacleColor = 3;
            obstacleColor[0] = obstacleColors[3][0];
            obstacleColor[1] = obstacleColors[3][1];
            obstacleColor[2] = obstacleColors[3][2];
            keyProcessed[GLFW_KEY_R] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_R) == GLFW_RELEASE) keyProcessed[GLFW_KEY_R] = false;

        // Background color selection
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS && !keyProcessed[GLFW_KEY_A]) {
            selectedBgColor = 0;
            bgColor[0] = bgColors[0][0];
            bgColor[1] = bgColors[0][1];
            bgColor[2] = bgColors[0][2];
            keyProcessed[GLFW_KEY_A] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_RELEASE) keyProcessed[GLFW_KEY_A] = false;

        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS && !keyProcessed[GLFW_KEY_S]) {
            selectedBgColor = 1;
            bgColor[0] = bgColors[1][0];
            bgColor[1] = bgColors[1][1];
            bgColor[2] = bgColors[1][2];
            keyProcessed[GLFW_KEY_S] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_RELEASE) keyProcessed[GLFW_KEY_S] = false;

        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS && !keyProcessed[GLFW_KEY_D]) {
            selectedBgColor = 2;
            bgColor[0] = bgColors[2][0];
            bgColor[1] = bgColors[2][1];
            bgColor[2] = bgColors[2][2];
            keyProcessed[GLFW_KEY_D] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_RELEASE) keyProcessed[GLFW_KEY_D] = false;

        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS && !keyProcessed[GLFW_KEY_F]) {
            selectedBgColor = 3;
            bgColor[0] = bgColors[3][0];
            bgColor[1] = bgColors[3][1];
            bgColor[2] = bgColors[3][2];
            keyProcessed[GLFW_KEY_F] = true;
        }
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_RELEASE) keyProcessed[GLFW_KEY_F] = false;

        // START GAME with sound
        if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) {
            AudioManager::GetInstance().Play("start", 0.7f);
            AudioManager::GetInstance().PlayMusic("background", 0.3f);
            state = PLAYING;
            score = 0;
            gameSpeed = INITIAL_GAME_SPEED;
            obstacleY = 1.0f;
            obstacleX = ((rand() % 180) - 90) / 100.0f;
            playerX = 0.0f;
            playerY = -0.8f;
        }
    }
    else if (state == PLAYING) {
        float inputX = 0.0f, inputY = 0.0f;

        // Joystick input
        int count;
        const float* axes = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &count);
        if (count >= 2 && axes) {
            if (std::abs(axes[0]) > JOYSTICK_DEADZONE) {
                float sign = (axes[0] > 0) ? 1.0f : -1.0f;
                inputX = sign * (std::abs(axes[0]) - JOYSTICK_DEADZONE) / (1.0f - JOYSTICK_DEADZONE);
            }
            if (std::abs(axes[1]) > JOYSTICK_DEADZONE) {
                float sign = (axes[1] > 0) ? 1.0f : -1.0f;
                inputY = sign * (std::abs(axes[1]) - JOYSTICK_DEADZONE) / (1.0f - JOYSTICK_DEADZONE);
            }
        }

        // Keyboard input
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) inputX = -1.0f;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) inputX = 1.0f;
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) inputY = 1.0f;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) inputY = -1.0f;

        // Calculate movement based on aspect ratio
        float aspectRatio = (float)windowWidth / (float)windowHeight;
        float moveScaleX = (PLAYER_MOVE_SPEED / 800.0f) * deltaTime;
        float moveScaleY = (PLAYER_MOVE_SPEED / 600.0f) * deltaTime;

        // Adjust for aspect ratio
        if (aspectRatio >= 1.0f) {
            moveScaleX /= aspectRatio;
        }
        else {
            moveScaleY *= aspectRatio;
        }

        playerX += inputX * moveScaleX;
        playerY += inputY * moveScaleY;

        // Clamp player position based on aspect ratio
        float boundX = (aspectRatio >= 1.0f) ? aspectRatio : 1.0f;
        float boundY = (aspectRatio >= 1.0f) ? 1.0f : 1.0f / aspectRatio;

        playerX = std::max(-boundX + PLAYER_RADIUS, std::min(boundX - PLAYER_RADIUS, playerX));
        playerY = std::max(-boundY + PLAYER_RADIUS, std::min(boundY - PLAYER_RADIUS, playerY));
    }
    else if (state == GAMEOVER) {
        if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) {
            AudioManager::GetInstance().Play("start", 0.7f);
            AudioManager::GetInstance().PlayMusic("background", 0.3f);
            state = PLAYING;
            score = 0;
            gameSpeed = INITIAL_GAME_SPEED;
            obstacleY = 1.0f;
            playerX = 0.0f;
            playerY = -0.8f;
        }
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            AudioManager::GetInstance().StopMusic();
            state = MENU;
        }
    }
}

// ============================================
// GAME UPDATE
// ============================================
void updateGame() {
    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    frameCount++;
    fpsTimer += deltaTime;
    if (fpsTimer >= 1.0f) {
        currentFPS = frameCount;
        frameCount = 0;
        fpsTimer = 0.0f;
    }

    if (state == PLAYING) {
        obstacleY -= gameSpeed * deltaTime * 60.0f;
        gameSpeed += SPEED_INCREASE;
        if (gameSpeed > MAX_GAME_SPEED) gameSpeed = MAX_GAME_SPEED;

        // Score point with sound
        if (obstacleY < -1.0f) {
            AudioManager::GetInstance().Play("point", 0.5f);
            obstacleY = 1.0f;

            // Random X based on aspect ratio
            float aspectRatio = (float)windowWidth / (float)windowHeight;
            float boundX = (aspectRatio >= 1.0f) ? aspectRatio : 1.0f;
            obstacleX = ((rand() % 180) - 90) / 100.0f * boundX;

            score++;
        }

        // Collision detection
        float closestX = std::max(obstacleX - OBSTACLE_WIDTH / 2, std::min(playerX, obstacleX + OBSTACLE_WIDTH / 2));
        float closestY = std::max(obstacleY - OBSTACLE_HEIGHT / 2, std::min(playerY, obstacleY + OBSTACLE_HEIGHT / 2));
        float dx = playerX - closestX;
        float dy = playerY - closestY;

        // Game over with sound
        if (std::sqrt(dx * dx + dy * dy) < PLAYER_RADIUS) {
            AudioManager::GetInstance().Play("collision", 0.8f);
            AudioManager::GetInstance().StopMusic();
            state = GAMEOVER;
        }
    }
}

// ============================================
// RENDER MENU
// ============================================
void renderMenu(GLFWwindow* window) {
    float centerX = 0.0f;
    float currentY = 0.75f;

    textRenderer->RenderText("MSK REAL LABS", windowWidth / 2.0f - 180.0f, windowHeight - 80.0f, 1.2f, glm::vec3(1.0f, 1.0f, 1.0f));
    textRenderer->RenderText("OBSTACLE DODGER", windowWidth / 2.0f - 200.0f, windowHeight - 140.0f, 0.8f, glm::vec3(1.0f, 1.0f, 1.0f));

    float menuStartY = windowHeight - 240.0f;

    textRenderer->RenderText("PLAYER COLOR", windowWidth / 2.0f - 100.0f, menuStartY, 0.7f, glm::vec3(0.5f, 0.8f, 1.0f));

    float boxStartX = windowWidth / 2.0f - 100.0f;
    float boxY = menuStartY - 50.0f;

    // Draw color boxes
    for (int i = 0; i < 4; i++) {
        float boxX = (boxStartX + i * 55.0f) / windowWidth * 2.0f - 1.0f;
        float boxYNorm = (boxY / windowHeight * 2.0f - 1.0f) + 0.05f;

        // Adjust for aspect ratio
        float aspectRatio = (float)windowWidth / (float)windowHeight;
        if (aspectRatio >= 1.0f) {
            boxX *= aspectRatio;
        }
        else {
            boxYNorm /= aspectRatio;
        }

        drawRectangle(boxX, boxYNorm, 0.1f, 0.1f, playerColors[i]);
        if (i == selectedPlayerColor) {
            float border[3] = { 1.0f, 1.0f, 1.0f };
            drawRectangle(boxX, boxYNorm, 0.11f, 0.11f, border);
            drawRectangle(boxX, boxYNorm, 0.1f, 0.1f, playerColors[i]);
        }
    }

    textRenderer->RenderText("Press: 1 = Red  2 = Green  3 = Blue  4 = Yellow",
        windowWidth / 2.0f - 280.0f, menuStartY - 90.0f, 0.45f, glm::vec3(0.7f, 0.7f, 0.7f));

    float obsStartY = menuStartY - 170.0f;
    textRenderer->RenderText("OBSTACLE COLOR", windowWidth / 2.0f - 120.0f, obsStartY, 0.7f, glm::vec3(0.5f, 0.8f, 1.0f));

    float obsBoxY = obsStartY - 50.0f;
    for (int i = 0; i < 4; i++) {
        float boxX = (boxStartX + i * 55.0f) / windowWidth * 2.0f - 1.0f;
        float boxYNorm = (obsBoxY / windowHeight * 2.0f - 1.0f) + 0.05f;

        float aspectRatio = (float)windowWidth / (float)windowHeight;
        if (aspectRatio >= 1.0f) boxX *= aspectRatio;
        else boxYNorm /= aspectRatio;

        drawRectangle(boxX, boxYNorm, 0.1f, 0.1f, obstacleColors[i]);
        if (i == selectedObstacleColor) {
            float border[3] = { 1.0f, 1.0f, 1.0f };
            drawRectangle(boxX, boxYNorm, 0.11f, 0.11f, border);
            drawRectangle(boxX, boxYNorm, 0.1f, 0.1f, obstacleColors[i]);
        }
    }

    textRenderer->RenderText("Press: Q = Orange  W = Magenta  E = Cyan  R = White",
        windowWidth / 2.0f - 310.0f, obsStartY - 90.0f, 0.45f, glm::vec3(0.7f, 0.7f, 0.7f));

    float bgStartY = obsStartY - 170.0f;
    textRenderer->RenderText("BACKGROUND COLOR", windowWidth / 2.0f - 140.0f, bgStartY, 0.7f, glm::vec3(0.5f, 0.8f, 1.0f));

    float bgBoxY = bgStartY - 50.0f;
    for (int i = 0; i < 4; i++) {
        float boxX = (boxStartX + i * 55.0f) / windowWidth * 2.0f - 1.0f;
        float boxYNorm = (bgBoxY / windowHeight * 2.0f - 1.0f) + 0.05f;

        float aspectRatio = (float)windowWidth / (float)windowHeight;
        if (aspectRatio >= 1.0f) boxX *= aspectRatio;
        else boxYNorm /= aspectRatio;

        drawRectangle(boxX, boxYNorm, 0.1f, 0.1f, bgColors[i]);
        if (i == selectedBgColor) {
            float border[3] = { 1.0f, 1.0f, 1.0f };
            drawRectangle(boxX, boxYNorm, 0.11f, 0.11f, border);
            drawRectangle(boxX, boxYNorm, 0.1f, 0.1f, bgColors[i]);
        }
    }

    textRenderer->RenderText("Press: A = Blue  S = Red  D = Green  F = Gray",
        windowWidth / 2.0f - 280.0f, bgStartY - 90.0f, 0.45f, glm::vec3(0.7f, 0.7f, 0.7f));

    float controlY = bgStartY - 170.0f;
    textRenderer->RenderText("CONTROLS", windowWidth / 2.0f - 70.0f, controlY, 0.7f, glm::vec3(0.5f, 0.8f, 1.0f));
    textRenderer->RenderText("Arrow Keys or Joystick - Move Player",
        windowWidth / 2.0f - 220.0f, controlY - 40.0f, 0.5f, glm::vec3(0.7f, 0.7f, 0.7f));
    textRenderer->RenderText("ENTER - Start Game    ESC - Exit",
        windowWidth / 2.0f - 200.0f, controlY - 70.0f, 0.5f, glm::vec3(0.7f, 0.7f, 0.7f));
}

// ============================================
// RENDER PLAYING
// ============================================
void renderPlaying(GLFWwindow* window) {
    std::stringstream ss;
    ss << "Score: " << score;
    textRenderer->RenderText(ss.str(), 20.0f, (float)windowHeight - 40.0f, 0.6f, glm::vec3(1.0f, 1.0f, 0.0f));

    ss.str("");
    ss << "FPS: " << currentFPS;
    textRenderer->RenderText(ss.str(), (float)windowWidth - 120.0f, (float)windowHeight - 40.0f, 0.5f, glm::vec3(0.7f, 0.7f, 0.7f));

    if (joystickConnected)
        textRenderer->RenderText("Joystick", 20.0f, 40.0f, 0.4f, glm::vec3(0.5f, 0.8f, 0.5f));
    else
        textRenderer->RenderText("Keyboard", 20.0f, 40.0f, 0.4f, glm::vec3(0.5f, 0.8f, 0.5f));
}

// ============================================
// RENDER GAME OVER
// ============================================
void renderGameOver(GLFWwindow* window) {
    float centerX = (float)windowWidth / 2.0f;
    float centerY = (float)windowHeight / 2.0f;

    drawCircle(playerX, playerY, PLAYER_RADIUS, playerColor);
    drawRectangle(obstacleX, obstacleY, OBSTACLE_WIDTH, OBSTACLE_HEIGHT, obstacleColor);

    textRenderer->RenderText("GAME OVER", centerX - 140.0f, centerY + 80.0f, 1.2f, glm::vec3(1.0f, 0.0f, 0.0f));

    std::stringstream ss;
    ss << "Score: " << score;
    textRenderer->RenderText(ss.str(), centerX - 60.0f, centerY + 20.0f, 0.8f, glm::vec3(1.0f, 1.0f, 1.0f));

    textRenderer->RenderText("Press ENTER to Restart", centerX - 160.0f, centerY - 30.0f, 0.6f, glm::vec3(0.0f, 1.0f, 0.0f));
    textRenderer->RenderText("Press ESC for Menu", centerX - 130.0f, centerY - 70.0f, 0.5f, glm::vec3(0.7f, 0.7f, 0.7f));
}

// ============================================
// RENDER DISPATCHER
// ============================================
void render(GLFWwindow* window) {
    switch (state) {
    case MENU: renderMenu(window); break;
    case PLAYING: renderPlaying(window); break;
    case GAMEOVER: renderGameOver(window); break;
    }
}

// ============================================
// CALLBACKS
// ============================================
void errorCallback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << std::endl;
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    updateProjection(width, height);
    if (textRenderer) textRenderer->UpdateProjection(width, height);
}

// ============================================
// MAIN
// ============================================
int main() {
    glfwSetErrorCallback(errorCallback);
    if (!glfwInit()) return -1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT,
        "MSK Real Labs - Obstacle Dodger", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSetWindowSizeLimits(window, 800, 600, GLFW_DONT_CARE, GLFW_DONT_CARE);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) return -1;

    joystickConnected = glfwJoystickPresent(GLFW_JOYSTICK_1);

    // Initialize projection
    updateProjection(WINDOW_WIDTH, WINDOW_HEIGHT);

    // Initialize game systems
    initShaders();
    initQuad();
    initCircle();

    // Initialize text renderer
    textRenderer = new TextRenderer(WINDOW_WIDTH, WINDOW_HEIGHT);

    bool fontLoaded = false;
    if (textRenderer->Load("fonts/Roboto-Regular.ttf", FONT_SIZE)) {
        fontLoaded = true;
    }
    else if (textRenderer->Load("Roboto-Regular.ttf", FONT_SIZE)) {
        fontLoaded = true;
    }
    else if (textRenderer->Load("C:/Windows/Fonts/Arial.ttf", FONT_SIZE)) {
        fontLoaded = true;
    }
    else if (textRenderer->Load("C:/Windows/Fonts/calibri.ttf", FONT_SIZE)) {
        fontLoaded = true;
    }

    if (!fontLoaded) {
        std::cerr << "ERROR: Could not load any font file!" << std::endl;
    }

    // Initialize audio
    if (AudioManager::GetInstance().Initialize()) {
        AudioManager::GetInstance().LoadSound("start", "sounds/start.wav");
        AudioManager::GetInstance().LoadSound("collision", "sounds/collision.wav");
        AudioManager::GetInstance().LoadSound("point", "sounds/point.wav");
        AudioManager::GetInstance().LoadSound("background", "sounds/background.wav");
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    srand(static_cast<unsigned int>(time(NULL)));
    obstacleX = ((rand() % 180) - 90) / 100.0f;

    lastFrame = static_cast<float>(glfwGetTime());

    while (!glfwWindowShouldClose(window)) {
        glClearColor(bgColor[0], bgColor[1], bgColor[2], 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        processInput(window);
        updateGame();

        if (state == PLAYING || state == GAMEOVER) {
            drawCircle(playerX, playerY, PLAYER_RADIUS, playerColor);
            drawRectangle(obstacleX, obstacleY, OBSTACLE_WIDTH, OBSTACLE_HEIGHT, obstacleColor);
        }

        render(window);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    AudioManager::GetInstance().Shutdown();
    delete textRenderer;
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);
    glDeleteVertexArrays(1, &circleVAO);
    glDeleteBuffers(1, &circleVBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}