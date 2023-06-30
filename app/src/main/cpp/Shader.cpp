#include <string>
#include <android/asset_manager.h>

#include "Shader.h"
#include "AndroidOut.h"

GLuint Shader::_loadGlShader(
        GLenum shaderType,
        AAssetManager *assetManager,
        const std::string &filePath
) {
    std::string prefix;
    switch (shaderType) {
        case GL_VERTEX_SHADER:
            prefix = "vertex source: ";
            break;
        case GL_FRAGMENT_SHADER:
            prefix = "fragment source: ";
            break;
    }

    char *source;
    _loadAssetFile(assetManager, filePath, &source);
    debug << prefix << std::endl << source << std::endl;

    auto glShader = glCreateShader(shaderType);
    glShaderSource(glShader, 1, &source, nullptr);
    delete[] source;
    glCompileShader(glShader);

    if (!_checkStatus(glShader, true)) {
        return false;
    } else {
        return glShader;
    }
}

bool Shader::_loadAssetFile(
        AAssetManager *assetManager,
        const std::string &fileName,
        char **fileContent
) {
    AAsset *asset = AAssetManager_open(
            assetManager,
            fileName.c_str(),
            AASSET_MODE_BUFFER);

    if (asset == nullptr) {
        // 处理错误情况
        *fileContent = nullptr;
        return false;
    }

    auto length = AAsset_getLength(asset);
    *fileContent = new char[length + 1];
    AAsset_read(asset, *fileContent, length);
    (*fileContent)[length] = '\0';

    AAsset_close(asset);
    return true;
}

bool Shader::_checkStatus(GLuint handler, bool loggable) {
    int success;

    bool isShader = glIsShader(handler) == GL_TRUE;
    bool isProgram = glIsProgram(handler) == GL_TRUE;
    if (isShader) {
        glGetShaderiv(handler, GL_COMPILE_STATUS, &success);
    }
    if (isProgram) {
        glGetProgramiv(handler, GL_LINK_STATUS, &success);
    }

    if (!success && loggable) {
        if (isShader) {
            int length;
            int typeCode;
            glGetShaderiv(handler, GL_SHADER_TYPE, &typeCode);

            auto shaderType = "unknown";
            if (typeCode == GL_VERTEX_SHADER) shaderType = "GL_VERTEX_SHADER";
            if (typeCode == GL_FRAGMENT_SHADER) shaderType = "GL_FRAGMENT_SHADER";

            glGetShaderiv(handler, GL_INFO_LOG_LENGTH, &length);
            auto log = new GLchar[length];
            glGetShaderInfoLog(handler, length, nullptr, log);
            warn << "Compile shader(" << shaderType << ") failure: " << log << std::endl;
            delete[] log;
        }

        if (isProgram) {
            int length;
            glGetProgramiv(handler, GL_INFO_LOG_LENGTH, &length);
            auto log = new GLchar[length];
            glGetProgramInfoLog(handler, length, nullptr, log);
            warn << "Program link failure: " << log << std::endl;
            delete[] log;
        }
    }

    return success;
}

Shader *Shader::loadShader(
        AAssetManager *assetManager,
        const std::string &vertexSourcePath,
        const std::string &fragmentSourcePath) {

    GLuint vertexShader = _loadGlShader(GL_VERTEX_SHADER, assetManager, vertexSourcePath);
    if (!vertexShader) return nullptr;

    GLuint fragmentShader = _loadGlShader(GL_FRAGMENT_SHADER, assetManager, fragmentSourcePath);
    if (!fragmentShader) {
        glDeleteShader(vertexShader);
        return nullptr;
    }

    auto program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    if (!_checkStatus(program)) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glDeleteProgram(program);
        return nullptr;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return new Shader(program);
}

void Shader::activate() const {
    glUseProgram(program_);
}

void Shader::deactivate() const {
    glUseProgram(0);
}

void Shader::setInt(std::string name, GLint value) {
    activate();
    auto location = glGetUniformLocation(program_, name.c_str());
    glUniform1i(location, value);
    deactivate();
}