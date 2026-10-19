// Shadow Walker - 影から影へ飛び移るステルスゲーム (C++ / DxLib)
// 文字コードは UTF-8。VisualStudio では C/C++ > コマンドライン に /utf-8 を追加してください。
#include "DxLib.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <Windows.h>
namespace {

    constexpr int TILE = 32;
    constexpr int MAP_W = 24;
    constexpr int MAP_H = 18;
    constexpr int HUD_H = 48;
    constexpr int SCREEN_W = TILE * MAP_W;
    constexpr int SCREEN_H = TILE * MAP_H + HUD_H;
    constexpr int STAGE_COUNT = 3;
    constexpr float PI = 3.14159265f;

    constexpr float FOV = 70.0f * PI / 180.0f;  // 敵の視野角
    constexpr float NEAR_DIST = 36.0f;          // 影の中でも気配で気づかれる距離
    constexpr float PLAYER_R = 10.0f;
    constexpr float WALK_SPEED = 130.0f;
    constexpr float SNEAK_SPEED = 65.0f;
    constexpr float ENEMY_SPEED = 55.0f;

    // 地形記号
    //  #:壁  .:明るい床  s:影  S:スタート  G:ゴール
    //  h:横巡回の敵  v:縦巡回の敵  r:その場で回転する敵
    // 1行は 8文字 x 3 = 24文字
    const char* const STAGES[STAGE_COUNT][MAP_H] = {
        {
            // ---- STAGE 1 ----
            "########" "########" "########",
            "#S......" "........" ".......#",
            "#.ss...." "........" "....ss.#",
            "#.ss...." "........" "....ss.#",
            "#......." "........" ".......#",
            "#......." "........" ".......#",
            "#...ss.." "........" ".ss....#",
            "#...ss.." "........" ".ss....#",
            "#......." "........" ".......#",
            "#......." "h......." ".......#",
            "#......." "........" ".......#",
            "#.ss...." "..ss...." "....ss.#",
            "#.ss...." "..ss...." "....ss.#",
            "#......." "........" ".......#",
            "#......." "........" ".......#",
            "#......." "....r..." "......s#",
            "#......." "........" "......G#",
            "########" "########" "########",
        },
        {
            // ---- STAGE 2 ----
            "########" "########" "########",
            "#S......" "...#...." ".......#",
            "#......." "...#...." ".......#",
            "#.ss...." "...#.ss." ".......#",
            "#.ss...." "...#.ss." "..v....#",
            "#......." "...#...." ".......#",
            "#......." "........" ".......#",
            "####.###" "###.####" "#.######",
            "#......." "........" ".......#",
            "#.ss...." "..h....." "...ss..#",
            "#.ss...." "........" "...ss..#",
            "#......." "........" ".......#",
            "####.###" "###..###" "#.######",
            "#......." "........" ".......#",
            "#..ss..." ".r......" "..ss...#",
            "#..ss..." "........" "..ss...#",
            "#......." "........" "......G#",
            "########" "########" "########",
        },
        {
            // ---- STAGE 3 ----
            "########" "########" "########",
            "#S......" "........" ".......#",
            "#......." "........" ".......#",
            "#.ss...." "..v....." "..ss...#",
            "#.ss...." "........" "..ss...#",
            "#......." "........" ".......#",
            "#...##.." "..##...." "##...s.#",
            "#...##.." "..##...." "##.....#",
            "#......." "........" ".......#",
            "#..ss..." "h......." "...ss..#",
            "#..ss..." "........" "...ss..#",
            "#......." "........" ".......#",
            "#.##...." "...r...." "....##.#",
            "#.##...." "........" "....##.#",
            "#......." "........" ".......#",
            "#.ss...." "..v....." "...ss..#",
            "#.ss...." "........" "...ss.G#",
            "########" "########" "########",
        },
    };

    enum class Scene { Title, Play, Clear, GameOver, AllClear };

    float NormAngle(float a) {
        while (a > PI) a -= 2.0f * PI;
        while (a < -PI) a += 2.0f * PI;
        return a;
    }

    struct Input {
        char cur[256];
        char prev[256];
        Input() {
            std::memset(cur, 0, sizeof(cur));
            std::memset(prev, 0, sizeof(prev));
        }
        void Update() {
            std::memcpy(prev, cur, sizeof(cur));
            GetHitKeyStateAll(cur);
        }
        bool Down(int k) const { return cur[k] != 0; }
        bool Pressed(int k) const { return cur[k] != 0 && prev[k] == 0; }
    };

