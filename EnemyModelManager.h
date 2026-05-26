#pragma once
#include <unordered_map>
#include <string>
#include "animationModel.h"
#include "EnemyData.h"

class EnemyModelManager {
public:
    static EnemyModelManager& Instance() {
        static EnemyModelManager instance;
        return instance;
    }

    void LoadModel(const EnemyData& data) {
        // すでにロード済みなら何もしない(複数回呼ばれても無駄なリソースを確保するのを避けるため)
        if (m_Models.find(data.id) != m_Models.end()) return;

        AnimationModel* model = new AnimationModel();

        // モデル本体のロード
        model->Load(data.visual.modelPath.c_str());

        // アニメーション全種類を事前にロード
        model->LoadAnimation(data.visual.animIdle.c_str(), "Idle");
        model->LoadAnimation(data.visual.animRun.c_str(), "Run");
        model->LoadAnimation(data.visual.animAttack.c_str(), "Attack");
        model->LoadAnimation(data.visual.animDamaged.c_str(), "Damaged");
        model->LoadAnimation(data.visual.animSkill.c_str(), "Skill");
        model->LoadAnimation(data.visual.animSleep.c_str(), "Sleep");

        m_Models[data.id] = model;
    }

    // --- 取得関数：敵の生成時に呼ぶ ---
    AnimationModel* GetModel(const std::string& enemyId) {
        auto it = m_Models.find(enemyId);
        if (it != m_Models.end()) {
            return it->second;
        }
        // ロードされていない場合はnullptrを返すか、エラーログを出す
        return nullptr;
    }

    void Uninit() {
        for (auto& pair : m_Models) {
            pair.second->Uninit();
            delete pair.second;
        }
        m_Models.clear();
    }

private:
    EnemyModelManager() {}
    std::unordered_map<std::string, AnimationModel*> m_Models;
};