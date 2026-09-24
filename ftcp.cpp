#ifdef _WIN32
#include <windows.h> //最優先で読み込む必要がある
#include <psapi.h>
#endif

#include "foldsToEdges.h"
#include "ftcp.h"

#ifdef _OPENMP
#include <omp.h>
#endif

#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <deque>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <vector>

using namespace std;

#define UP 0
#define UPPER_RIGHT 1
#define RIGHT 2
#define LOWER_RIGHT 3
#define DOWN 4
#define LOWER_LEFT 5
#define LEFT 6
#define UPPER_LEFT 7
#define DIR_MAX 8

const int DIRECTIONS[8][2] = {
    {0, -1}, // UP
    {1, -1}, // UR
    {1, 0},  // R
    {1, 1},  // LR
    {0, 1},  // DN
    {-1, 1}, // LL
    {-1, 0}, // L
    {-1, -1} // UL
};

const int TILE[36][8] = {{0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 1, 0, 0, 0, 1},
                         {0, 0, 1, 0, 0, 0, 1, 0}, {0, 0, 1, 0, 0, 1, 1, 1},
                         {0, 0, 1, 0, 1, 1, 0, 1}, {0, 0, 1, 1, 1, 0, 0, 1},
                         {0, 1, 0, 0, 0, 1, 0, 0}, {0, 1, 0, 0, 1, 0, 1, 1},
                         {0, 1, 0, 0, 1, 1, 1, 0}, {0, 1, 0, 1, 0, 1, 0, 1},
                         {0, 1, 0, 1, 1, 0, 1, 0}, {0, 1, 0, 1, 1, 1, 1, 1},
                         {0, 1, 1, 0, 1, 0, 0, 1}, {0, 1, 1, 1, 0, 0, 1, 0},
                         {0, 1, 1, 1, 0, 1, 1, 1}, {0, 1, 1, 1, 1, 1, 0, 1},
                         {1, 0, 0, 0, 1, 0, 0, 0}, {1, 0, 0, 1, 0, 0, 1, 1},
                         {1, 0, 0, 1, 0, 1, 1, 0}, {1, 0, 0, 1, 1, 1, 0, 0},
                         {1, 0, 1, 0, 0, 1, 0, 1}, {1, 0, 1, 0, 1, 0, 1, 0},
                         {1, 0, 1, 0, 1, 1, 1, 1}, {1, 0, 1, 1, 0, 1, 0, 0},
                         {1, 0, 1, 1, 1, 0, 1, 1}, {1, 0, 1, 1, 1, 1, 1, 0},
                         {1, 1, 0, 0, 1, 0, 0, 1}, {1, 1, 0, 1, 0, 0, 1, 0},
                         {1, 1, 0, 1, 0, 1, 1, 1}, {1, 1, 0, 1, 1, 1, 0, 1},
                         {1, 1, 1, 0, 0, 1, 0, 0}, {1, 1, 1, 0, 1, 0, 1, 1},
                         {1, 1, 1, 0, 1, 1, 1, 0}, {1, 1, 1, 1, 0, 1, 0, 1},
                         {1, 1, 1, 1, 1, 0, 1, 0}, {1, 1, 1, 1, 1, 1, 1, 1}};

