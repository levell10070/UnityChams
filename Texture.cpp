#include "Texture.h"

void GenerateFillSurfaceColor(const Vec4& color, size_t size, unsigned char** pOutTexture)
{
	if (pOutTexture)
	{
		unsigned char* data = new unsigned char[4 * size * size * sizeof(unsigned char)];

		for (unsigned int i = 0; i < size * size; i++)
		{
			data[i * 4] = color.x;
			data[i * 4 + 1] = color.y;
			data[i * 4 + 2] = color.z;
			data[i * 4 + 3] = color.w;
		}

		*pOutTexture = data;
	}
}

void ColorToTexture(const Vec4& color, GLuint* outTex)
{
	unsigned char* pGenTex = nullptr;

	glGenTextures(1, outTex);
	GenerateFillSurfaceColor(color, 128, &pGenTex);
	glBindTexture(GL_TEXTURE_2D, *outTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, pGenTex);

	delete[] pGenTex;
}

GLuint getBoundTexture2D()
{
	GLuint lastBoundTex;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, (GLint*)&lastBoundTex);
	return lastBoundTex;
}

GLTexture2DBindRestore::GLTexture2DBindRestore(GLuint toBind)
{
	prevTexture2D = getBoundTexture2D();
	glBindTexture(GL_TEXTURE_2D, toBind);
}

GLTexture2DBindRestore::~GLTexture2DBindRestore()
{
	glBindTexture(GL_TEXTURE_2D, prevTexture2D);
}

GLuint CreateTexture(int width, int height, int channels, GLenum wrapMode, const unsigned char* imageRaw)
{
	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);

	// Specify minification and magnification filters and wrap mode
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);

	// Determine the format based on the number of channels
	GLenum format = GL_RGB;
	if (channels == 4) {
		format = GL_RGBA;
	}

	glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, imageRaw);

	return textureID;
}
