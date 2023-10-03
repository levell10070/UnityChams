#include <GL/glew.h>
#include <Windows.h>

#include <iostream>
#include <unordered_map>
#include <thread>
#include <atomic>

#include <MinHook.h>

std::vector<void*> gToUnloadHooks;

std::atomic_bool gbRunning = true;

struct ShaderObject {
	void AddSource(const char* pSource)
	{
		sources.push_back(pSource);
	}

	void AddSources(size_t sourcesCnt, const char** pSources)
	{
		for (int i = 0; i < sourcesCnt; i++)
			AddSource(pSources[i]);
	}

	void AddSources(size_t sourcesCnt, const std::vector<std::string>& pSources)
	{
		std::vector<const char*> sources;

		for (int i = 0; i < sourcesCnt; i++)
			sources.push_back(pSources[i].c_str());

		AddSources(sourcesCnt, sources.data());
	}

	void AddSources(size_t sourcesCnt, const char** pSources, const int* length)
	{
		std::vector<std::string> sources;

		bool bUseStrLen = length == nullptr;

		for (int i = 0; i < sourcesCnt; i++)
		{
			const char* currSrc = pSources[i];
			size_t srcSz = bUseStrLen ? strlen(currSrc) : length[i];

			sources.emplace_back(currSrc, srcSz);
		}

		AddSources(sourcesCnt, sources);
	}

	std::string getSource()
	{
		std::string result = "";

		for (const std::string& source : sources)
			result += source + "\n";
	}


	std::vector<std::string> sources;
	GLuint shader;
	GLuint type;
};

struct ShaderProgram {
	std::vector<ShaderObject*> linkedShaders;
	GLuint program;
};

struct Shaders {
	std::unordered_map<GLuint, ShaderObject> shadersObjects;
	std::unordered_map<GLuint, ShaderProgram> shaders;

	bool HasShaderObject(GLuint shaderObj)
	{
		return shadersObjects.find(shaderObj) != shadersObjects.end();
	}

	bool HasShaderProgram(GLuint shaderObj)
	{
		return shadersObjects.find(shaderObj) != shadersObjects.end();
	}
};

Shaders gShaders;

GLuint(WINAPI* oglCreateShader)(GLenum type);
GLuint WINAPI hglCreateShader(GLenum type)
{
	GLuint shader = oglCreateShader(type);

	if (shader == 0)
		return shader;

	gShaders.shadersObjects.erase(shader);

	ShaderObject& shaderObj = gShaders.shadersObjects[shader];

	shaderObj.type = type;
	shaderObj.shader = shader;

	return shader;
}

void (GLAPIENTRY* oglShaderSource) (GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
void GLAPIENTRY hglShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length)
{
	oglShaderSource(shader, count, string, length);

	if (gShaders.HasShaderObject(shader) == false)
	{
		printf("Warning: %d shader object not registered\n", shader);
		return;
	}

	ShaderObject& shaderObj = gShaders.shadersObjects[shader];

	shaderObj.AddSources(count, (const char**)string, length);
}

GLuint(GLAPIENTRY* oglCreateProgram) (void);
GLuint GLAPIENTRY hglCreateProgram (void)
{
	GLuint shader = oglCreateProgram();

	if (shader == 0)
		return shader;

	gShaders.shaders.erase(shader);

	ShaderProgram& shaderProg = gShaders.shaders[shader];

	shaderProg.program = shader;

	return shader;
}

void (GLAPIENTRY* oglAttachShader)(GLuint program, GLuint shader);
void hglAttachShader(GLuint program, GLuint shader)
{
	oglAttachShader(program, shader);

	if (gShaders.HasShaderProgram(program) == false)
	{
		printf("Warning: %d program not registered\n", program);
		return;
	}
}

void* wglSwapBuffers;
BOOL (WINAPI* owglSwapBuffers)(HDC hdc);
BOOL WINAPI hwglSwapBuffers(HDC hdc)
{
	BOOL result = owglSwapBuffers(hdc);

	static bool bInitilized = false;

	if (bInitilized)
		return result;

	if (glewInit() != GLEW_OK)
	{
		printf("Failed Initializing Glew\n");

		MH_DisableHook(wglSwapBuffers);

		gbRunning = false;

		return result;
	}

	MH_CreateHook(glCreateShader, hglCreateShader, (void**)&oglCreateShader);
	MH_EnableHook(glCreateShader);
	gToUnloadHooks.push_back(glCreateShader);

	MH_CreateHook(glShaderSource, hglShaderSource, (void**)&oglShaderSource);
	MH_EnableHook(glShaderSource);
	gToUnloadHooks.push_back(glShaderSource);

	MH_CreateHook(glCreateProgram, hglCreateProgram, (void**)&oglCreateProgram);
	MH_EnableHook(glCreateProgram);
	gToUnloadHooks.push_back(glCreateProgram);

	MH_CreateHook(glAttachShader, hglAttachShader, (void**)&oglAttachShader);
	MH_EnableHook(glAttachShader);
	gToUnloadHooks.push_back(glAttachShader);

	MH_DisableHook(wglSwapBuffers);
	bInitilized = true;

	return result;
}

bool Run()
{
	if (MH_Initialize() != MH_OK)
		return false;

	wglSwapBuffers = (void*)GetProcAddress(GetModuleHandleA("OPENGL32.DLL"), "wglSwapBuffers");

	if (wglSwapBuffers == nullptr)
		return false;

	MH_CreateHook(wglSwapBuffers, hwglSwapBuffers, (void**)&owglSwapBuffers);
	MH_EnableHook(wglSwapBuffers);

	while (gbRunning)
	{
		// Lets periodicly check 
		// if we are signaled to unload

		Sleep(1000);
	}

	for (void* toUnload : gToUnloadHooks)
		MH_DisableHook(toUnload);

	MH_Uninitialize();

	return true;
}

void WINAPI Start(HMODULE hMod)
{
	bool result = 1;

	if (AllocConsole())
	{
		// At this point, we sucessfully created the console

		FILE* f;
		freopen_s(&f, "CONOUT$", "w", stdout);

		// At this point, we sucessfully 
		// reopened stdout ( 
		// now we can write to stdout of this console 
		// )

		result = Run();

		// At this point, our module, finilized Running!

		fclose(f);
		FreeConsole();
	}

	// Now Lets unload everything!

	FreeLibraryAndExitThread(hMod, result);
}

int WINAPI DllMain(HMODULE hMod, DWORD reason)
{
	if (reason != DLL_PROCESS_ATTACH)
		return TRUE;

	CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)Start, (LPVOID)hMod, NULL, NULL);

	return TRUE;
}