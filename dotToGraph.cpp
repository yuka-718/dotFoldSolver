// ドット絵からループを出力

#include "BoundaryGraph.h"
#include "foldsToEdges.h"
#include "ftcp.h"
#include "loopToFolds.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <unordered_map>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

using namespace std;

// 文字列の先頭n文字を末尾に移動させる関数
static string rotateString(string s, int n)
{
    if (s.empty())
        return s;
    n %= s.length();
    return s.substr(n) + s.substr(0, n);
}

// vector<int>の先頭32要素をarray<int, 32>に変換する関数
static array<int, 32> vectorToArray(const vector<int> &v)
{
    array<int, 32> arr;
    copy_n(v.begin(), 32, arr.begin());
    return arr;
}

bool is_NG_loopstr(string loopstr)
{
    // カドの配置について
    if (loopstr[0] == 'S')
        return true;
    if (loopstr[8] == 'S')
        return true;
    if (loopstr[16] == 'S')
        return true;
    if (loopstr[24] == 'S')
        return true;

    // 偶頂点から出る斜め線の本数は偶数でなければならない
    // 奇頂点から出る斜め線の本数は偶数でなければならない
    int count[2] = {0, 0};
    for (int i = 0; i < 32; i++)
    {
        if (i % 8 == 0)
            continue;
        char c = loopstr[i];
        if (c != 'S')
            count[i % 2]++;
    }
    if (count[0] % 2 != 0 || count[1] % 2 != 0)
        return true;
    return false;
}

vector<vector<int>> dotstrTo2DVector(string dotstr)
{
    vector<vector<int>> dotArt;

    // 64文字の文字列が入力されるので二次元配列に格納する
    for (int i = 0; i < 8; i++)
    {
        std::vector<int> row;
        for (int j = 0; j < 8; j++)
        {
            row.push_back(0);
        }
        dotArt.push_back(row);
    }

    for (int i = 0; i < 8; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            int n = i * 8 + j;
            int cell = dotstr[n] - '0';
            dotArt[i][j] = cell;
        }
    }
    return dotArt;
}

int distance(int x1, int y1, int x2, int y2)
{
    return (x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2);
}

int OUTER_X[32] = {
    0, 1, 2, 3, 4, 5, 6, 7, //
    8, 8, 8, 8, 8, 8, 8, 8, //
    8, 7, 6, 5, 4, 3, 2, 1, //
    0, 0, 0, 0, 0, 0, 0, 0  //
};

int OUTER_Y[32] = {
    0, 0, 0, 0, 0, 0, 0, 0, //
    0, 1, 2, 3, 4, 5, 6, 7, //
    8, 8, 8, 8, 8, 8, 8, 8, //
    8, 7, 6, 5, 4, 3, 2, 1  //
};

// 距離条件によるスライド可能性の判定
// サーキット上の点の先頭からstep個目が第一コーナーに割り当てられる
bool can_slide(const vector<Point> &circuit, int step)
{
    return true;
    for (int i = 0; i < 32; i++)
    {
        for (int j = i + 1; j < 32; j++)
        {

            // 展開図上でのiとjの距離の計算
            int q1 = (i - step + 32) % 32;
            int q2 = (j - step + 32) % 32;
            int x1 = OUTER_X[q1];
            int y1 = OUTER_Y[q1];
            int x2 = OUTER_X[q2];
            int y2 = OUTER_Y[q2];
            int dist1 = distance(x1, y1, x2, y2);

            // サーキット上でのiとjの距離の計算
            int p1 = i;
            int p2 = j;
            int x3 = circuit[p1].x;
            int y3 = circuit[p1].y;
            int x4 = circuit[p2].x;
            int y4 = circuit[p2].y;
            int dist2 = distance(x3, y3, x4, y4);

            // 展開図上での距離とサーキット上での距離の比較
            if (dist1 < dist2)
            {
                return false;
            }
        }
    }
    return true;
}

