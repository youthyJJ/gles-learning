#include <string>
#include <android/asset_manager.h>
#include <android/imagedecoder.h>
#include <vector>
#include <GLES3/gl3.h>

#ifndef EGL_LEARNING_IMAGE_H
#define EGL_LEARNING_IMAGE_H

class Image {
public:
    GLuint texture_;

    static std::shared_ptr<Image> load(
            AAssetManager *assetManager,
            const std::string &assetPath
    );

    inline ~Image() {
        if (texture_) {
            glDeleteTextures(1, &texture_);
            texture_ = 0;
        }
    }

private:

    inline Image(GLint texture) : texture_(texture) {}

};


#endif //EGL_LEARNING_IMAGE_H
