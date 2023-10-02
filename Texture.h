#include <GL/glew.h>
#include "Vectors.h"

void GenerateFillSurfaceColor(const Vec4& color, size_t size, unsigned char** pOutTexture);
void ColorToTexture(const Vec4& color, GLuint* outTex);
GLuint getBoundTexture2D();
GLuint CreateTexture(int width, int height, int channels, GLenum wrapMode = GL_CLAMP_TO_EDGE, const unsigned char* imageRaw = nullptr);

struct GLTexture2DBindRestore {
	GLTexture2DBindRestore(GLuint toBind);
	~GLTexture2DBindRestore();

	GLuint prevTexture2D;
};