void PrintMemoryUsage() {
#ifdef _WIN32
    // 自身のプロセスハンドルを取得
    HANDLE hProcess = GetCurrentProcess();
    PROCESS_MEMORY_COUNTERS pmc;

    if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
        std::cout << "--- メモリ使用量 ---" << std::endl;

        // **ワーキングセットサイズ (WorkingSetSize)**
        // 物理メモリ (RAM) で現在使用されているメモリ量
        // これがタスクマネージャーで見られる「メモリ」の主要な数値に近いです。
        std::cout << "ワーキングセット (RAM): " << (pmc.WorkingSetSize / 1024)
                  << " KB" << std::endl;

        // **ページファイル使用量 (PagefileUsage)**
        // コミットされた（予約された）メモリのうち、現在ページファイルまたは物理メモリに存在している量
        std::cout << "コミットされたメモリ: " << (pmc.PagefileUsage / 1024)
                  << " KB" << std::endl;

        // **ピークワーキングセットサイズ (PeakWorkingSetSize)**
        // 実行中に達した最大のワーキングセットサイズ
        std::cout << "ピークワーキングセット: "
                  << (pmc.PeakWorkingSetSize / 1024) << " KB" << std::endl;

        // その他、必要に応じてpmcの他のメンバーも参照できます。
    } else {
        std::cerr << "メモリ情報の取得に失敗しました。" << std::endl;
    }
#else
    std::cout << "Memory usage reporting is only available on Windows."
              << std::endl;
#endif
}

Counter::Counter(int width) {
    w = width;
    mate_size = 3 * w - 1;
    state_size = 1ULL << mate_size;
    tileConditon = vector<vector<int>>(w * w, vector<int>(36, 1));

    // cell_x, cell_yの初期化
    for (int cell = 0; cell < 49; cell++) {
        cell_x[cell] = cell % w;
        cell_y[cell] = cell / w;
    }

    // boundsの初期化
    for (int cell = 0; cell < 49; cell++) {
        int x = cell_x[cell];
        int y = cell_y[cell];

        // 設置判定をする方向を計算
        int l = 1, ul = 1, u = 1, ur = 1;
        if (y == 0)
            ul = 0, u = 0, ur = 0;
        if (x == 0)
            l = 0, ul = 0;
        if (x == w - 1)
            ur = 0;
        d_mask[cell] = l | (ul << 1) | (u << 2) | (ur << 3);
    }

    // tile_4_edgesの初期化
    for (int tile = 0; tile < 36; tile++) {
        // tileの4方向の辺を得る
        int tl = TILE[tile][LEFT];
        int tul = TILE[tile][UPPER_LEFT];
        int tu = TILE[tile][UP];
        int tur = TILE[tile][UPPER_RIGHT];
        tile_4_edges[tile] = tl | (tul << 1) | (tu << 2) | (tur << 3);
    }
}

void Counter::setTileCondition(int cell, int tile, int value) {
    tileConditon[cell][tile] = value;
}

int Counter::get_binary_digit(unsigned long long n, int d) {
    return (n >> d) & 1;
}

unsigned long long Counter::set_bit(unsigned long long n, int d, int v) {
    unsigned long long m = n;
    unsigned long long mask = ~(1ULL << d);
    m = m & mask;
    unsigned long long bit = (unsigned long long)v << d;
    return m | bit;
}

// 設置判定するときと置くときで座標が異なる
// => どっちの使用状況か引数で得る必要がある
// => あるいは何番目のタイルまで設置したか
int Counter::get_index(int dir, int x) {
    if (dir == UP)
        return 3 * x;
    if (dir == UPPER_RIGHT)
        return min(3 * x + 1, 3 * w - 3);
    if (dir == UPPER_LEFT)
        return max(3 * x - 1, 0);
    if (dir == LEFT)
        return max(3 * x - 2, 0);

    return 0;
}

bool Counter::can_put(int cell, int tile, unsigned long long mate) {
    // preCreaseによって、置いて良いタイルが制限されている場合
    if (tileConditon[cell][tile] == 0)
        return false;

    int x = cell_x[cell];
    int y = cell_y[cell];

    // mateの4方向のindexを得る
    int li, ui, uli, uri;
    li = get_index(LEFT, x);
    ui = get_index(UP, x);
    uli = get_index(UPPER_LEFT, x);
    uri = get_index(UPPER_RIGHT, x);

    // mateの4方向の辺を得る
    int ml, mul, mu, mur;
    ml = get_binary_digit(mate, li);
    mul = get_binary_digit(mate, uli);
    mu = get_binary_digit(mate, ui);
    mur = get_binary_digit(mate, uri);
    uint32_t m_all = ml | (mul << 1) | (mu << 2) | (mur << 3);

    // 接続判定
    if ((m_all & d_mask[cell]) != (tile_4_edges[tile] & d_mask[cell]))
        return false;

    return true;
}

