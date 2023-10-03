#include <GL/glew.h>
#include <Windows.h>
#include <ShlObj.h>

#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <thread>
#include <atomic>
#include <fstream>

#include <MinHook.h>

#include <chrono>
#include <string>

#include <filesystem>

std::string BrowseForFolder(const std::string& title) {
	std::string folderPath;
	BROWSEINFO bi = { 0 };
	LPITEMIDLIST pidl;

	bi.ulFlags = BIF_RETURNONLYFSDIRS;
	bi.lpszTitle = title.c_str();

	pidl = SHBrowseForFolder(&bi);

	if (pidl != NULL) {
		TCHAR selectedPath[MAX_PATH];
		if (SHGetPathFromIDList(pidl, selectedPath)) {
			folderPath = selectedPath;
		}
		CoTaskMemFree(pidl);
	}

	return folderPath;
}

std::vector<void*> gToUnloadHooks;

std::atomic_bool gbRunning = true;

std::string getCurrentMilliseconds() {
	std::chrono::milliseconds millis = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch()
	);
	return std::to_string(millis.count());
}

bool WriteFile(const std::string& filePath, const std::string& content) {
	// Open the file for writing.
	std::ofstream outputFile(filePath);

	// Check if the file is successfully opened.
	if (!outputFile.is_open()) {
		return false; // Return false to indicate failure.
	}

	// Write the content to the file.
	outputFile << content;

	// Close the file.
	outputFile.close();

	return true; // Return true to indicate success.
}

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

	std::string getSource(bool sorted = false) const
	{
		std::string result = "";

		if (!sorted)
		{
			for (const std::string& source : sources)
				result += source + "\n";

			return result;
		}

		std::vector<std::string> sortedSources = sources;

		std::sort(sortedSources.begin(), sortedSources.end());

		for (const std::string& source : sources)
			result += source + "\n";

		return result;
	}

	std::string getTypeName() const
	{
		switch (type)
		{
		case GL_VERTEX_SHADER: return "vs";
		case GL_FRAGMENT_SHADER: return "fs";
		case GL_GEOMETRY_SHADER: return "geometry";
		case GL_COMPUTE_SHADER: return "compute";
		default: return "unkn";
		}
	}

	std::string getFullFileName(const std::string& prefix) const
	{
		return prefix + "_" + getTypeName() + ".txt";
	}

	bool DumpToFile(const std::string& outputPath, const std::string& prefix) const
	{
		std::filesystem::path path = outputPath;
		std::filesystem::path fullOutputPath = path / getFullFileName(prefix);
		std::string fullOutputPathStr = fullOutputPath.string();

		if (WriteFile(fullOutputPathStr, getSource()) == false)
			return false;

		printf("Dumped: %s\n", fullOutputPathStr.c_str());

		return true;
	}

	size_t getHash() const
	{
		std::hash<std::string> hashser;

		return hashser(getSource(true));
	}

	std::vector<std::string> sources;
	GLuint shader;
	GLuint type;
};

struct ShaderProgram {
	
	static std::unordered_set<size_t> dumpedShaders;

	ShaderProgram()
	{
		pseudoName = getCurrentMilliseconds();
	}

	std::unordered_map<GLuint, ShaderObject> linkedShaders;
	GLuint program;
	std::string pseudoName;

	size_t getHash() const
	{
		std::vector<size_t> hashes;
		std::hash<std::string> hasher;
		std::string hashesConcatened = "";

		for (const auto& kv : linkedShaders)
			hashes.push_back(kv.second.getHash());

		std::sort(hashes.begin(), hashes.end());

		for (size_t hash : hashes)
			hashesConcatened += std::to_string(hash);

		return hasher(hashesConcatened);
	}

	bool DumpToFile(const std::string& outPath) const
	{
		bool bAnyDumped = false;

		for (const auto& kv : linkedShaders)
		{
			const ShaderObject& shaderObj = kv.second;

			if (shaderObj.DumpToFile(outPath, pseudoName) == false)
				continue;

			bAnyDumped = true;
		}

		return bAnyDumped;
	}
};

struct Shaders {
	std::unordered_map<GLuint, ShaderObject> shadersObjects;
	std::unordered_map<GLuint, ShaderProgram> shaders;
	std::unordered_set<size_t> dumpedShaders;

	bool HasShaderObject(GLuint shaderObj)
	{
		return shadersObjects.find(shaderObj) != shadersObjects.end();
	}

	bool HasShaderProgram(GLuint shaderObj)
	{
		return shaders.find(shaderObj) != shaders.end();
	}

	bool DumpAllToFile(const std::string& outputPath)
	{
		if (std::filesystem::exists(outputPath) == false)
		{
			try {
				// Define the path to the directory you want to create, including nested folders.
				std::filesystem::path directoryPath = outputPath;

				// Use fs::create_directories to create the directory and its parent directories if they don't exist.
				std::filesystem::create_directories(directoryPath);
			}
			catch (const std::filesystem::filesystem_error& e) {
				return false;
			}
		}

		for (const std::pair<GLuint, ShaderProgram>& shader : shaders)
		{
			const ShaderProgram& shaderProg = shader.second;
			size_t shaderHash = shaderProg.getHash();

			if (dumpedShaders.find(shaderHash) != dumpedShaders.end())
				continue;

			if (shaderProg.DumpToFile(outputPath) == false)
				continue;

			dumpedShaders.insert(shaderHash);
		}

		return true;
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

	ShaderProgram& shaderProg = gShaders.shaders[program];

	shaderProg.linkedShaders[shader] = gShaders.shadersObjects[shader];
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

		if (GetAsyncKeyState(VK_INSERT) & 1)
		{
			std::string dumpFolder = BrowseForFolder("Choose Shaders Dump Folder");

			if (dumpFolder.empty() == false)
			{
				printf("Dumping All Shaders to: %s\n", dumpFolder.c_str());

				gShaders.DumpAllToFile(dumpFolder);
			}
		}

		Sleep(1000);

		gbRunning = (GetAsyncKeyState(VK_DELETE) & 1) == 0;
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