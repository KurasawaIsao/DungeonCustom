# Enemy/Ally Role Notes and Refactor Plan

## 現状の役割

### Unit

- 盤面上に存在する戦闘ユニットの共通基盤。
- HP、能力値、状態異常、ターン予算、グリッド移動、アニメーション、ダメージ処理を持つ。
- 「誰を敵とみなすか」「死亡時に何が起きるか」「どの AI で動くか」は持たない。

### EnemyD

- 敵勢力の Unit。
- `EnemyData` からステータス、モデル、AI 種別、ドロップ、経験値、勧誘補正を読み込む。
- プレイヤーと Ally を敵対対象として索敵し、敵用 AI で行動する。
- 死亡時はドロップ、勧誘メニュー、敵リストからの削除を担当する。
- ShopKeeper など、敵勢力の例外状態もここに含まれている。

### Ally

- 味方勢力の Unit。
- 勧誘時に EnemyD のデータを受け取り、味方として再生成される。
- プレイヤー追従、AI モード切替、会話、味方マーク表示を担当する。
- EnemyD を敵対対象として索敵し、敵撃破時の経験値処理を担当する。

### UnitAI

- Unit を動かすための共通判断部品。
- 視線判定、隣接判定、経路探索、スキル実行など、敵味方共通の低レベル処理を持つ。
- 勢力判定や報酬/ドロップなどのゲームルールは持たせない。

## 冗長化している箇所

- `EnemyD::UpdateUnit` と `Ally::UpdateUnit` は、行動フェーズの後に移動フェーズを呼ぶ構造が同じ。
- `EnemyD::UpdateActionPhase` と `Ally::UpdateActionPhase` は、状態異常処理、行動済みチェック、スキル、通常攻撃の流れが近い。
- `EnemyD::UpdateMovePhase` と `Ally::UpdateMovePhase` は、移動可否、状態異常、移動失敗時の予算消費が近い。
- `EnemyD.cpp` の `MoveToHostileLikeAlly` 系と `Ally.cpp` の `MoveToPlayerByArea` 系は、目標候補の作り方と隣接位置への移動がほぼ同型。
- `FindVisibleHostileTarget` と `FindVisibleEnemy` は、対象リストと認識ルールだけが違う。
- `Attack` は、角抜け不可、命中判定、ダメージ適用、ターン終了の基本部分が重複している。

## リファクタ方針

1. まず勢力差分を明文化する。

- `EnemyD` 固有: `EnemyData` 適用、ドロップ、勧誘、敵 AI 種別、店主状態、敵死亡ログ。
- `Ally` 固有: プレイヤー追従、AI モード、会話、味方マーク、仲間死亡処理、撃破経験値配布。
- 共通候補: フェーズ進行、状態異常ガード、通常攻撃、接近移動、視線/認識の枠組み。

2. 小さい共通関数から抜き出す。

- `Unit` に通常攻撃の共通処理を追加する。
- 例: `TryMeleeAttack(Unit* target, bool consumeTurn, bool showAnimation)`。
- EnemyD/Ally は「攻撃後の追加処理」だけを残す。

3. ターゲット探索をポリシー化する。

- `UnitManager` に勢力を意識した検索 API を追加する。
- 例: `GetHostileUnits(Unit& self)`、`GetVisibleHostile(Unit& self, const RecognitionRule& rule)`。
- 直接 `GetEnemies()` / `GetAllies()` を回す箇所を段階的に置き換える。

4. 追従/接近移動を共通サービスに寄せる。

- `ChaseAI` か新規 `UnitMovementPlanner` に、隣接ゴール生成、ライン位置生成、候補スコアリング、移動開始を集約する。
- EnemyD 側の `MoveToHostileLikeAlly` と Ally 側の `MoveToPlayerByArea` は、呼び出しだけに薄くする。

5. フェーズ処理をテンプレート化する。

- `Unit` または新規 `UnitTurnController` に「行動フェーズ、移動フェーズ、失敗時予算消費」の共通骨格を置く。
- 派生クラスは `SelectActionTarget`、`ActOnTarget`、`MoveByPolicy` のような差分フックを実装する。

6. 最後に EnemyD/Ally を薄くする。

- EnemyD は「敵データを持つ勢力ユニット」へ。
- Ally は「仲間モードと会話を持つ勢力ユニット」へ。
- 共通 AI/移動/戦闘ルールは Unit、UnitAI、MovementPlanner、TargetPolicy に寄せる。

## 推奨実装順

1. 通常攻撃の共通化。
2. UnitManager に敵対対象取得 API を追加。
3. 可視ターゲット探索を共通化。
4. 隣接目標への移動とライン追従を共通化。
5. UpdateActionPhase / UpdateMovePhase のフェーズ骨格を共通化。
6. RecruitManager の EnemyD 依存を EnemyData 由来の初期化データへ寄せる。

## 注意点

- 一気に継承階層を増やすより、先に関数単位で重複を減らす。
- `EnemyD` から `Ally` を生成する勧誘フローは壊れやすいので、最後の段階まで大きく触らない。
- `UnitAI` に勢力判定を入れると再び肥大化するため、敵味方の解決はポリシーか UnitManager 側に置く。
- 既存のターン予算は繊細なので、各段階でビルドして、倍速/鈍足/状態異常/睡眠明けを確認する。