unsigned long long Counter::put(int cell, int take, unsigned long long mate) {
    int x = cell % w;
    int y = cell / w;

    // 更新すべき方向
    int r = 1, ll = 1, d = 1, lr = 1;
    if (x == 0)
        ll = 0;
    if (x == w - 1)
        r = 0, lr = 0;

    // 更新すべき方向のindex
    int ri, lli, di, lri;
    ri = get_index(LEFT, x + 1);
    lli = get_index(UPPER_RIGHT, x - 1);
    di = get_index(UP, x);
    lri = mate_size - 1;

    int ul = get_index(UPPER_LEFT, x);

    // mateの更新
    unsigned long long mate2 = mate;
    int lr2 = get_binary_digit(mate2, mate_size - 1);
    mate2 = set_bit(mate, ul, lr2);
    int ds[4] = {RIGHT, LOWER_LEFT, DOWN, LOWER_RIGHT};
    int updates[4] = {r, ll, d, lr};
    int ids[4] = {ri, lli, di, lri};
    for (int i = 0; i < 4; i++) {
        if (updates[i] == 1)
            mate2 = set_bit(mate2, ids[i], TILE[take][ds[i]]);
    }

    return mate2;
}

unsigned long long Counter::count() {
    // tableの準備
    vector<vector<unsigned long long>> table(
        2, vector<unsigned long long>(state_size, 0));

    table[0][0] = 1;

    // mate配列の鍵を作る（OpenMP使用時のみ）
#ifdef _OPENMP
    vector<omp_lock_t> locks(state_size);
    for (unsigned long long i = 0; i < state_size; i++) {
        omp_init_lock(&locks[i]);
    }
#endif

    for (int i = 0; i < w * w; i++) {
        int prev = i % 2;
        int next = (i + 1) % 2;

        // next配列の初期化
        for (unsigned long long k = 0; k < state_size; k++)
            table[next][k] = 0;

// 各mateの更新
#pragma omp parallel for schedule(guided)
        for (unsigned long long k = 0; k < state_size; k++) {

            if (table[prev][k] == 0)
                continue;

            for (int j = 0; j < 36; j++) { // タイルjを設置

                // セルiにタイルjが置けるか？
                if (!can_put(i, j, k))
                    continue;

                // セルiにタイルjを設置したときのmateを得る
                unsigned long long newstate = put(i, j, k);

                // 書き込み先のロックを取得
#ifdef _OPENMP
                omp_set_lock(&locks[newstate]);
#endif

                // 得られたmateへ到達する経路数を加算する

                table[next][newstate] += table[prev][k];

                // ロックを開放
#ifdef _OPENMP
                omp_unset_lock(&locks[newstate]);
#endif
            }
        }
    }

    // ロックの解放
#ifdef _OPENMP
    for (unsigned long long i = 0; i < state_size; i++) {
        omp_destroy_lock(&locks[i]);
    }
#endif

    // evenまたはoddのmate数の和を計算する
    unsigned long long sum = 0;
    int next = w % 2;

    for (unsigned long long state = 0; state < state_size; state++) {
        sum += table[next][state];
    }

    return sum;
}

