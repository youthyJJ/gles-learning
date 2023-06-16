#include "Renderer.h"

#include <GLES3/gl3.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <memory>
#include <vector>
#include <android/imagedecoder.h>
#include <cassert>
#include <regex>

#include "AndroidOut.h"

//! executes glGetString and outputs the result to logcat
#define PRINT_GL_STRING(s) {debug << #s": "<< glGetString(s) << std::endl;}

/*!
 * @brief if glGetString returns a space separated list of elements, prints each one on a new line
 *
 * This works by creating an istringstream of the input c-style string. Then that is used to create
 * a vector -- each element of the vector is a new element in the input string. Finally a foreach
 * loop consumes this and outputs it to logcat using @a aout
 */
#define PRINT_GL_STRING_AS_LIST(s) { \
std::istringstream extensionStream((const char *) glGetString(s));\
std::vector<std::string> extensionList(\
        std::istream_iterator<std::string>{extensionStream},\
        std::istream_iterator<std::string>());\
debug << #s":\n";\
for (auto& extension: extensionList) {\
    debug << extension << "\n";\
}\
debug << std::endl;\
}

//! Color for cornflower blue. Can be sent directly to glClearColor
#define CORNFLOWER_BLUE 100 / 255.f, 149 / 255.f, 237 / 255.f, 1
#define ERROR_COLOR 255 / 255.f, 10 / 255.f, 10 / 255.f, 1


/*!
 * Half the height of the projection matrix. This gives you a renderable area of height 4 ranging
 * from -2 to 2
 */
static constexpr float kProjectionHalfHeight = 2.f;

/*!
 * The near plane distance for the projection matrix.
 * Since this is an orthographic projection matrix,
 * it's convenient to have negative values for sorting (and avoiding z-fighting at 0).
 */
static constexpr float kProjectionNearPlane = -1.f;

/*!
 * The far plane distance for the projection matrix.
 * Since this is an orthographic porjection matrix,
 * it's convenient to have the far plane equidistant from 0 as the near plane.
 */
static constexpr float kProjectionFarPlane = 1.f;

static bool checkStatus(GLenum handler, bool loggable = true);

GLuint vertexShader;
GLenum fragmentShader;
GLenum shaderProgram;
GLuint VAO;
GLuint EBO;


void Renderer::_initRenderer() {
    // Choose your render attributes
    constexpr EGLint attributes[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_DEPTH_SIZE, 24,
            EGL_NONE
    };

    // The default display is probably what you want on Android
    auto display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);

    // figure out how many configs there are
    EGLint numConfigs;
    eglChooseConfig(display, attributes, nullptr, 0, &numConfigs);

    // get the list of configurations
    std::unique_ptr<EGLConfig[]> supportedConfigs(new EGLConfig[numConfigs]);
    eglChooseConfig(display, attributes, supportedConfigs.get(), numConfigs, &numConfigs);

    // Find a config we like.
    // Could likely just grab the first if we don't care about anything else in the config.
    // Otherwise hook in your own heuristic
    auto config = *std::find_if(
            supportedConfigs.get(),
            supportedConfigs.get() + numConfigs,
            [&display](const EGLConfig &config) {
                EGLint red, green, blue, depth;
                if (eglGetConfigAttrib(display, config, EGL_RED_SIZE, &red)
                    && eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &green)
                    && eglGetConfigAttrib(display, config, EGL_BLUE_SIZE, &blue)
                    && eglGetConfigAttrib(display, config, EGL_DEPTH_SIZE, &depth)) {

                    debug << "Found config with "
                          << red << ", "
                          << green << ", "
                          << blue << ", "
                          << depth
                          << std::endl;
                    return red == 8 && green == 8 && blue == 8 && depth == 24;
                }
                return false;
            });

    debug << "Found " << numConfigs << " configs" << std::endl;
    debug << "Chose " << config << std::endl;

    // create the proper window surface
    EGLint format;
    eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);
    EGLSurface surface = eglCreateWindowSurface(display, config, app_->window, nullptr);

    // Create a GLES 3 context
    EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLContext context = eglCreateContext(display, config, nullptr, contextAttributes);

    // get some window metrics
    auto madeCurrent = eglMakeCurrent(display, surface, surface, context);
    assert(madeCurrent);

    display_ = display;
    surface_ = surface;
    context_ = context;

    // make width and height invalid so it gets updated the first frame in @a updateRenderArea()
    width_ = -1;
    height_ = -1;

    PRINT_GL_STRING(GL_VENDOR);
    PRINT_GL_STRING(GL_RENDERER);
    PRINT_GL_STRING(GL_VERSION);
    PRINT_GL_STRING_AS_LIST(GL_EXTENSIONS);


