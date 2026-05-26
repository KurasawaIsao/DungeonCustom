#pragma once
#include "GameObject.h"
#include "audio.h"

class SoundEffect : public GameObject
{
private:
    Audio* m_Audio = nullptr;
    std::string m_FilePath;
    bool m_IsLoop = false;

public:
    // 初期化
    void Init() override {
        // ComponentとしてAudioを生成 (Audioのコンストラクタに合わせて引数を調整)
        m_Audio = new Audio(this);
    }

    // 再生開始
    void Start(const char* fileName, bool loop) {
        m_IsLoop = loop;
        m_FilePath = fileName;

        // StartがInitより先に呼ばれるケースへの対策
        if (!m_Audio) {
            m_Audio = new Audio(this); 
        }

        m_Audio->Load(fileName); 
        m_Audio->Play(loop);      
    }


    void Update() override {
        // ループ再生でない場合、再生が終わったかチェック
        if (!m_IsLoop) {
            if (!m_Audio->IsPlaying()) {
                SetDestroy(); 
            }
        }
    }

    void Draw() override {} // 音だけなので何も描画しない


    void Uninit() override {
        if (m_Audio) {
            m_Audio->Stop();   
            m_Audio->Uninit();
            delete m_Audio;    
            m_Audio = nullptr; // 多重解放防止
        }
    }
    void Stop() {
        if (m_Audio) {
            m_Audio->Stop();
        }
	}
	bool GetIsLoop() const { return m_IsLoop; }
	const std::string& GetFilePath() const { return m_FilePath; }
};