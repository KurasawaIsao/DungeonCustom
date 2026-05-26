

#include "main.h"
#include "manager.h"
#include "Time.h"
#include <thread>
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include "renderer.h"

// アプリ全体の最初の入口。
// ここでは Windows のウィンドウ作成、DirectX/ImGui 初期化、60FPS のメインループだけを担当する。
// 実際のゲーム進行は Manager::Update / Manager::Draw から現在の Scene に委譲される。


const char* CLASS_NAME = "AppClass";
const char* WINDOW_NAME = "不思議のダンジョンCustom";



LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

/// <summary>
/// //
/// </summary>
/// <param name="hWnd"></param>
/// <returns></returns>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

HWND g_Window;

namespace Application
{
	HWND GetWindow()
	{
		return g_Window;
	}
}


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{

	// 1. Windows 側の準備。描画・ゲームロジックの前にウィンドウハンドルを確定させる。
	ImGui_ImplWin32_EnableDpiAwareness();
	WNDCLASSEX wcex;
	{
		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.style = 0;
		wcex.lpfnWndProc = WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInstance;
		wcex.hIcon = nullptr;
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = nullptr;
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = CLASS_NAME;
		wcex.hIconSm = nullptr;

		RegisterClassEx(&wcex);


		RECT rc = { 0, 0, (LONG)SCREEN_WIDTH, (LONG)SCREEN_HEIGHT };
		AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

		g_Window = CreateWindowEx(0, CLASS_NAME, WINDOW_NAME, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
			rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
		Renderer::SetWindowHandle(g_Window);
	}

	CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);



	ShowWindow(g_Window, SW_SHOWDEFAULT);
	UpdateWindow(g_Window);

	Renderer::Init();

	// 2. 描画補助 UI の初期化。各 Scene の Draw 後に Manager が ImGui を重ねて描画する。
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(g_Window);
	ImGui_ImplDX11_Init(Renderer::GetDevice(), Renderer::GetDeviceContext());
	
	Manager::InitImGuiFonts();



	Manager::Init();

	DWORD dwExecLastTime;
	DWORD dwCurrentTime;
	timeBeginPeriod(1);
	dwExecLastTime = timeGetTime();
	dwCurrentTime = 0;

	MSG msg;
	while (1)
	{
		// 3. OS メッセージを優先して処理し、空き時間にゲームを 1 フレーム進める。
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				break;
			}
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			dwCurrentTime = timeGetTime();

			if ((dwCurrentTime - dwExecLastTime) >= (1000 / 60))
			{
				dwExecLastTime = dwCurrentTime;

				// Time 更新 -> ゲーム状態更新 -> 描画、の順番を毎フレーム固定する。
				Time::Update();
				Manager::Update();
				Manager::Draw();
			}
		}
	}



	// 4. 生成と逆順に後片付けする。Manager::Uninit が現在 Scene と各管理クラスを破棄する。
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();


	
	timeEndPeriod(1);

	UnregisterClass(CLASS_NAME, wcex.hInstance);

	Manager::Uninit();

	CoUninitialize();

	return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
		return true;

	switch(uMsg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_KEYDOWN:
		switch(wParam)
		{
		case VK_ESCAPE:
			DestroyWindow(hWnd);
			break;
		}
		break;

	default:
		break;
	}
	
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

