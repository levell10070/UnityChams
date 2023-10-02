#include <GL/glew.h>

#include <Windows.h>
#include <iostream>

#include <MinHook.h>
#include <stack>
#include <atomic>
#include <unordered_set>
#include <unordered_map>
#include <thread>

#include "Shader.h"
#include "Texture.h"
#include "FrameBuffer.h"

std::string gDrawTexShaderVs = R"(
	#version 330

	layout(location=0) in vec2 inPos;
	layout(location=1) in vec2 inTexCoords;
	
	out vec2 o_texCoords;

	void main()
	{
		o_texCoords = inPos;
		gl_Position = vec4(inPos, 0.0, 1.0);
	}	
)";

std::string  gDrawTexShaderFs = R"(
	#version 330

	in vec2 o_texCoords;

	out vec4 o_OutCol;

	uniform sampler2D texInput;

	void main()
	{
		o_OutCol = texture(texInput, o_texCoords);
	}	

)";

bool bInitialized = false;

HDC gAppDC;
HGLRC gHackGlCtx = 0;
HGLRC gGameGlCtx = 0;

GLuint gBridgeFBO = 0;
GLuint gBridgeFBOTex = 0;
const unsigned char* gBridgeFBOData = nullptr;

GLuint gFullScrVAO;

GLuint gDrawTexShader = 0;

std::unordered_set<std::string> foundUniforms;
std::atomic_bool gbTesting;
std::unordered_set<std::string>::iterator gCurrentUniform;

int (GLAPIENTRY* oglGetUniformLocation)(GLuint, const GLchar*);
GLint GLAPIENTRY hglGetUniformLocation(GLuint program, const GLchar* name) {

	if(gbTesting == true)
		return oglGetUniformLocation(program, name);

	// At this point we are not testing, lets keep taking samples

	if (foundUniforms.count(name) < 1)
	{
		//printf("%d:%s\n", program, name);

		foundUniforms.insert(name);
	}

	return oglGetUniformLocation(program, name);
}

bool BindedShaderHasUniform(const std::string& uniform)
{
	GLint currProgram;
	glGetIntegerv(GL_CURRENT_PROGRAM, &currProgram);

	GLint id = -1;

	if (oglGetUniformLocation) id = oglGetUniformLocation(currProgram, uniform.c_str());
	else id = glGetUniformLocation(currProgram, uniform.c_str());

	return id != -1;
}

std::unordered_map<std::string, ChamsInfo> uniformsWallhack{
	{"_Cutoff", {{0, 255, 255, 0}, {0, 255, 0, 0}}},
	{"_LightColor0", {{0, 139, 139, 0}, {255, 139, 0, 0}}}
};

void (WINAPI* oglDrawElements)(GLenum mode, GLsizei count, GLenum type, const void* indices);

GLuint gTexCyan;
GLuint gTexRed;
GLuint gTexYellow;
GLuint gTexPurple;

GLuint* gTexAlwaysOnTop = &gTexCyan;
GLuint* gTexJustVisible = &gTexPurple;

void InitChamsTextures()
{
	GLTexture2DBindRestore guard(0);

	ColorToTexture({ 0		,0xFF	,0xFF	,0xFF }, &gTexCyan);
	ColorToTexture({ 0xFF	,0x0	,0x0	,0xFF }, &gTexRed);
	ColorToTexture({ 0xFF	,0xFF	,0x0	,0xFF }, &gTexYellow);
	ColorToTexture({ 0xCF	,0x34	,0x76	,0xFF }, &gTexPurple);
}


void DrawVisible(GLenum mode, GLsizei count, GLenum type, const void* indices)
{
	GLTexture2DBindRestore guard(*gTexJustVisible);

	oglDrawElements(mode, count, type, indices);
}

void DrawAlwaysTop(GLenum mode, GLsizei count, GLenum type, const void* indices)
{
	GLTexture2DBindRestore guard(*gTexAlwaysOnTop);

	glDisable(GL_DEPTH_TEST);

	oglDrawElements(mode, count, type, indices);

	glEnable(GL_DEPTH_TEST);
}