// 外周部の割当条件を満たす平坦折り可能な展開図が存在するか判定
bool Counter::hasCP() {
    // tableの準備
    bool *table = (bool *)malloc(sizeof(bool) * state_size * 2);

#pragma omp parallel for
    for (unsigned long long i = 0; i < state_size * 2; i++) {
        table[i] = false;
    }

    table[0] = true;

    for (int i = 0; i < w * w; i++) {
        int prev = i % 2;
        int next = (i + 1) % 2;

        // next配列の初期化
        for (unsigned long long k = 0; k < state_size; k++)
            table[next * state_size + k] = false;

// 各mateの更新
#pragma omp parallel for schedule(guided)
        for (unsigned long long k = 0; k < state_size; k++) {

            if (table[prev * state_size + k] == false)
                continue;

            for (int j = 0; j < 36; j++) { // タイルjを設置

                // セルiにタイルjが置けるか？
                if (!can_put(i, j, k))
                    continue;

                // セルiにタイルjを設置したときのmateを得る
                unsigned long long newstate = put(i, j, k);

                // 得られたmateへ到達する経路数を加算する
                table[next * state_size + newstate] = true;
            }
        }
    }

    // 展開図が存在するか
    int next = w % 2;
    unsigned long long row_offset = next * state_size;
    bool has = false;

#pragma omp parallel for reduction(|| : has)
    for (unsigned long long state = 0; state < state_size; state++) {
        has = has || table[row_offset + state];
    }

    free(table);

    return has;
}

string Counter::findCP() {

    // tableの準備
    vector<vector<string>> table(2, vector<string>(state_size, "NG"));

    table[0][0] = "";

    // mate配列の鍵を作る（OpenMP使用時のみ）
#ifdef _OPENMP
    vector<omp_lock_t> locks(state_size);
    for (unsigned long long i = 0; i < state_size; i++) {
        omp_init_lock(&locks[i]);
    }
#endif

    for (int i = 0; i < w * w; i++) {
        int prev = i % 2;
        int next = (i + 1) % 2;

        // next配列の初期化
        for (unsigned long long k = 0; k < state_size; k++)
            table[next][k] = "NG";

// 各mateの更新
#pragma omp parallel for schedule(guided)
        for (unsigned long long k = 0; k < state_size; k++) {
            if (table[prev][k] == "NG")
                continue;

            for (int j = 0; j < 36; j++) { // タイルjを設置

                // セルiにタイルjが置けるか？
                if (!can_put(i, j, k))
                    continue;

                // セルiにタイルjを設置したときのmateを得る
                unsigned long long newstate = put(i, j, k);

                // すでに経路があるなら書き込まない
                if (table[next][newstate] != "NG")
                    continue;

                string tilestr;
                string head = "";
                if (j < 10)
                    head = "0";
                tilestr = head + to_string(j);

                // 書き込み先のロックを取得
#ifdef _OPENMP
                omp_set_lock(&locks[newstate]);
#endif

                // 得られたmateへ到達する展開図を書き込む
                table[next][newstate] = table[prev][k] + tilestr;

                // ロックを開放
#ifdef _OPENMP
                omp_unset_lock(&locks[newstate]);
#endif
            }
        }
    }

    // ロックの解放
#ifdef _OPENMP
    for (unsigned long long i = 0; i < state_size; i++) {
        omp_destroy_lock(&locks[i]);
    }
#endif

    // evenまたはoddのmate数の和を計算する
    unsigned long long sum = 0;
    int next = w % 2;

    for (unsigned long long state = 0; state < state_size; state++) {
        if (table[next][state] != "NG")
            return table[next][state];
    }
    return "No CP";
}

string Counter::to_str(int a) {
    string ans;
    if (a < 10) {
        ans = "0" + to_string(a);
    } else {
        ans = to_str(a);
    }
    return ans;
}