// 距離条件判定
// サーキット上の先頭の頂点がカドに割り当てられる
bool distance_condition(const vector<Point> &circuit)
{

    for (int i = 0; i < 32; i++)
    {
        for (int j = i + 1; j < 32; j++)
        {

            // 展開図上でのiとjの距離の計算
            int x1 = OUTER_X[i];
            int y1 = OUTER_Y[i];
            int x2 = OUTER_X[j];
            int y2 = OUTER_Y[j];
            int distOnCP = distance(x1, y1, x2, y2);

            // サーキット上でのiとjの距離の計算
            int x3 = circuit[i].x;
            int y3 = circuit[i].y;
            int x4 = circuit[j].x;
            int y4 = circuit[j].y;
            int distOnCircuit = distance(x3, y3, x4, y4);

            // 展開図上での距離とサーキット上での距離の比較
            if (distOnCP < distOnCircuit)
            {
                // 不適である根拠を表示
                // cout << "i :" << i << " j :" << j << endl;
                // cout << "distOnCP :" << distOnCP
                //  << " distOnCircuit :" << distOnCircuit << endl;
                return false;
            }
        }
    }
    return true;
}

void print_circuit(const vector<Point> &circuit)
{
    cout << "print_circuit" << endl;
    for (int i = 0; i < 32; i++)
    {
        Point p = circuit[i];
        cout << i << " (" << p.x << "," << p.y << ")" << endl;
    }
}

void print_circuit2(const vector<Point> &circuit)
{
    cout << "print_circuit" << endl;
    for (int i = 0; i < 32; i++)
    {
        Point p = circuit[i];
        cout << p.x + p.y * 9 << " ";
    }
    cout << endl;
}

void findCP(string dotstr, int skip)
{

    // CP探索モジュールを宣言
    Counter cpFinder(7);

    // ドット絵を二次元配列に変換
    vector<vector<int>> dotArt = dotstrTo2DVector(dotstr);

    // ドット絵をグラフ化
    BoundaryGraph bg;
    bg.build(dotArt);

    // ドット絵からループを作成
    vector<vector<Point>> cycles = bg.findClockwiseCycles();
    cout << cycles.size() << " Cycles found" << endl;

    // ループから8通りのズラシを生成
    vector<vector<Point>> rotateLoops;
    for (auto c : cycles)
    {
        for (int i = 0; i < 8; i++)
        {
            vector<Point> tmp = c;
            tmp.pop_back(); // なぜかサイズが33なので一度32にしておく
            std::rotate(tmp.begin(), tmp.begin() + i, tmp.end());
            tmp.push_back(tmp[0]);
            rotateLoops.push_back(tmp);
        }
    }
    cout << rotateLoops.size() << " loops generated by slide" << endl;

    // ループのうち、距離条件を満たすものを抽出
    vector<vector<Point>> loops;
    for (auto c : rotateLoops)
    {
        if (distance_condition(c))
        {
            loops.push_back(c);
        }
    }
    cout << loops.size() << " loops generated by distance condition" << endl;

    // ループを方向表示に変換
    vector<vector<string>> loops_dir;
    for (auto c : loops)
    {
        vector<string> dir = bg.getPathDirections(c);
        loops_dir.push_back(dir);
    }

    // ループのうちカド条件と偶奇頂点の斜め線の条件を満たすものを抽出
    vector<string> loops2;
    for (auto c : loops_dir)
    {
        string loop = bg.directionsToString(c);

        if (is_NG_loopstr(loop))
            continue;

        loops2.push_back(loop);
    }
    cout << loops2.size() << " loops generated by corner and edge condition"
         << endl;

    // ループから折り割り当てを生成 <=これが重い？
    vector<vector<int>> folds;
    for (auto c : loops2)
    {
        vector<vector<int>> tmp_folds = createAllFolds(c);
        folds.insert(folds.end(), tmp_folds.begin(), tmp_folds.end());
    }

    // 折り割り当てをarray形式に変換
    vector<array<int, 32>> folds_arr;
    for (auto f : folds)
    {
        array<int, 32> f_arr = vectorToArray(f);
        folds_arr.push_back(f_arr);
    }

    // 平坦折り可能な折り割り当てを探す
    int flatFoldsID = -1;
    for (int i = 0; i < folds_arr.size(); i++)
    {
        vector<int> edges = create_edges_by_folds(folds_arr[i]);
        string cpstr = cpFinder.edges_to_cpstr(edges);
        if (cpstr != "No CP")
        {
            flatFoldsID = i;
            break;
        }
    }

    // 平坦折り可能な折り割り当てが無ければ終了
    if (flatFoldsID == -1)
    {
        cout << "No CP" << endl;
        return;
    }

    // 平坦折り可能な折り割り当てがあればCPを得る
    vector<int> edges = create_edges_by_folds(folds_arr[flatFoldsID]);
    string cpstr = cpFinder.edges_to_cpstr(edges);
    cout << cpstr << endl;

    // CPの4隅を復元
    string cornersstr = "";
    for (int i = 0; i < 4; i++)
    {
        int outer = i * 8 + 1;
        int e = get_edge_from_fold(folds_arr[flatFoldsID][outer], 2);
        cornersstr += " " + to_string(e);
    }
    cout << "CORNERS:" << cornersstr << endl;

    // CPの描画
    cout << "CPSTR:" << cpstr << endl;
    return;
}

