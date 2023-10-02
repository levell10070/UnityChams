#include "FrameBuffer.h"
#include "Texture.h"

GLuint CreateFrameBuffer(int width, int height, int channels, GLuint& outTexture) {

    GLFrameBufferBindRestore guard(0);

    // Create a framebuffer object (FBO) and bind it
    GLuint framebuffer;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    // Create a texture and bind it
    outTexture = CreateTexture(width, height, channels);

    // Attach the texture to the FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outTexture, 0);

    // Check if the FBO is complete
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        // Handle error or return an error code
        glBindFramebuffer(GL_FRAMEBUFFER, 0); // Unbind the FBO
        glDeleteFramebuffers(1, &framebuffer); // Delete the FBO
        return 0; // Return 0 to indicate failure
    }

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, width, height);

    return framebuffer; // Return the FBO ID
}

GLuint GetBindedFrameBuffer()
{
    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    return prevFBO;
}

GLFrameBufferBindRestore::GLFrameBufferBindRestore(GLuint toBind)
{
    prevFBO = GetBindedFrameBuffer();
    glBindFramebuffer(GL_FRAMEBUFFER, toBind);
}

GLFrameBufferBindRestore::~GLFrameBufferBindRestore()
{
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
}