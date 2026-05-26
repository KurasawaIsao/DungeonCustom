#pragma once
#include "imgui.h"
class Scene;  // ëOï˚êÈåæ

class Manager
{
private:
	static class Scene* m_scene;
	static class Scene* m_Nextscene;
	static ImFont* fontDebug;
	static ImFont* fontLog;

public:
	static void Init();
	static void Uninit();
	static void Update();
	static void Draw();
	static void InitImGuiFonts();
	static void Drawgui();
	static void DrawOutlinedText(
		ImDrawList* dl,
		const ImVec2& pos,
		ImU32 color,
		const char* text);

	static void DrawGaugeBar(
		ImDrawList* dl,
		const ImVec2& pos,
		const ImVec2& size,
		float rate,
		ImU32 backColor,
		ImU32 fillColor,
		ImU32 frameColor);

	static Scene* GetScene() { return m_scene; }
	template <typename T>
	static void SetScene()
	{
		m_Nextscene = new T();
	}
	static ImFont* GetDebugFont() { return fontDebug; };
	static ImFont* GetLogFont() { return fontLog; };
};