void findCP_old(string dotstr, int skip)
{
    auto start_total = std::chrono::high_resolution_clock::now();

    auto start = std::chrono::high_resolution_clock::now();
    vector<vector<int>> dotArt = dotstrTo2DVector(dotstr);

    // ドット絵をグラフ化
    BoundaryGraph bg;
    bg.build(dotArt);
    auto end = std::chrono::high_resolution_clock::now();
    cout << "Build Graph" << endl;

    std::cout << "analysing 8x8 dots..." << std::endl;

    // ドット絵からループを作成
    start = std::chrono::high_resolution_clock::now();
    vector<vector<Point>> cycles = bg.findClockwiseCycles();
    end = std::chrono::high_resolution_clock::now();

    cout << cycles.size() << " Cycles found" << endl;
    cout << "Find Cycles: "
         << std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
                .count()
         << "ms" << endl;

    Counter c(7);

    for (int i = 0; i < cycles.size(); i++)
    {
        // サイクルを方向表示に変換
        vector<string> directions = bg.getPathDirections(cycles[i]);
        string dirstr = bg.directionsToString(directions);
        cout << "cycle " << i << ": " << dirstr << endl;

        // 距離条件を8個のズラシに対して調査
        bool distanceCondition[8];
        for (int j = 0; j < 8; j++)
        {
            vector<Point> tmp = cycles[i];
            tmp.pop_back(); // なぜかサイズが33なので一度32
            std::rotate(tmp.begin(), tmp.begin() + j, tmp.end());
            tmp.push_back(tmp[0]);
            distanceCondition[j] = distance_condition(tmp);
        }

        // サイクル（方向）から折割当を生成
        auto fold_start = std::chrono::high_resolution_clock::now();
        std::vector<vector<int>> folds;
        string rotatestr;

        for (int j = 0; j < 8; j++)
        {

            // 距離条件を満たさないものはスキップ
            if (distanceCondition[j] == false)
                continue;

            rotatestr = rotateString(dirstr, j);
            // この時点でNGな開始点を除去
            if (is_NG_loopstr(rotatestr))
                continue;

            vector<vector<int>> tmp_folds = createAllFolds(rotatestr);
            folds.insert(folds.end(), tmp_folds.begin(), tmp_folds.end());
        }

        auto fold_end = std::chrono::high_resolution_clock::now();
        cout << "  Create Folds: "
             << std::chrono::duration_cast<std::chrono::milliseconds>(
                    fold_end - fold_start)
                    .count()
             << "ms" << endl;
        cout << "  Num Folds: " << folds.size() << endl;

        // foldsの印字
#if 0
        for (auto f : folds)
        {
            for (auto c : f)
            {
                cout << c << " ";
            }
            cout << endl;
        }
#endif

        // 折割り当てが平坦に折れるか検証
        // skip=a のとき、a個おきに探索する
        auto put_tile_start = std::chrono::high_resolution_clock::now();
        long long total_folds_to_cpstr_time = 0;
        for (int k = 0; k < folds.size(); k += skip)
        {
            vector<int> fold = folds[k];
            array<int, 32> foldArr = vectorToArray(fold);

            auto f2c_start = std::chrono::high_resolution_clock::now();

            vector<int> edges = create_edges_by_folds(foldArr);
            string cpstr = c.edges_to_cpstr(edges);

            auto f2c_end = std::chrono::high_resolution_clock::now();
            total_folds_to_cpstr_time +=
                std::chrono::duration_cast<std::chrono::milliseconds>(f2c_end -
                                                                      f2c_start)
                    .count();

            if (cpstr != "No CP")
            {
                cout << "CP found with cycle " << i << endl;

                cout << cpstr << endl;
                // 4隅の復元
                string cornersstr = "";
                for (int i = 0; i < 4; i++)
                {
                    int outer = i * 8 + 1;
                    int e = get_edge_from_fold(foldArr[outer], 2);
                    cornersstr += " " + to_string(e);
                }

                // CPの描画
                cout << "CPSTR:" << cpstr << endl;
                cout << "CORNERS:" << cornersstr << endl;

                auto put_tile_end = std::chrono::high_resolution_clock::now();
                cout << "  PUT TILE: "
                     << std::chrono::duration_cast<std::chrono::milliseconds>(
                            put_tile_end - put_tile_start)
                            .count()
                     << "ms" << endl;
                cout << "  Total folds_to_cpstr: " << total_folds_to_cpstr_time
                     << "ms" << endl;

                auto total_end = std::chrono::high_resolution_clock::now();
                cout << "Total Time: "
                     << std::chrono::duration_cast<std::chrono::milliseconds>(
                            total_end - start_total)
                            .count()
                     << "ms" << endl;
                return;
            }
        }
        auto put_tile_end = std::chrono::high_resolution_clock::now();
        cout << "  Check CP: "
             << std::chrono::duration_cast<std::chrono::milliseconds>(
                    put_tile_end - put_tile_start)
                    .count()
             << "ms" << endl;
        cout << "  Total folds_to_cpstr: " << total_folds_to_cpstr_time << "ms"
             << endl;
    }

    cout << "No CP" << endl;
    auto total_end = std::chrono::high_resolution_clock::now();
    cout << "Total Time: "
         << std::chrono::duration_cast<std::chrono::milliseconds>(total_end -
                                                                  start_total)
                .count()
         << "ms" << endl;
}