    struct Enemy {
        float x = 0, y = 0;    // 中心座標(px)
        float angle = 0;       // 向いている角度(rad)
        float dx = 0, dy = 0;  // 巡回方向
        float wait = 0;        // 折り返し時の待機時間
        char type = 'h';
        bool alert = false;    // プレイヤーを視認中
    };

    class Game {
    public:
        void Run();

    private:
        Scene scene_ = Scene::Title;
        int stage_ = 0;
        char tiles_[MAP_H][MAP_W];
        float px_ = 0, py_ = 0;
        bool sneaking_ = false;
        bool inShadow_ = false;
        float gauge_ = 0;       // 発見度 0..1
        float stageTime_ = 0;
        float totalTime_ = 0;
        std::vector<Enemy> enemies_;
        Input in_;

        void LoadStage(int index);
        bool IsWall(float x, float y) const;
        bool IsShadow(float x, float y) const;
        bool Collides(float x, float y) const;
        bool LineClear(float x0, float y0, float x1, float y1) const;
        bool CanSee(const Enemy& e) const;
        float EnemyRange() const { return 150.0f + 15.0f * stage_; }
        void RayEnd(const Enemy& e, float ang, float& ox, float& oy) const;

        void Update(float dt);
        void UpdatePlay(float dt);
        void UpdateEnemy(Enemy& e, float dt);

        void Draw();
        void DrawField();
        void DrawFan(const Enemy& e);
        void DrawHud();
        void DrawOverlay(const char* title, const char* sub, unsigned int color);
        static void DrawCentered(int y, const char* text, unsigned int color, int size);
    };

    // ------------------------------------------------------------
    // ステージ読み込み
    // ------------------------------------------------------------
    void Game::LoadStage(int index) {
        enemies_.clear();
        gauge_ = 0;
        stageTime_ = 0;
        for (int y = 0; y < MAP_H; ++y) {
            const char* row = STAGES[index][y];
            const int len = static_cast<int>(std::strlen(row));
            for (int x = 0; x < MAP_W; ++x) {
                char c = (x < len) ? row[x] : '#';
                const float cx = x * TILE + TILE * 0.5f;
                const float cy = y * TILE + TILE * 0.5f;
                if (c == 'S') {
                    px_ = cx;
                    py_ = cy;
                    c = '.';
                }
                else if (c == 'h' || c == 'v' || c == 'r') {
                    Enemy e;
                    e.type = c;
                    e.x = cx;
                    e.y = cy;
                    if (c == 'h') { e.dx = 1; e.dy = 0; }
                    if (c == 'v') { e.dx = 0; e.dy = 1; }
                    e.angle = std::atan2(e.dy, e.dx);
                    enemies_.push_back(e);
                    c = '.';
                }
                tiles_[y][x] = c;
            }
        }
    }

