#pragma once

#include <GL/glew.h>

struct Vec3 {
	GLboolean x = 0;
	GLboolean y = 0;
	GLboolean z = 0;
};

struct Vec4 {
	GLboolean x = 0;
	GLboolean y = 0;
	GLboolean z = 0;
	GLboolean w = 0;
};

struct ChamsInfo {
	Vec4 visibleChams;
	Vec4 alwaysTopChams;
};