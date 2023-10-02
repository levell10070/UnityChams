#include <GL/glew.h>
#include <Windows.h>

#include <iostream>
#include <unordered_map>
#include <thread>
#include <atomic>

#include <MinHook.h>

#include "Texture.h"

// This part is subject 
// to change from gram to game
// if you dont see the chams showing
// you may try with game specific uniforms names

void (WINAPI* oglDrawElements)(GLenum mode, GLsizei count, GLenum type, const void* indices);

GLuint gTexCyan;
GLuint gTexRed;
GLuint gTexYellow;
GLuint gTexPurple;

std::unordered_map<std::string, ChamsInfo> gChamsDescs{
	{"_Cutoff"		, {&gTexCyan, &gTexPurple}},	// For Trees.
	{"_LightColor0"	, {&gTexCyan, &gTexYellow}}		// For Entities
};

std::atomic_bool gbRunning = true;

bool CurrentShaderHasUniform(const std::string& uniform)
{
	GLint currProgram;
	glGetIntegerv(GL_CURRENT_PROGRAM, &currProgram);

	GLint id = -1;

	id = glGetUniformLocation(currProgram, uniform.c_str());

	return id != -1;
}

void DrawVisible(GLenum mode, GLsizei count, GLenum type, const void* indices, const ChamsInfo& chamsDesc)
{
	GLTexture2DBindRestore guard(*(chamsDesc.pVisibleChamsTex));

	oglDrawElements(mode, count, type, indices);
}

void DrawAlwaysTop(GLenum mode, GLsizei count, GLenum type, const void* indices, const ChamsInfo& chamsDesc)
{
	GLTexture2DBindRestore guard(*(chamsDesc.pAlwaysTopChams));

	glDisable(GL_DEPTH_TEST);

	oglDrawElements(mode, count, type, indices);

	glEnable(GL_DEPTH_TEST);
}

bool ChamsContextInitialize()
{
	// At this point, glew started without any problems

	{
		GLTexture2DBindRestore guard(0);

		ColorToTexture({ 0		,0xFF	,0xFF	,0xFF }, &gTexCyan);
		ColorToTexture({ 0xFF	,0x0	,0x0	,0xFF }, &gTexRed);
		ColorToTexture({ 0xFF	,0xFF	,0x0	,0xFF }, &gTexYellow);
		ColorToTexture({ 0xCF	,0x34	,0x76	,0xFF }, &gTexPurple);
	}

	// At this point, we have the right context, for our chams

	return true;
}

void ChamsContextShutdown(bool bUnhookElems = true)
{
	// To Unhook the glDrawElements func
	if(bUnhookElems) MH_DisableHook(glDrawElements);

	gbRunning = false;
}

void WINAPI hglDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) {

	static bool bGlewInitialized = false;

	if (!bGlewInitialized)
	{
		// At this point, there is a current app context binded
		// Meaning we can use it to call glewInit(), all we care 
		// is for the glProcs

		if (glewInit() != GLEW_OK)
		{
			printf("Failed Initializing Glew\n");

			glDrawElements(mode, count, type, indices);

			ChamsContextShutdown();

			return;
		}

		// At this point, Glew initilized Properly

		bGlewInitialized = true;
	}

	// At this point, there are, available glProcs

	if ((GetAsyncKeyState(VK_DELETE) & 1) == 1)
	{
		// At this point, user has requested to unload
		// Lets do this call
		
		oglDrawElements(mode, count, type, indices);

		ChamsContextShutdown();

		return;
	}

	if (mode != GL_TRIANGLES || count < 900)
	{
		// Seems this draw call is to small 
		// to be a entity, say like a player
		// lets ignore it.

		return oglDrawElements(mode, count, type, indices);
	}

	const std::pair<const std::string, ChamsInfo>* pChamDescKv = nullptr;

	for (const auto& chamDescKv : gChamsDescs)
	{
		if (!CurrentShaderHasUniform(chamDescKv.first)) continue;

		// At this point, we found a draw-call that is suitable as a "Chams Target".

		pChamDescKv = &chamDescKv;
		break;
	}

	if (pChamDescKv == nullptr)
	{
		// Seems we are not interested into, 
		// tweaking this draw-call, lets pass

		return oglDrawElements(mode, count, type, indices);
	}

	// At this point, we found the right 
	// thing we want to tweak on.
	// now, since we konw the right game context is bound, 
	// lets deal with chams context initializetion here

	static bool gbChamsCtxInitialized = false;

	if (!gbChamsCtxInitialized &&
		!(gbChamsCtxInitialized = ChamsContextInitialize()))
	{
		// At this point, seems we failed
		// to initilize Chams Context
		// Lets simply unhook and signal 
		// the main thread to unload

		oglDrawElements(mode, count, type, indices);

		ChamsContextShutdown();

		return;
	}

	// At this point, if first time, being here measn, we was sucessfully to initialize
	// otherwise, means we alredy initilized, and here we are running!

	DrawAlwaysTop(mode, count, type, indices, pChamDescKv->second);
	DrawVisible(mode, count, type, indices, pChamDescKv->second);
}

bool Run()
{
	if (MH_Initialize() != MH_OK)
		return false;

	MH_CreateHook((LPVOID)glDrawElements, hglDrawElements, (LPVOID*)&oglDrawElements);
	MH_EnableHook(glDrawElements);

	while (gbRunning)
	{
		// Lets periodicly check 
		// if we are signaled to unload

		Sleep(1000);
	}

	// The glDrawElements hook, is spected to unload itself, 
	// before signaling this thread to finish by setting 
	// the gbRunning flag, meaning that at this point, 
	// we are guaranteed to unload

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