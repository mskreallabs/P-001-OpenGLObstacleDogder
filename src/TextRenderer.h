#ifndef TEXT_RENDERER_H
#define TEXT_RENDERER_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <map>

#include <ft2build.h>
#include FT_FREETYPE_H

// Structure to store character glyph data
struct Character {
    unsigned int TextureID;  // ID handle of the glyph texture
    glm::ivec2   Size;       // Size of glyph in pixels
    glm::ivec2   Bearing;    // Offset from baseline to left/top of glyph
    unsigned int Advance;    // Horizontal offset to advance to next glyph
};

class TextRenderer {
public:
    // Constructor - takes window dimensions for projection
    TextRenderer(unsigned int width, unsigned int height);

    // Destructor - cleans up all OpenGL and FreeType resources
    ~TextRenderer();

    // Load font face and generate glyph textures for ASCII 32-126
    bool Load(const std::string& fontPath, unsigned int fontSize);

    // Update projection matrix when window is resized
    void UpdateProjection(unsigned int width, unsigned int height);

    // Render text string at screen pixel coordinates
    void RenderText(const std::string& text, float x, float y, float scale, glm::vec3 color);

private:
    // OpenGL objects for text rendering
    unsigned int m_VAO;
    unsigned int m_VBO;
    unsigned int m_ShaderProgram;

    // Screen dimensions for orthographic projection
    unsigned int m_ScreenWidth;
    unsigned int m_ScreenHeight;

    // FreeType library handle
    FT_Library m_FT;

    // Glyph map: ASCII character -> Character glyph data
    std::map<char, Character> m_Characters;

    // Shader source code
    const char* m_VertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec4 vertex; // vec2 pos, vec2 tex
        out vec2 TexCoords;
        uniform mat4 projection;
        void main() {
            gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
            TexCoords = vertex.zw;
        }
    )";

    const char* m_FragmentShaderSource = R"(
        #version 330 core
        in vec2 TexCoords;
        out vec4 color;
        uniform sampler2D text;
        uniform vec3 textColor;
        void main() {    
            vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
            color = vec4(textColor, 1.0) * sampled;
        }
    )";

    // Helper functions for shader compilation and buffer initialization
    unsigned int CompileShader(unsigned int type, const char* source);
    void InitRenderData();
};

#endif // TEXT_RENDERER_H