#include <memory>

#include "Image.h"
#include "AndroidOut.h"

std::shared_ptr<Image> Image::load(
        AAssetManager *assetManager,
        const std::string &assetPath
) {
    auto asset = AAssetManager_open(
            assetManager,
            assetPath.c_str(),
            AASSET_MODE_BUFFER
    );
    if (!asset) {
        warn << "asset open failure, path: " << assetPath << std::endl;
        return nullptr;
    }

    AImageDecoder *decoder;
    auto create = AImageDecoder_createFromAAsset(asset, &decoder);
    if (ANDROID_IMAGE_DECODER_SUCCESS != create) {
        warn << "image create failure, path: " << assetPath << std::endl;
        return nullptr;
    }

    AImageDecoder_setAndroidBitmapFormat(
            decoder,
            ANDROID_BITMAP_FORMAT_RGBA_8888
    );

    auto header = AImageDecoder_getHeaderInfo(decoder);
    auto width = AImageDecoderHeaderInfo_getWidth(header);
    auto height = AImageDecoderHeaderInfo_getHeight(header);
    auto stride = AImageDecoder_getMinimumStride(decoder);
    debug << "image info:"
          << " [width: " << width << "]"
          << " [height: " << height << "]"
          << " [stride: " << stride << "]"
          << std::endl;

    auto decodeData = std::make_unique<std::vector<uint8_t>>(height * stride);
    auto decode = AImageDecoder_decodeImage(
            decoder,
            decodeData->data(),
            stride,
            decodeData->size()
    );
    if (ANDROID_IMAGE_DECODER_SUCCESS != decode) {
        warn << "image decode failure, path: " << assetPath << std::endl;
        return nullptr;
    }

    // 图片数据将左上角视为圆点,GL将右下角视为原点,需要将图片上下翻转才能正确显示
    auto verticalFlippedData = std::make_unique<std::vector<uint8_t>>(height * stride);
    for (int y = 0; y < height; ++y) {
        int srcOffset = y * stride;
        int destOffset = (height - y - 1) * stride;
        std::memcpy(
                verticalFlippedData->data() + destOffset,
                decodeData->data() + srcOffset,
                stride
        );
    }

    GLuint texture;
    glGenTextures(1, &texture);
    debug << "gl textureId: " << texture << std::endl;
    glBindTexture(GL_TEXTURE_2D, texture);

    // 设置环绕模式,此处设置为拉伸
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 设置缩放纹理过滤选项
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            width,
            height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            verticalFlippedData->data()
    );


    glGenerateMipmap(GL_TEXTURE_2D);

    AImageDecoder_delete(decoder);
    AAsset_close(asset);

    return std::shared_ptr<Image>(new Image(texture));
}
