#pragma once

#include <GL/glew.h>

GLuint CreateFrameBuffer(int width, int height, int channels, GLuint& outTexture);

struct GLFrameBufferBindRestore {
    GLFrameBufferBindRestore(GLuint toBind);
    ~GLFrameBufferBindRestore();

    GLuint prevFBO;
};