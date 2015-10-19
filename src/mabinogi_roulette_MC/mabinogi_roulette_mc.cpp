#include "../checkpoint/checkpoint.h"
#include "myrandom/myrand.h"
#include <algorithm>                            // for std::shuffle
#include <cstdint>                              // for std::int32_t
#include <iostream>                             // for std::cout
#include <random>                               // for std::mt19937
#include <utility>                              // for std::make_pair
#include <vector>                               // for std::vector
#include <boost/algorithm/cxx11/iota.hpp>       // for boost::algorithm::iota
#include <boost/format.hpp>                     // for boost::format
#include <boost/range/algorithm.hpp>            // for boost::find, boost::transform
#include <tbb/concurrent_vector.h>              // for tbb::concurrent_vector
#include <tbb/parallel_for.h>                   // for tbb::parallel_for

namespace {
    // ビンゴボードのマス数
    static auto constexpr BOARDSIZE = 25U;

    // モンテカルロシミュレーションの試行回数
    static auto constexpr MCMAX = 100000;

    // 行・列の総数
    static auto constexpr ROWCOLUMNSIZE = 10U;

    //! A typedef.
    /*!
        そのマスに書かれてある番号と、そのマスが当たったかどうかを示すフラグ
        のstd::pair
    */
    using mypair = std::pair<std::int32_t, bool>;

    //! A typedef.
    /*!
        行・列が埋まるまでに要した回数と、その時点で埋まったマスのstd::pair
    */
    using mypair2 = std::pair<std::int32_t, std::int32_t>;

    //! A function.
    /*!
        ビンゴボードを生成する
        \return ビンゴボードが格納された可変長配列
    */
    std::vector<mypair> makeBoard();

    //! A function.
    /*!
        モンテカルロ・シミュレーションを行う
        \return モンテカルロ法の結果が格納された二次元可変長配列
    */
    std::vector< std::vector<mypair2> > montecarlo();

    //! A function.
    /*!
        モンテカルロ・シミュレーションの実装
        \return モンテカルロ法の結果が格納された可変長配列
    */
    std::vector<mypair2> montecarloImpl();

    //! A function.
    /*!
        モンテカルロ・シミュレーションをTBBで並列化して行う
        \return モンテカルロ法の結果が格納された可変長配列
    */
    tbb::concurrent_vector< std::vector<mypair2> > montecarloTBB();
}

int main()
{
    checkpoint::CheckPoint cp;

    cp.checkpoint("処理開始", __LINE__);
    
    // モンテカルロ・シミュレーションの結果を代入
    auto const mcresult(montecarlo());

    cp.checkpoint("並列化無効", __LINE__);
    
    // TBBで並列化したモンテカルロ・シミュレーションの結果を代入
    auto const mcresult2(montecarloTBB());

    cp.checkpoint("並列化有効", __LINE__);
        
    // モンテカルロ・シミュレーションの平均試行回数の結果を格納した可変長配列
    std::vector<double> trialavg(ROWCOLUMNSIZE);

    // モンテカルロ・シミュレーションのi回目の試行で、埋まっているマスの数を格納した可変長配列
    std::vector<double> fillavg(ROWCOLUMNSIZE);

    // 行・列の総数分繰り返す
    for (auto i = 0; i < ROWCOLUMNSIZE; i++) {
        // 総和を0で初期化
        auto trialsum = 0;
        auto fillsum = 0;

        // 試行回数分繰り返す
        for (auto j = 0; j < MCMAX; j++) {
            // j回目の結果を加える
            trialsum += mcresult[j][i].first;
            fillsum += mcresult[j][i].second;
        }

        // 平均を算出してi行・列目のtrialavg、fillavgに代入
        trialavg[i] = static_cast<double>(trialsum) / static_cast<double>(MCMAX);
        fillavg[i] = static_cast<double>(fillsum) / static_cast<double>(MCMAX);
    }

    for (auto i = 0U; i < ROWCOLUMNSIZE; i++) {
        auto const efficiency = trialavg[i] / static_cast<double>(i + 1);
        std::cout
            << boost::format("%d個目に必要な平均試行回数： %.1f回, 効率 = %.1f（回/個）, ")
            % (i + 1) % trialavg[i] % efficiency
            << boost::format("埋まっているマスの平均個数： %.1f個\n")
            % fillavg[i];
    }

    cp.checkpoint_print();

    return 0;
}

