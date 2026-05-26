#include "EffectManager.h"
#include "manager.h"
#include "scene.h"
#include "EffectBillboard.h"
#include "audio.h"
#include "SoundEffect.h"

void EffectManager::PlaySE(const char* sePath) {
    SoundEffect* se = Manager::GetScene()->AddGameObject<SoundEffect>(1);
    se->Start(sePath, false);
}

void EffectManager::PlayBGM(const char* sePath)
{
    auto bgms = Manager::GetScene()->GetGameObjects<SoundEffect>();
    SoundEffect* targetBGM = nullptr;

    // 1. まず現在再生中のBGM（Loop属性）があるか探す
    for (auto* bgm : bgms) {
        if (bgm->GetIsLoop()) {
            targetBGM = bgm;
            break;
        }
    }

    if (targetBGM) {
        // 既存のBGMと違うパスなら切り替え
        if (targetBGM->GetFilePath() != sePath) {
            targetBGM->Stop();
            targetBGM->Start(sePath, true);
        }
    }
    else {
        // BGMが一つもないなら新規作成
        SoundEffect* newBgm = Manager::GetScene()->AddGameObject<SoundEffect>(1);
        // 重要：Initが呼ばれる前にStartを呼んでも大丈夫なようにする（下記SoundEffectの修正参照）
        newBgm->Start(sePath, true);
    }
}

void EffectManager::Play(Vector3 pos, const char* texPath, const char* sePath) {
    // 見た目の生成
    if (texPath) {
        pos.y += 0.5f;
        EffectBillboard::Create(pos, texPath, 4, 4, 1.0f);
    }
    // 音の生成
    if (sePath) {
        PlaySE(sePath);
    }
}
EffectBillboard* EffectManager::CreateLoopEffect(Vector3 pos, const char* texPath) {
    EffectBillboard* eb = EffectBillboard::Create(pos, texPath, 4, 4, 1.0f);
    eb->SetLoop(true);
    return eb;
}
EffectBillboard* EffectManager::CreateSpriteEffect(Vector3 pos, const char* texPath) {
    EffectBillboard* eb = EffectBillboard::Create(pos, texPath, 1, 1, 1.0f);
    eb->SetLoop(true);
    return eb;
}