#pragma once

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>

#define NOMINMAX
#include <windows.h>
#include <assert.h>
#include <functional>

#include <d3d11.h>
#pragma comment (lib, "d3d11.lib")


#include <DirectXMath.h>
using namespace DirectX;


#include "DirectXTex.h"
#if _DEBUG
#pragma comment(lib,"DirectXTex_Debug.lib")//デバッグ用
#else
#pragma comment(lib,"DirectXTex_Release.lib")//リリース用(実行速度が早い
#endif



#pragma comment (lib, "winmm.lib")


#define SCREEN_WIDTH	(1280)
#define SCREEN_HEIGHT	(720)


namespace Application
{
	HWND GetWindow();
	void Invoke(std::function<void()> Function, int Time);
}