string Vector2DToString(const vector<vector<int>> &v)
{
    string exampleStr = "";
    for (auto row : v)
        for (auto cell : row)
            exampleStr += to_string(cell);
    return exampleStr;
}

set<Edge> calcBoundaryEdgeSet(const vector<vector<int>> &dotArt)
{
    set<Edge> B;
    int rows = 8;
    int cols = 8;
    for (int r = 0; r <= rows; ++r)
    {
        for (int c = 0; c <= cols; ++c)
        {
            // 現在の格子点 (c, r) から右方向と下方向の境界をチェック
            // 1. 横方向の境界 (c, r) --- (c+1, r)
            // 上のマスと下のマスの色が異なれば辺を張る
            if (c < cols)
            {
                int up = (r == 0) ? 0 : dotArt[r - 1][c];
                int down = (r == rows) ? 0 : dotArt[r][c];
                if (up != down)
                {
                    B.insert(Edge{Point{c, r}, Point{c + 1, r}});
                }
            }
            // 2. 縦方向の境界 (c, r) --- (c, r+1)
            // 左のマスと右のマスの色が異なれば辺を張る
            if (r < rows)
            {
                int left = (c == 0) ? 0 : dotArt[r][c - 1];
                int right = (c == cols) ? 0 : dotArt[r][c];
                if (left != right)
                {
                    B.insert(Edge{Point{c, r}, Point{c, r + 1}});
                }
            }
        }
    }
    return B;
}

set<Edge> calcConvexHullEdgeSet(const set<Edge> &B)
{
    set<Edge> H;

    // 各辺の端点のx座標とy座標の最大値、最小値をそれぞれ計算する
    int xmin = 8, xmax = 0, ymin = 8, ymax = 0;
    for (const auto &e : B)
    {
        xmin = min(xmin, min(e.p1.x, e.p2.x));
        xmax = max(xmax, max(e.p1.x, e.p2.x));
        ymin = min(ymin, min(e.p1.y, e.p2.y));
        ymax = max(ymax, max(e.p1.y, e.p2.y));
    }

    // 正方形グリッドの辺のうち(xmin,ymin)から(xmax,ymax)までの辺生成する
    for (int x = xmin; x <= xmax; x++)
    {
        for (int y = ymin; y <= ymax; y++)
        {
            if (x < xmax)
                H.insert(Edge{Point{x, y}, Point{x + 1, y}});
            if (y < ymax)
                H.insert(Edge{Point{x, y}, Point{x, y + 1}});
        }
    }
    return H;
}

