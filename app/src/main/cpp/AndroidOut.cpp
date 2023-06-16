#include "AndroidOut.h"

AndroidOut androidDebug("GL_ES");
std::ostream debug(&androidDebug);

AndroidOut androidWarn("GL_ES", ANDROID_LOG_WARN);
std::ostream warn(&androidWarn);