// --------

    static const char *vertexShaderSource = R"vertex(#version 300 es
            layout (location = 0) in vec3 aPos;

            void main() {
                gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
            }
        )vertex";

    static const char *fragmentShaderSource = R"fragment(#version 300 es
            out vec4 FragColor;

            void main() {
                FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);
            }
        )fragment";


    float vertices[] = {
            0.5f, 0.5f, 0.0f,    // 右上角1
            0.5f, -0.5f, 0.0f,   // 右下角1
            -0.5f, -0.5f, 0.0f,  // 左下角1
            -0.5f, 0.5f, 0.0f   // 左上角1
    };

    int indies[] = {
            0, 1, 3,
            1, 2, 3
    };


    // 编译顶点着色器
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glCompileShader(vertexShader);
    if (!checkStatus(vertexShader)) {
        glClearColor(ERROR_COLOR);
        return;
    }

    // 编译片元着色器
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glCompileShader(fragmentShader);
    if (!checkStatus(fragmentShader)) {
        glDeleteShader(vertexShader);
        glClearColor(ERROR_COLOR);
        return;
    }

    // 创建着色器程序, 链接着色器
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    if (!checkStatus(shaderProgram)) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        glClearColor(ERROR_COLOR);
        return;
    }

    // 程序创建好了之后, 就不再需要shader了
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);


    // 创建一个VBO,用来将CPU中的数据缓冲到GPU中
    GLuint vbo;
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 趁着VBO还没解绑, 创建一个VAO用来描述缓冲进去的数据的结构
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    // 完事之后解绑
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // 创建一个元素数组, 缓存一下绘制顺序
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indies), indies, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

//------

    glClearColor(CORNFLOWER_BLUE);

    // enable alpha globally for now, you probably don't want to do this in a game
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

}

static bool checkStatus(GLenum handler, bool loggable) {
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

void Renderer::_updateRenderArea() {
    EGLint width;
    eglQuerySurface(display_, surface_, EGL_WIDTH, &width);

    EGLint height;
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &height);

    if (width != width_ || height != height_) {
        width_ = width;
        height_ = height;
        glViewport(0, 0, width, height);
    }
}

Renderer::~Renderer() {
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context_ != EGL_NO_CONTEXT) {
            eglDestroyContext(display_, context_);
            context_ = EGL_NO_CONTEXT;
        }
        if (surface_ != EGL_NO_SURFACE) {
            eglDestroySurface(display_, surface_);
            surface_ = EGL_NO_SURFACE;
        }
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
    }
}

void Renderer::handleInput() {
    // handle all queued inputs
    auto *inputBuffer = android_app_swap_input_buffers(app_);
    if (!inputBuffer) {
        // no inputs yet.
        return;
    }

    // handle motion events (motionEventsCounts can be 0).
    for (auto i = 0; i < inputBuffer->motionEventsCount; i++) {
        auto &motionEvent = inputBuffer->motionEvents[i];
        auto action = motionEvent.action;

        // Find the pointer index, mask and bitshift to turn it into a readable value.
        auto pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        debug << "Pointer(s): ";

        // get the x and y position of this event if it is not ACTION_MOVE.
        auto &pointer = motionEvent.pointers[pointerIndex];
        auto x = GameActivityPointerAxes_getX(&pointer);
        auto y = GameActivityPointerAxes_getY(&pointer);

        // determine the action type and process the event accordingly.
        switch (action & AMOTION_EVENT_ACTION_MASK) {
            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
                debug << "(" << pointer.id << ", " << x << ", " << y << ") "
                      << "Pointer Down";
                break;

            case AMOTION_EVENT_ACTION_CANCEL:
                // treat the CANCEL as an UP event: doing nothing in the app, except
                // removing the pointer from the cache if pointers are locally saved.
                // code pass through on purpose.
            case AMOTION_EVENT_ACTION_UP:
            case AMOTION_EVENT_ACTION_POINTER_UP:
                debug << "(" << pointer.id << ", " << x << ", " << y << ") "
                      << "Pointer Up";
                break;

            case AMOTION_EVENT_ACTION_MOVE:
                // There is no pointer index for ACTION_MOVE, only a snapshot of
                // all active pointers; app needs to cache previous active pointers
                // to figure out which ones are actually moved.
                for (auto index = 0; index < motionEvent.pointerCount; index++) {
                    pointer = motionEvent.pointers[index];
                    x = GameActivityPointerAxes_getX(&pointer);
                    y = GameActivityPointerAxes_getY(&pointer);
                    debug << "(" << pointer.id << ", " << x << ", " << y << ")";

                    if (index != (motionEvent.pointerCount - 1)) debug << ",";
                    debug << " ";
                }
                debug << "Pointer Move";
                break;
            default:
                debug << "Unknown MotionEvent Action: " << action;
        }
        debug << std::endl;
    }
    // clear the motion input count in this buffer for main thread to re-use.
    android_app_clear_motion_events(inputBuffer);

    // handle input key events.
    for (auto i = 0; i < inputBuffer->keyEventsCount; i++) {
        auto &keyEvent = inputBuffer->keyEvents[i];
        debug << "Key: " << keyEvent.keyCode << " ";
        switch (keyEvent.action) {
            case AKEY_EVENT_ACTION_DOWN:
                debug << "Key Down";
                break;
            case AKEY_EVENT_ACTION_UP:
                debug << "Key Up";
                break;
            case AKEY_EVENT_ACTION_MULTIPLE:
                // Deprecated since Android API level 29.
                debug << "Multiple Key Actions";
                break;
            default:
                debug << "Unknown KeyEvent Action: " << keyEvent.action;
        }
        debug << std::endl;
    }
    // clear the key input count too.
    android_app_clear_key_events(inputBuffer);
}

void Renderer::render() {
    _updateRenderArea();
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shaderProgram);

// ----

    // 启用对应的VAO对象
    glBindVertexArray(VAO);
    // 选取索引为0的数据
    glEnableVertexAttribArray(0);
    // 不使用EBO的时候, 可以直接绘制
    // glDrawArrays(GL_TRIANGLES, 0, 3);

    // 使用EBO了, 这需要启用EBO, 再绘制EBO声明的内容
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // 完事后解绑
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);

// ----

    eglSwapBuffers(display_, surface_);
}