void InitializeFBOAndTexs()
{
	static bool bInited = false;

	if (bInited)
		return;

	GLint viewPort[4];

	glGetIntegerv(GL_VIEWPORT, viewPort);

	gGameGlCtx = wglGetCurrentContext();

	printf("Game: %d\n", gGameGlCtx);
	printf("Hack: %d\n", gHackGlCtx);

	wglMakeCurrent(gAppDC, gGameGlCtx);

	gBridgeFBO = CreateFrameBuffer(viewPort[2], viewPort[3], 4, gBridgeFBOTex);
	InitChamsTextures();

	printf("%d\n", std::this_thread::get_id());

	bInited = true;
}

void WINAPI hglDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) {

	if (!bInitialized)
		return oglDrawElements(mode, count, type, indices);

	if (mode != GL_TRIANGLES || count < 1000) return oglDrawElements(mode, count, type, indices);

#ifdef TESTING
	if (!gbTesting && GetAsyncKeyState(VK_SPACE) & 1)
	{
		// At this point, we want to start testing

		gbTesting = true;
		gCurrentUniform = foundUniforms.begin();
	}

	if (gbTesting && GetAsyncKeyState(VK_SPACE) & 1)
	{
		// At this point, we want to go to the next uniform

		gCurrentUniform++;

		printf("Selected Uniform: %s\n", (*gCurrentUniform).c_str());
	}

	if (!gbTesting) return;
	if (BindedShaderHasUniform(*gCurrentUniform) == false) return;
#else
	std::pair<std::string, ChamsInfo> foundKv;
	bool bFound = false;

	for (const auto& kv : uniformsWallhack)
	{
		if (BindedShaderHasUniform(kv.first) == true) {
			foundKv = kv;
			bFound = true;
			break;
		}
	}

	if (!bFound) return oglDrawElements(mode, count, type, indices);

	// At this point, we found a given uniformid
	// Lets proceed with the customization
	
#endif

	//oglDrawElements(mode, count, type, indices);

	// At this point, lets just render our stuff
	// in our own FBO, so we can use the texture
	// to render in our context =)

	InitializeFBOAndTexs();

	/*{
		GLFrameBufferBindRestore bridge(gBridgeFBO);

		DrawAlwaysTop(mode, count, type, indices);
		DrawVisible(mode, count, type, indices);
	}*/

	DrawAlwaysTop(mode, count, type, indices);
	DrawVisible(mode, count, type, indices);
}

std::stack<void*> gToUnloadFuncs;
std::atomic_bool gbRunning = true;

void (GLAPIENTRY*oglShaderSource)(GLuint shader,
	GLsizei count,
	const GLchar** string,
	const GLint* length);

void GLAPIENTRY hglShaderSource(GLuint shader, GLuint count,
	const GLchar** string,
	const GLint* length)
{
	printf("%s\n", *string);

	oglShaderSource(shader, count, string, length);
}

