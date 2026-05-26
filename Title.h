#pragma once
#include "scene.h"
#include <string>
#include <vector>
enum class TitlePhase
{
	Character,
	Logo,
	UI,
	Done
};



class Title :public Scene
{
private:
	float m_TitleTime = 0.0f;

	float m_LogoTime = 0.0f;
	float m_LogoY = -200.0f; // ‰æ–ÊŠO
	
	float m_UITime = 0.0f;

	bool m_SceneStart = false;

	
	class TitleCharacter* m_Char1 = nullptr;
	class TitleCharacter* m_Char2 = nullptr;
	class TitleCharacter* m_Char3 = nullptr;

	class Audio* m_BGM;

	bool m_CameraStarted = false;
	bool m_Char1Started = false;
	bool m_Char2Started = false;
	bool m_Char3Started = false;
	bool m_UIStarted = false;

	bool m_EditorScene = false;
	TitlePhase m_Phase = TitlePhase::Character;

	std::string m_DungeonId = "default";
	char m_DungeonIdBuf[256]{};

	std::vector<std::string> m_DungeonFiles; 
	int m_SelectedDungeonIndex = 0;         
	void RefreshDungeonList();
	std::string m_SelectedDungeonId;
	int m_TitleMenuCursor = 0;
	bool m_DungeonDropdownOpen = false;
	float m_DungeonSelectAlpha = 0.0f;

	void UpdateDungeonSelectFade();
	void UpdateDungeonSelectInput();
	void DrawDungeonSelectUI();
	void StartSelectedDungeon();
	void StartEditorScene();
	const std::string& GetSelectedDungeonName() const;

public:
	void Init()override;
	void Update()override;
	void Draw()override;
	void Uninit()override;
	void UpdateTimeline();
};