int calcLoopLength(string dotstr)
{
    vector<vector<int>> dotVector2D = dotstrTo2DVector(dotstr);
    BoundaryGraph bg;
    bg.build(dotVector2D);

    // dotVector2Dから境界辺集合を得る
    set<Edge> Bnd = calcBoundaryEdgeSet(dotVector2D);

    // 境界辺集合から凸包辺集合を得る
    set<Edge> H = calcConvexHullEdgeSet(Bnd);

    // 凸包辺集合Hから未使用辺集合Nを得る（NはHとBの差集合）
    set<Edge> N;
    set_difference(H.begin(), H.end(), Bnd.begin(), Bnd.end(),
                   inserter(N, N.begin()));

    vector<Edge> NV(N.begin(), N.end());
    int n = NV.size();

    int maxDepth = 3;
    for (int k = 0; k <= maxDepth; k++)
    {
        if (k > n)
            break;

        // n個からk個選ぶ組み合わせを next_permutation で生成
        // 0をn-k個、1をk個並べた配列を用意し、辞書順に並べ替える
        vector<int> selector(n);
        fill(selector.begin(), selector.begin() + n - k, 0);
        fill(selector.begin() + n - k, selector.end(), 1);

        do
        {
            vector<Edge> edges;
            for (int i = 0; i < n; ++i)
            {
                if (selector[i])
                {
                    edges.push_back(NV[i]);
                    // NV[i] が選ばれた辺。ここに処理を記述
                }
            }

            for (auto e : edges)
            {
                bg.addEdge(e.p1.x, e.p1.y, e.p2.x, e.p2.y);
            }
            if (bg.isConnected())
            {
                return bg.total_edges + k;
            }
            else
            {
                for (auto e : edges)
                    bg.removeEdge(e);
            }

        } while (next_permutation(selector.begin(), selector.end()));
    }
    return INT_MAX;
}

//
//
#ifdef __EMSCRIPTEN__
static string wasm_output;

extern "C" EMSCRIPTEN_KEEPALIVE int dotfold_loop_length(const char *dotstr)
{
    if (dotstr == nullptr || string(dotstr).size() != 64)
        return -1;
    return calcLoopLength(string(dotstr));
}

extern "C" EMSCRIPTEN_KEEPALIVE const char *dotfold_solve(const char *dotstr,
                                                           int skip)
{
    if (dotstr == nullptr || string(dotstr).size() != 64)
    {
        wasm_output = "ERROR:invalid input";
        return wasm_output.c_str();
    }

    // The browser worker reads the same text protocol as the desktop app.
    // Capturing cout keeps the WebAssembly module quiet and gives JavaScript
    // one deterministic response to parse.
    ostringstream captured;
    streambuf *previous = cout.rdbuf(captured.rdbuf());
    findCP_old(string(dotstr), max(1, skip));
    cout.rdbuf(previous);

    wasm_output = captured.str();
    return wasm_output.c_str();
}
#else
int main(int argc, char *argv[])
{

    // clang-format off
    // 中央で2つのブロックが角で接するドット絵 (十字路が発生する例)
    std::vector<std::vector<int>> dotArtExample = {
        {0,0,0,1,1,0,0,0},
        {0,0,1,1,1,1,0,0},
        {0,1,1,1,1,1,1,0},
        {1,1,1,1,1,1,1,1},
        {1,1,1,1,1,1,1,1},
        {0,1,1,1,1,1,1,0}, 
        {0,0,1,1,1,1,0,0},
        {0,0,0,1,1,0,0,0}
    };
    // clang-format on
    string exampleStr = Vector2DToString(dotArtExample);

    string mode = "-mode=findCP";
    string dotstr = exampleStr;
    int skip = 1;

    if (argc != 1)
    {
        mode = string(argv[1]);
        dotstr = string(argv[2]);
        if (argc >= 4)
        {
            skip = std::stoi(argv[3]);
        }
    }

    // ループ長を計算するモード
    if (mode == "-mode=calcLength")
    {
        int loopLength = calcLoopLength(dotstr);
        cout << "LOOP_LENGTH: " << loopLength << endl;
    }

    // CPを探すモード
    if (mode == "-mode=findCP")
    {
        cout << "--- Search CP ---" << endl;

        int loopLength = calcLoopLength(dotstr);
        cout << "LOOP_LENGTH: " << loopLength << endl;

        findCP_old(dotstr, skip);
        // findCP(dotstr, skip);
    }

    return 0;
}
#endif