bool InitializeFullScreenVAO()
{
	float fullScreenQuad[] = {
		// Positions       // Texture Coords
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
		 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,

		-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
		 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		 1.0f,  1.0f, 0.0f, 1.0f, 1.0f
	};

	GLuint gFullScrVBO = 0;

	glGenVertexArrays(1, &gFullScrVAO);
	glGenBuffers(1, &gFullScrVBO);
	glBindVertexArray(gFullScrVAO);
	glBindBuffer(GL_ARRAY_BUFFER, gFullScrVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(fullScreenQuad), fullScreenQuad, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

	return true;
}

bool Initialize(HDC hdc, HGLRC& outContext, HGLRC& prevCtx)
{
	gAppDC = hdc;

	outContext = wglCreateContext(hdc);

	wglMakeCurrent(hdc, outContext);

	if (glewInit() != GLEW_OK)
	{
		printf("Failed Initializing Glew\n");
		return false;
	}

	if ((gDrawTexShader = CompileShaders(gDrawTexShaderVs.c_str(), gDrawTexShaderFs.c_str())) == 0)
		return false;

	if (InitializeFullScreenVAO() == false)
		return false;

#ifdef TESTING
	/*MH_CreateHook((LPVOID)glShaderSource, hglShaderSource, (LPVOID*)&oglShaderSource);
	MH_EnableHook(glShaderSource);
	gToUnloadFuncs.push_back(glShaderSource);*/

	MH_CreateHook((LPVOID)glGetUniformLocation, hglGetUniformLocation, (LPVOID*)&oglGetUniformLocation);
	MH_EnableHook(glGetUniformLocation);
	gToUnloadFuncs.push(glGetUniformLocation);
#endif

	return true;
}

void* wglSwapBuffers;
BOOL(WINAPI* owglSwapBuffers)(HDC hdc);

BOOL WINAPI hwglSwapBuffers(HDC hdc)
{
	if(!gbRunning) return owglSwapBuffers(hdc);

	HGLRC prevCtx = wglGetCurrentContext();

	if(!bInitialized) 
		gbRunning = bInitialized = Initialize(hdc, gHackGlCtx, prevCtx);

	wglMakeCurrent(hdc, gHackGlCtx);

	if(gBridgeFBOTex) {
		static bool bSharedGameCtx = false;
		static GLuint dummyColorTex = 0;

		if (!bSharedGameCtx)
		{
			GLTexture2DBindRestore guard(0);

			ColorToTexture({255, 0, 0, 255}, &dummyColorTex);

			printf("Previous: %d\n", prevCtx);
			printf("Game(WGL): %d\n", gGameGlCtx);
			printf("Hack(WGL): %d\n", gHackGlCtx);
			
			wglMakeCurrent(NULL, NULL);

			//bool bSucessesShare = wglShareLists(gGameGlCtx, gHackGlCtx);

			//printf("%s\n", bSucessesShare ? "Sucessfully Sharing Context" : "Failed Sharing Context");
			//printf("%d\n", GetLastError()); // (5) ERROR_ACCESS_DENIED
			//printf("%d\n", std::this_thread::get_id());

			wglMakeCurrent(hdc, gHackGlCtx);

			bSharedGameCtx = true;
		}

		glUseProgram(gDrawTexShader);
		glActiveTexture(GL_TEXTURE0);

		//GLTexture2DBindRestore guard(gTexCyan); // gBridgeFBOTex

		//// At this point the texture binded the 
		//// rendered Stuff from the game context

		//glUniform1i(glGetUniformLocation(gDrawTexShader, "texInput"), 0);

		//// At this point, the shader, 
		//// have the texture binded

		//glBindVertexArray(gFullScrVAO);
		//glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	wglMakeCurrent(hdc, prevCtx);

	auto result = owglSwapBuffers(hdc);;

	return result;
}

bool Run()
{
	HMODULE ogl = LoadLibrary("OPENGL32.DLL");

	if (!ogl)
		return false;

	wglSwapBuffers = (void*) GetProcAddress(ogl, "wglSwapBuffers");

	if (MH_Initialize() != MH_OK)
		return false;

	MH_CreateHook((LPVOID)wglSwapBuffers, hwglSwapBuffers, (LPVOID*) & owglSwapBuffers);
	MH_EnableHook(wglSwapBuffers);
	gToUnloadFuncs.push(wglSwapBuffers);

	MH_CreateHook((LPVOID)glDrawElements, hglDrawElements, (LPVOID*)&oglDrawElements);
	MH_EnableHook(glDrawElements);
	gToUnloadFuncs.push(glDrawElements);

	while (gbRunning)
	{
		gbRunning = (GetAsyncKeyState(VK_DELETE) & 1) == 0;
		Sleep(200);
	}

	while (gToUnloadFuncs.empty() == false)
	{
		void* proc = gToUnloadFuncs.top(); gToUnloadFuncs.pop();
		MH_DisableHook(proc);
	}

	MH_Uninitialize();

	return true;
}

void WINAPI Start(HMODULE hMod)
{
	AllocConsole();
	FILE* f;
	freopen_s(&f, "CONOUT$", "w", stdout);

	bool result = Run();

	fclose(f);
	FreeConsole();

	FreeLibraryAndExitThread(hMod, result);
}

int WINAPI DllMain(HMODULE hMod, DWORD reason)
{
	if (reason != DLL_PROCESS_ATTACH)
		return TRUE;

	CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)Start, (LPVOID)hMod, NULL, NULL);

	return TRUE;
}