std::string GetExeDirectory() {
#ifdef _WIN32
    char path[MAX_PATH];
    // GetModuleFileNameA:
    // 実行中のモジュール（nullptrは自身を指す）のフルパスを取得 path:
    // パスを格納するバッファ MAX_PATH: バッファの最大サイズ
    DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);

    if (length == 0 || length == MAX_PATH) {
        // エラーまたはバッファオーバーフロー
        return "";
    }

    std::string fullPath(path);

    // フルパスからファイル名部分を除去し、ディレクトリ部分のみを残す
    // Windowsのパス区切り文字 '\' を探す
    size_t last_slash = fullPath.find_last_of('\\');

    if (last_slash != std::string::npos) {
        // 最後の '\' までをディレクトリとして切り出す
        std::string dir = fullPath.substr(0, last_slash + 1);

        // オプション: パス区切り文字を '/' に統一する
        // (Pythonに渡すときなど便利)
        std::replace(dir.begin(), dir.end(), '\\', '/');

        return dir;
    }

    return ""; // 見つからなかった場合
#else
    return "";
#endif
}

void writeToFile(string output_path, string output_txt) {
    // 1. std::ofstream オブジェクトを作成し、ファイル名を与える
    ofstream outfile(output_path);

    // 2. ファイルが正常に開かれたか確認
    if (outfile.is_open()) {
        // 3. cout と同じように << 演算子でデータを出力
        outfile << output_txt;

        // 4. ファイルを閉じる (非常に重要)
        outfile.close();
        cout << "wrote file." << endl;
    } else {
        cerr << "error: cannot open the file." << endl;
        if (outfile.bad())
            std::cerr << "   - badbit: 致命的なエラー (読み書き不能) "
                         "が発生しています。"
                      << std::endl;
        if (outfile.fail())
            std::cerr << "   - failbit: "
                         "書式設定エラーまたは論理エラーが発生しています。"
                      << std::endl;
        if (outfile.eof())
            std::cerr << "   - eofbit: ファイルの終端に達しています "
                         "(書き込みには関係薄)。"
                      << std::endl;
    }
}

void Counter::setTileCondition(vector<vector<int>> &preEdges) {
    // 各マスに置く可能性のあるタイルを設定
    for (int i = 0; i < w * w; i++) {
        for (int t = 0; t < 36; t++) {
            bool puttable = true;
            for (int d = 0; d < 8; d++) {
                if (preEdges[i][d] == -1)
                    continue;
                if (preEdges[i][d] != TILE[t][d]) {
                    puttable = false;
                    break;
                }
            }
            setTileCondition(i, t, puttable);
        }
    }
}

// 各内部頂点の状態から展開図を生成する
// 入力：0 0 -1 -1 1 0 0 ...
// 出力：
string Counter::edges_to_cpstr(vector<int> &innerVerticesState) {

    // PreCreaseの設定
    // 各頂点の各方向に対し
    // -1 : 未定
    // 0 : 折らない
    // 1 : 折る
    // を設定する
    vector<vector<int>> preEdges(w * w, vector<int>(8, -1));
    for (int i = 0; i < w * w; i++) {
        for (int d = 0; d < 8; d++) {
            preEdges[i][d] = innerVerticesState[i * 8 + d];
        }
    }

    // 各マスに置く可能性のあるタイルを設定
    setTileCondition(preEdges);

    bool has = hasCP();
    if (!has)
        return "No CP";
    return findCP();
}

void print_edges_state(vector<int> edges) {
    for (int i = 0; i < 49; i++) {
        for (int j = 0; j < 8; j++) {
            cout << edges[i * 8 + j] << " ";
        }
        cout << endl;
    }
}

string folds_to_cpstr(array<int, 32> &folds) {
    vector<int> edges = create_edges_by_folds(folds);
    Counter c(7);
    return c.edges_to_cpstr(edges);
}

#if 0
int main(void) {
    array<int, 32> folds = {8, 4, 1, 1, 0, 0, 4, 0, //
                            8, 1, 1, 4, 1, 0, 0, 3, //
                            8, 4, 4, 0, 0, 0, 0, 6, //
                            8, 0, 4, 1, 3, 4, 4, 3};

    string ans = folds_to_cpstr(folds);
    cout << ans << endl;
    return 0;
}
#endif