    // ------------------------------------------------------------
    // 判定系
    // ------------------------------------------------------------
    bool Game::IsWall(float x, float y) const {
        const int tx = static_cast<int>(std::floor(x / TILE));
        const int ty = static_cast<int>(std::floor(y / TILE));
        if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H) return true;
        return tiles_[ty][tx] == '#';
    }

    bool Game::IsShadow(float x, float y) const {
        const int tx = static_cast<int>(std::floor(x / TILE));
        const int ty = static_cast<int>(std::floor(y / TILE));
        if (tx < 0 || ty < 0 || tx >= MAP_W || ty >= MAP_H) return false;
        return tiles_[ty][tx] == 's';
    }

    bool Game::Collides(float x, float y) const {
        return IsWall(x - PLAYER_R, y - PLAYER_R) || IsWall(x + PLAYER_R, y - PLAYER_R) ||
            IsWall(x - PLAYER_R, y + PLAYER_R) || IsWall(x + PLAYER_R, y + PLAYER_R);
    }

    bool Game::LineClear(float x0, float y0, float x1, float y1) const {
        const float dx = x1 - x0, dy = y1 - y0;
        const float dist = std::sqrt(dx * dx + dy * dy);
        const int steps = static_cast<int>(dist / 4.0f) + 1;
        for (int i = 1; i <= steps; ++i) {
            const float t = static_cast<float>(i) / steps;
            if (IsWall(x0 + dx * t, y0 + dy * t)) return false;
        }
        return true;
    }

    bool Game::CanSee(const Enemy& e) const {
        const float dx = px_ - e.x, dy = py_ - e.y;
        const float dist = std::sqrt(dx * dx + dy * dy);

        if (inShadow_) {
            // 影の中: 忍び足なら絶対に見つからない。歩くと至近距離では気づかれる
            if (sneaking_ || dist > NEAR_DIST) return false;
            return LineClear(e.x, e.y, px_, py_);
        }
        if (dist > EnemyRange()) return false;
        const bool inCone = std::fabs(NormAngle(std::atan2(dy, dx) - e.angle)) <= FOV * 0.5f;
        if (!inCone && dist > 20.0f) return false;
        return LineClear(e.x, e.y, px_, py_);
    }

    void Game::RayEnd(const Enemy& e, float ang, float& ox, float& oy) const {
        const float range = EnemyRange();
        const float cs = std::cos(ang), sn = std::sin(ang);
        float d = 0;
        while (d < range) {
            const float nd = d + 4.0f;
            if (IsWall(e.x + cs * nd, e.y + sn * nd)) break;
            d = nd;
        }
        ox = e.x + cs * d;
        oy = e.y + sn * d;
    }

    // ------------------------------------------------------------
    // 更新
    // ------------------------------------------------------------
    void Game::Run() {
        int last = GetNowCount();
        while (ProcessMessage() == 0) {
            const int now = GetNowCount();
            float dt = (now - last) / 1000.0f;
            last = now;
            if (dt > 0.05f) dt = 0.05f;

            in_.Update();
            if (in_.Pressed(KEY_INPUT_ESCAPE)) break;
            Update(dt);

            ClearDrawScreen();
            Draw();
            ScreenFlip();
        }
    }

    void Game::Update(float dt) {
        const bool ok = in_.Pressed(KEY_INPUT_RETURN) || in_.Pressed(KEY_INPUT_SPACE);
        switch (scene_) {
        case Scene::Title:
            if (ok) {
                stage_ = 0;
                totalTime_ = 0;
                LoadStage(stage_);
                scene_ = Scene::Play;
            }
            break;
        case Scene::Play:
            UpdatePlay(dt);
            break;
        case Scene::Clear:
            if (ok) {
                ++stage_;
                LoadStage(stage_);
                scene_ = Scene::Play;
            }
            break;
        case Scene::GameOver:
            if (ok) {
                LoadStage(stage_);
                scene_ = Scene::Play;
            }
            break;
        case Scene::AllClear:
            if (ok) scene_ = Scene::Title;
            break;
        }
    }

    void Game::UpdatePlay(float dt) {
        if (in_.Pressed(KEY_INPUT_R)) {
            LoadStage(stage_);
            return;
        }
        stageTime_ += dt;

        // ---- プレイヤー移動 ----
        sneaking_ = in_.Down(KEY_INPUT_LSHIFT) || in_.Down(KEY_INPUT_RSHIFT);
        float mx = 0, my = 0;
        if (in_.Down(KEY_INPUT_LEFT) || in_.Down(KEY_INPUT_A)) mx -= 1;
        if (in_.Down(KEY_INPUT_RIGHT) || in_.Down(KEY_INPUT_D)) mx += 1;
        if (in_.Down(KEY_INPUT_UP) || in_.Down(KEY_INPUT_W)) my -= 1;
        if (in_.Down(KEY_INPUT_DOWN) || in_.Down(KEY_INPUT_S)) my += 1;
        if (mx != 0 && my != 0) {
            mx *= 0.7071f;
            my *= 0.7071f;
        }
        const float speed = sneaking_ ? SNEAK_SPEED : WALK_SPEED;
        const float nx = px_ + mx * speed * dt;
        if (!Collides(nx, py_)) px_ = nx;
        const float ny = py_ + my * speed * dt;
        if (!Collides(px_, ny)) py_ = ny;

        inShadow_ = IsShadow(px_, py_);

        // ---- 敵 ----
        bool seen = false;
        for (Enemy& e : enemies_) {
            UpdateEnemy(e, dt);
            e.alert = CanSee(e);
            if (e.alert) seen = true;
        }

        // ---- 発見度 ----
        if (seen) gauge_ += dt * 1.2f;
        else gauge_ -= dt * 0.6f;
        if (gauge_ < 0) gauge_ = 0;
        if (gauge_ >= 1.0f) {
            gauge_ = 1.0f;
            scene_ = Scene::GameOver;
            return;
        }

        // ---- ゴール判定 ----
        const int tx = static_cast<int>(px_ / TILE);
        const int ty = static_cast<int>(py_ / TILE);
        if (tx >= 0 && ty >= 0 && tx < MAP_W && ty < MAP_H && tiles_[ty][tx] == 'G') {
            totalTime_ += stageTime_;
            scene_ = (stage_ + 1 >= STAGE_COUNT) ? Scene::AllClear : Scene::Clear;
        }
    }

    void Game::UpdateEnemy(Enemy& e, float dt) {
        if (e.type == 'r') {
            e.angle = NormAngle(e.angle + 0.9f * dt);
            return;
        }

        if (e.wait > 0) {
            e.wait -= dt;
        }
        else {
            const float nx = e.x + e.dx * ENEMY_SPEED * dt;
            const float ny = e.y + e.dy * ENEMY_SPEED * dt;
            if (IsWall(nx + e.dx * 16.0f, ny + e.dy * 16.0f)) {
                e.dx = -e.dx;
                e.dy = -e.dy;
                e.wait = 0.8f;
            }
            else {
                e.x = nx;
                e.y = ny;
            }
        }

        // 進行方向へ滑らかに向きを変える
        const float target = std::atan2(e.dy, e.dx);
        const float diff = NormAngle(target - e.angle);
        const float step = 5.0f * dt;
        if (std::fabs(diff) <= step) e.angle = target;
        else e.angle = NormAngle(e.angle + (diff > 0 ? step : -step));
    }

    // ------------------------------------------------------------
    // 描画
    // ------------------------------------------------------------
    void Game::DrawCentered(int y, const char* text, unsigned int color, int size) {
        SetFontSize(size);
        const int w = GetDrawStringWidth(text, static_cast<int>(std::strlen(text)));
        DrawString((SCREEN_W - w) / 2, y, text, color);
        SetFontSize(18);
    }

    void Game::Draw() {
        if (scene_ == Scene::Title) {
            DrawBox(0, 0, SCREEN_W, SCREEN_H, GetColor(15, 15, 22), TRUE);
            DrawCentered(150, "SHADOW WALKER", GetColor(120, 220, 255), 56);
            DrawCentered(230, "影から影へ。見つからずにゴールを目指せ", GetColor(210, 210, 220), 22);
            DrawCentered(330, "移動: 矢印キー / WASD", GetColor(180, 180, 190), 20);
            DrawCentered(360, "Shift: 忍び足（影の中なら完全に見つからない）", GetColor(180, 180, 190), 20);
            DrawCentered(390, "R: ステージやり直し   Esc: 終了", GetColor(180, 180, 190), 20);
            DrawCentered(480, "Enter か Space でスタート", GetColor(255, 230, 120), 24);
            return;
        }

        DrawField();
        DrawHud();

        if (scene_ == Scene::Clear) {
            DrawOverlay("STAGE CLEAR!", "Enter で次のステージへ", GetColor(120, 255, 160));
        }
        else if (scene_ == Scene::GameOver) {
            DrawOverlay("発見された！", "Enter でリトライ", GetColor(255, 100, 100));
        }
        else if (scene_ == Scene::AllClear) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "合計タイム %.1f 秒   Enter でタイトルへ", totalTime_);
            DrawOverlay("ALL CLEAR!", buf, GetColor(255, 230, 120));
        }
    }

    void Game::DrawField() {
        // 床・壁・影・ゴール
        for (int y = 0; y < MAP_H; ++y) {
            for (int x = 0; x < MAP_W; ++x) {
                const int px = x * TILE, py = y * TILE;
                const char c = tiles_[y][x];
                unsigned int col;
                switch (c) {
                case '#': col = GetColor(105, 105, 130); break;
                case 's': col = GetColor(12, 12, 18); break;
                case 'G': col = GetColor(50, 170, 90); break;
                default:  col = ((x + y) % 2 == 0) ? GetColor(78, 78, 92) : GetColor(72, 72, 86); break;
                }
                DrawBox(px, py, px + TILE, py + TILE, col, TRUE);
                if (c == '#') {
                    DrawBox(px, py, px + TILE, py + TILE, GetColor(70, 70, 90), FALSE);
                }
                else if (c == 'G') {
                    DrawBox(px + 6, py + 6, px + TILE - 6, py + TILE - 6, GetColor(200, 255, 210), FALSE);
                }
            }
        }

        // 視界（壁で遮られる）
        for (const Enemy& e : enemies_) DrawFan(e);

        // 敵
        for (const Enemy& e : enemies_) {
            const unsigned int body = e.alert ? GetColor(255, 60, 60) : GetColor(230, 140, 60);
            DrawCircle(static_cast<int>(e.x), static_cast<int>(e.y), 11, body, TRUE);
            DrawCircle(static_cast<int>(e.x), static_cast<int>(e.y), 11, GetColor(40, 20, 10), FALSE);
            DrawLine(static_cast<int>(e.x), static_cast<int>(e.y),
                static_cast<int>(e.x + std::cos(e.angle) * 16),
                static_cast<int>(e.y + std::sin(e.angle) * 16), GetColor(255, 255, 255), 2);
        }

        // プレイヤー
        const int ipx = static_cast<int>(px_), ipy = static_cast<int>(py_);
        if (inShadow_) {
            SetDrawBlendMode(DX_BLENDMODE_ALPHA, 150);
            DrawCircle(ipx, ipy, static_cast<int>(PLAYER_R), GetColor(90, 140, 190), TRUE);
            SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        }
        else {
            DrawCircle(ipx, ipy, static_cast<int>(PLAYER_R), GetColor(110, 220, 255), TRUE);
        }
        DrawCircle(ipx, ipy, static_cast<int>(PLAYER_R), GetColor(20, 40, 60), FALSE);
        if (sneaking_) DrawCircle(ipx, ipy, static_cast<int>(PLAYER_R) + 4, GetColor(200, 200, 255), FALSE);
    }

    void Game::DrawFan(const Enemy& e) {
        const int N = 28;
        const float span = FOV;
        const unsigned int col = e.alert ? GetColor(255, 60, 60) : GetColor(255, 235, 120);

        SetDrawBlendMode(DX_BLENDMODE_ALPHA, e.alert ? 110 : 75);
        float prevX = 0, prevY = 0;
        for (int i = 0; i <= N; ++i) {
            const float a = e.angle - span * 0.5f + span * i / N;
            float ox, oy;
            RayEnd(e, a, ox, oy);
            if (i > 0) {
                DrawTriangle(static_cast<int>(e.x), static_cast<int>(e.y),
                    static_cast<int>(prevX), static_cast<int>(prevY),
                    static_cast<int>(ox), static_cast<int>(oy), col, TRUE);
            }
            prevX = ox;
            prevY = oy;
        }
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
    }

    void Game::DrawHud() {
        const int top = MAP_H * TILE;
        DrawBox(0, top, SCREEN_W, SCREEN_H, GetColor(20, 20, 30), TRUE);

        DrawFormatString(12, top + 14, GetColor(230, 230, 240), "STAGE %d/%d   TIME %.1f",
            stage_ + 1, STAGE_COUNT, stageTime_);

        // 発見度ゲージ
        const int gx = 330, gy = top + 12, gw = 200, gh = 22;
        DrawString(gx - 70, top + 14, "発見度", GetColor(230, 230, 240));
        DrawBox(gx, gy, gx + gw, gy + gh, GetColor(50, 50, 60), TRUE);
        const int r = static_cast<int>(120 + 135 * gauge_);
        const int g = static_cast<int>(220 - 160 * gauge_);
        DrawBox(gx, gy, gx + static_cast<int>(gw * gauge_), gy + gh, GetColor(r, g, 60), TRUE);
        DrawBox(gx, gy, gx + gw, gy + gh, GetColor(200, 200, 210), FALSE);

        const char* state = inShadow_ ? "影の中" : "明るい場所";
        DrawString(580, top + 14, state, inShadow_ ? GetColor(140, 190, 255) : GetColor(255, 220, 140));
        if (sneaking_) DrawString(680, top + 14, "忍び足", GetColor(200, 200, 255));
    }

    void Game::DrawOverlay(const char* title, const char* sub, unsigned int color) {
        SetDrawBlendMode(DX_BLENDMODE_ALPHA, 170);
        DrawBox(0, 0, SCREEN_W, TILE * MAP_H, GetColor(0, 0, 0), TRUE);
        SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 0);
        DrawCentered(220, title, color, 52);
        DrawCentered(300, sub, GetColor(230, 230, 240), 24);
    }

}  // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    SetOutApplicationLogValidFlag(FALSE);
    ChangeWindowMode(TRUE);
    SetGraphMode(SCREEN_W, SCREEN_H, 32);
    SetMainWindowText("Shadow Walker");
    SetUseCharCodeFormat(DX_CHARCODEFORMAT_UTF8);
    if (DxLib_Init() == -1) return -1;
    SetDrawScreen(DX_SCREEN_BACK);
    SetFontSize(18);

    Game game;
    game.Run();

    DxLib_End();
    return 0;
}



int main() {
    // WinMain を直接呼び出して既存のエントリコードを使う
    return WinMain(GetModuleHandle(NULL), NULL, GetCommandLineA(), SW_SHOWDEFAULT);
}
