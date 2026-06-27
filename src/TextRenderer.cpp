#include "TextRenderer.h"
#include <iostream>

// Constructor: store window dimensions for projection
TextRenderer::TextRenderer(unsigned int width, unsigned int height)
    : m_ScreenWidth(width)
    , m_ScreenHeight(height)
    , m_VAO(0)
    , m_VBO(0)
    , m_ShaderProgram(0)
    , m_FT(nullptr)
{
}

// Destructor: clean up all allocated resources
TextRenderer::~TextRenderer() {
    // Delete all glyph textures
    for (auto& ch : m_Characters) {
        glDeleteTextures(1, &ch.second.TextureID);
    }

    // Delete OpenGL objects
    if (m_VAO) glDeleteVertexArrays(1, &m_VAO);
    if (m_VBO) glDeleteBuffers(1, &m_VBO);
    if (m_ShaderProgram) glDeleteProgram(m_ShaderProgram);

    // Free FreeType library
    if (m_FT) FT_Done_FreeType(m_FT);
}

// Compile a shader from source and check for errors
unsigned int TextRenderer::CompileShader(unsigned int type, const char* source) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }
    return shader;
}

// Load font and create text rendering pipeline
bool TextRenderer::Load(const std::string& fontPath, unsigned int fontSize) {
    // Initialize FreeType library
    if (FT_Init_FreeType(&m_FT)) {
        std::cerr << "ERROR::FREETYPE: Could not init FreeType Library" << std::endl;
        return false;
    }

    // Load font face
    FT_Face face;
    if (FT_New_Face(m_FT, fontPath.c_str(), 0, &face)) {
        std::cerr << "ERROR::FREETYPE: Failed to load font: " << fontPath << std::endl;
        std::cerr << "Make sure the font file exists in your project directory or provide full path." << std::endl;
        return false;
    }

    // Set font size in pixels
    FT_Set_Pixel_Sizes(face, 0, fontSize);

    // Disable byte-alignment restriction for single-channel textures
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Generate textures for ASCII characters 32-126
    for (unsigned char c = 32; c < 127; c++) {
        // Load character glyph
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            std::cerr << "ERROR::FREETYPE: Failed to load Glyph: " << c << std::endl;
            continue;
        }

        // Generate OpenGL texture from glyph bitmap
        unsigned int texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0,
            GL_RED,
            GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer
        );

        // Set texture parameters for crisp text
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Store character data in map
        Character character = {
            texture,
            glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
            glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
            static_cast<unsigned int>(face->glyph->advance.x)
        };
        m_Characters.insert(std::pair<char, Character>(c, character));
    }

    // Clean up FreeType face (keep library for later cleanup)
    glBindTexture(GL_TEXTURE_2D, 0);
    FT_Done_Face(face);

    // Create shader program for text rendering
    unsigned int vertexShader = CompileShader(GL_VERTEX_SHADER, m_VertexShaderSource);
    unsigned int fragmentShader = CompileShader(GL_FRAGMENT_SHADER, m_FragmentShaderSource);

    m_ShaderProgram = glCreateProgram();
    glAttachShader(m_ShaderProgram, vertexShader);
    glAttachShader(m_ShaderProgram, fragmentShader);
    glLinkProgram(m_ShaderProgram);

    // Check for linking errors
    int success;
    glGetProgramiv(m_ShaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(m_ShaderProgram, 512, nullptr, infoLog);
        std::cerr << "ERROR::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        return false;
    }

    // Delete shaders after linking
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Initialize VAO and VBO for text quads
    InitRenderData();

    std::cout << "TextRenderer initialized successfully with FreeType!" << std::endl;
    std::cout << "Loaded " << m_Characters.size() << " characters from " << fontPath << std::endl;
    return true;
}

// Create VAO and VBO for dynamic text rendering
void TextRenderer::InitRenderData() {
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);

    // Allocate buffer for 6 vertices * 4 floats each (position + texcoords)
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);

    // Set vertex attributes
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

// Update projection matrix when window size changes
void TextRenderer::UpdateProjection(unsigned int width, unsigned int height) {
    m_ScreenWidth = width;
    m_ScreenHeight = height;
}

// Render text at screen pixel coordinates with specified color
void TextRenderer::RenderText(const std::string& text, float x, float y, float scale, glm::vec3 color) {
    // Activate shader and set uniforms
    glUseProgram(m_ShaderProgram);
    glUniform3f(glGetUniformLocation(m_ShaderProgram, "textColor"), color.x, color.y, color.z);

    // Create orthographic projection in screen space
    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(m_ScreenWidth),
        0.0f, static_cast<float>(m_ScreenHeight));
    glUniformMatrix4fv(glGetUniformLocation(m_ShaderProgram, "projection"), 1, GL_FALSE, &projection[0][0]);

    // Bind VAO and texture unit
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(m_VAO);

    // Iterate through all characters in the string
    for (char c : text) {
        if (m_Characters.find(c) == m_Characters.end()) continue;

        Character ch = m_Characters[c];

        // Calculate quad position and size
        float xpos = x + ch.Bearing.x * scale;
        float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;
        float w = ch.Size.x * scale;
        float h = ch.Size.y * scale;

        // Define quad vertices with texture coordinates
        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos,     ypos,       0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 1.0f },

            { xpos,     ypos + h,   0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 1.0f },
            { xpos + w, ypos + h,   1.0f, 0.0f }
        };

        // Bind character glyph texture
        glBindTexture(GL_TEXTURE_2D, ch.TextureID);

        // Update VBO with new vertex data
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        // Render the quad
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // Advance cursor to next character position
        x += (ch.Advance >> 6) * scale;
    }

    // Unbind
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}