#ifndef EGL_LEARNING_SHADER_H
#define EGL_LEARNING_SHADER_H

#include <android/asset_manager.h>
#include <string>
#include <GLES3/gl3.h>

class Shader {
private :
    GLuint program_;

    constexpr Shader(GLuint program) : program_(program) {}

    static bool _loadAssetFile(
            AAssetManager *assetManager,
            const std::string &fileName,
            char **fileContent
    );

    static GLuint _loadGlShader(
            GLenum shaderType,
            AAssetManager *assetManager,
            const std::string &filePath
    );

    static bool _checkStatus(
            GLuint handler,
            bool loggable = true
    );

public:

    static Shader *loadShader(
            AAssetManager *assetManager,
            const std::string &vertexSourcePath,
            const std::string &fragmentSourcePath
    );

    void activate() const;

    void deactivate() const;

    inline ~Shader() {
        if (program_) {
            glDeleteProgram(program_);
            program_ = 0;
        }
    }

};


#endif //EGL_LEARNING_SHADER_H