namespace {
    std::vector<mypair> makeBoard()
    {
        // 仮のビンゴボードを生成
        std::vector<std::int32_t> boardtmp(BOARDSIZE);

        // 仮のビンゴボードに1～25の数字を代入
        boost::algorithm::iota(boardtmp, 1);

        // 仮のビンゴボードの数字をシャッフル
        std::shuffle(boardtmp.begin(), boardtmp.end(), std::mt19937());

        // ビンゴボードを生成
        std::vector<mypair> board(BOARDSIZE);

        // 仮のビンゴボードからビンゴボードを生成する
        boost::transform(
            boardtmp,
            board.begin(),
            [](auto n) { return std::make_pair(n, false); });

        // ビンゴボードを返す
        return board;
    }

    std::vector< std::vector<mypair2> > montecarlo()
    {
        // モンテカルロ・シミュレーションの結果を格納するための二次元可変長配列
        std::vector< std::vector<mypair2> > mcresult;

        // MCMAX個の容量を確保
        mcresult.reserve(MCMAX);

        // 試行回数分繰り返す
        for (auto i = 0; i < MCMAX; i++) {
            // モンテカルロ・シミュレーションの結果を代入
            mcresult.push_back(montecarloImpl());
        }

        // モンテカルロ・シミュレーションの結果を返す
        return mcresult;
    }

    std::vector<mypair2> montecarloImpl()
    {
        // ビンゴボードを生成
        auto board(makeBoard());

        // 自作乱数クラスを初期化
        myrandom::MyRand mr(1, BOARDSIZE);

        // その行・列が既に埋まっているかどうかを格納する可変長配列
        // ROWCOLUMNSIZE個の要素をfalseで初期化
        std::vector<bool> rcfill(ROWCOLUMNSIZE, false);

        // 行・列が埋まるまでに要した回数と、その時点で埋まったマスを格納した
        // 可変長配列
        std::vector< mypair2 > fillnum;

        // ROWCOLUMNSIZE個の容量を確保
        fillnum.reserve(ROWCOLUMNSIZE);

        // その時点で埋まっているマスを計算するためのラムダ式
        auto const sum = [](const std::vector< mypair > & vec)
        {
            auto cnt = 0;
            for (auto & e : vec) {
                if (e.second) {
                    cnt++;
                }
            }

            return cnt;
        };

        // 無限ループ
        for (auto i = 1; true; i++) {
            // 乱数で得た数字で、かつまだ当たってないマスを検索
            auto itr = boost::find(board, std::make_pair(mr.myrand(), false));

            // そのようなマスがあった
            if (itr != board.end()) {
                // そのマスは当たったとし、フラグをtrueにする
                itr->second = true;
            }
            // そのようなマスがなかった
            else {
                //ループ続行
                continue;
            }

            // 各行・列が埋まったかどうかをチェック
            for (auto j = 0; j < 5; j++) {
                // 行をチェック
                if (board[5 * j].second &&
                    board[5 * j + 1].second &&
                    board[5 * j + 2].second &&
                    board[5 * j + 3].second &&
                    board[5 * j + 4].second &&
                    // その行は既に埋まっているかどうか
                    !rcfill[j]) {

                    // その行は埋まったとして、フラグをtrueにする
                    rcfill[j] = true;
                    
                    // 要した試行回数と、その時点で埋まったマスの数を格納
                    fillnum.push_back(std::make_pair(i, sum(board)));
                }

                // 列をチェック
                if (board[j].second &&
                    board[j + 5].second &&
                    board[j + 10].second &&
                    board[j + 15].second &&
                    board[j + 20].second &&
                    // その列は既に埋まっているかどうか
                    !rcfill[j + 5]) {

                    // その列は埋まったとして、フラグをtrueにする
                    rcfill[j + 5] = true;
                    
                    // 要した試行回数と、その時点で埋まったマスの数を格納
                    fillnum.push_back(std::make_pair(i, sum(board)));
                }
            }

            // 全ての行・列が埋まったかどうか
            if (fillnum.size() == ROWCOLUMNSIZE) {
                // 埋まったのでループ脱出
                break;
            }
        }

        // 要した試行関数の可変長配列を返す
        return fillnum;
    }

    tbb::concurrent_vector< std::vector<mypair2> > montecarloTBB()
    {
        // モンテカルロ・シミュレーションの結果を格納するための二次元可変長配列
        // 二つのスレッドが同時にアクセスする可能性があるためtbb::concurrent_vectorを使う
        tbb::concurrent_vector< std::vector<mypair2> > mcresult;

        // MCMAX個の容量を確保
        mcresult.reserve(MCMAX);

        // MCMAX回のループを並列化して実行
        tbb::parallel_for(
            0,
            MCMAX,
            1,
            [&mcresult](auto) { mcresult.push_back(montecarloImpl()); });

        // モンテカルロ・シミュレーションの結果を返す
        return mcresult;
    }
}
