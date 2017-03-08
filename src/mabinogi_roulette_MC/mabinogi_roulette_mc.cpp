/*! \file mabinogi_roulette_mc.cpp
    \brief マビノギのルーレットビンゴをモンテカルロ・シミュレーションを行う

    Copyright © 2015-2016 @dc1394 All Rights Reserved.
    This software is released under the BSD 2-Clause License.
*/

#include "../checkpoint/checkpoint.h"
#include "myrandom/myrand.h"
#include <algorithm>                            // for std::shuffle
#include <cstdint>                              // for std::int32_t
#include <cmath>                                // for std::sqrt
#include <fstream>                              // for std::ofstream
#include <iostream>                             // for std::cout
#include <iterator>                             // for std::ostream_iterator
#include <map>                                  // for std::map
#include <random>                               // for std::mt19937
#include <tuple>                                // for std::tie
#include <unordered_map>                        // for std::unordered_map
#include <utility>                              // for std::make_pair, std::move
#include <vector>                               // for std::vector
#include <boost/algorithm/cxx11/iota.hpp>       // for boost::algorithm::iota
#include <boost/format.hpp>                     // for boost::format
#include <boost/range/algorithm.hpp>            // for boost::find, boost::for_each, boost::max_element, boost::transform
#include <boost/range/numeric.hpp>              // for boost::accumulate
#include <cilk/cilk.h>                          // for cilk_for
#include <tbb/concurrent_vector.h>              // for tbb::concurrent_vector

namespace {
    //! A global variable (constant expression).
    /*!
        列のサイズ
    */
    static auto constexpr COLUMN = 5U;

    //! A global variable (constant expression).
    /*!
        行のサイズ
    */
    static auto constexpr ROW = 5U;

    //! A global variable (constant expression).
    /*!
        ビンゴボードのマス数
    */
    static auto constexpr BOARDSIZE = ROW * COLUMN;

    //! A global variable (constant expression).
    /*!
        モンテカルロシミュレーションの試行回数
    */
    static auto constexpr MCMAX = 1000000U;

    //! A global variable (constant expression).
    /*!
        行・列の総数
    */
    static auto constexpr ROWCOLUMN = ROW + COLUMN;

    //! A typedef.
    /*!
        そのマスに書かれてある番号と、そのマスが当たったかどうかを示すフラグのstd::pair
    */
    using mypair = std::pair<std::int32_t, bool>;

    //! A typedef.
    /*!
        行・列が埋まるまでに要した回数と、その時点で埋まったマスのstd::pair
    */
    using mypair2 = std::pair<std::int32_t, std::int32_t>;

    //! A typedef.
    /*!
        10個目の行・列が埋まったときの分布を格納するためのmapの型
    */
    using mymap = std::map<std::int32_t, std::int32_t>;
    
    //! A function.
    /*!
        n個目の行・列が埋まったときの平均試行回数、埋まっているマスの平均個数を求める
        \param mcresult モンテカルロ・シミュレーションの結果が格納された二次元可変長配列
        \return n個目の行・列が埋まったときの平均試行回数、埋まっているマスの平均個数が格納された可変長配列のstd::pair
    */
    std::pair< std::vector<double>, std::vector<double> > eval_average(tbb::concurrent_vector< std::vector<mypair2> > const & mcresult);

    //! A function.
    /*!
        10個目の行・列が埋まったときの中央値を求める
        \param mcresult モンテカルロ・シミュレーションの結果が格納された二次元可変長配列
        \return 10個目の行・列が埋まったときの中央値
    */
    std::int32_t eval_median(tbb::concurrent_vector< std::vector<mypair2> > const & mcresult);

    //! A function.
    /*!
        10個目の行・列が埋まったときの最頻値と分布を求める
        \param mcresult モンテカルロ・シミュレーションの結果が格納された二次元可変長配列
        \return 10個目の行・列が埋まったときの最頻値と分布のstd::pair
    */
    std::pair<std::int32_t, std::map<std::int32_t, std::int32_t> > eval_mode(tbb::concurrent_vector< std::vector<mypair2> > const & mcresult);

    //! A function.
    /*!
        10個目の行・列が埋まったときの標準偏差を求める
        \param avgten 10個目の行・列が埋まったときの平均試行回数
        \param mcresult モンテカルロ・シミュレーションの結果が格納された二次元可変長配列
        \return 10個目の行・列が埋まったときの標準偏差
    */
    double eval_std_deviation(double avgten, tbb::concurrent_vector< std::vector<mypair2> > const & mcresult);

    //! A function.
    /*!
        ビンゴボードを生成する
        \return ビンゴボードが格納された可変長配列
    */
    auto makeboard();

    //! A function.
    /*!
        モンテカルロ・シミュレーションを行う
        \return モンテカルロ・シミュレーションの結果が格納された二次元可変長配列
    */
    std::vector< std::vector<mypair2> > montecarlo();

    //! A function.
    /*!
        モンテカルロ・シミュレーションの実装
        \param mr 自作乱数クラスのオブジェクト
        \return モンテカルロ法の結果が格納された可変長配列
    */
    std::vector<mypair2> montecarloImpl(myrandom::MyRand & mr);

    //! A function.
    /*!
        モンテカルロ・シミュレーションをTBBで並列化して行う
        \return モンテカルロ・シミュレーションの結果が格納された二次元可変長配列
    */
    tbb::concurrent_vector< std::vector<mypair2> > montecarloTBB();

    //! A function.
    /*!
        10個目の行・列が埋まったときの分布をcsvファイルに出力する
        \param distmap 10個目の行・列が埋まったときの分布
    */
    void outputcsv(mymap const & distmap);
}

int main()
{
    checkpoint::CheckPoint cp;

    cp.checkpoint("処理開始", __LINE__);

#ifdef CHECK_PARARELL_PERFORM
    // モンテカルロ・シミュレーションの結果を代入
    auto const mcresult(montecarlo());

    cp.checkpoint("並列化無効", __LINE__);
#endif      

    // TBBで並列化したモンテカルロ・シミュレーションの結果を代入
    auto const mcresult2(montecarloTBB());

    cp.checkpoint("並列化有効", __LINE__);

    std::vector<double> trialavg, fillavg;

    std::tie(trialavg, fillavg) = eval_average(mcresult2);

    for (auto i = 0U; i < ROWCOLUMN; i++) {
        auto const efficiency = trialavg[i] / static_cast<double>(i + 1);
        std::cout
            << boost::format("%d個目に必要な平均試行回数：%.1f回, 効率：%.1f(回/個), ")
            % (i + 1) % trialavg[i] % efficiency
            << boost::format("埋まっているマスの平均個数：%.1f個\n")
            % fillavg[i];
    }

    std::int32_t mode;
    std::map<std::int32_t, std::int32_t> distmap;
    std::tie(mode, distmap) = eval_mode(mcresult2);

    outputcsv(distmap);

    std::cout <<
        boost::format("10個目に必要な中央値：%d回, 最頻値：%d回, 標準偏差：%.1f")
        % eval_median(mcresult2)
        % mode
        % eval_std_deviation(trialavg[ROWCOLUMN - 1], mcresult2) << std::endl;

    cp.checkpoint("それ以外の処理", __LINE__);

    cp.checkpoint_print();

    return 0;
}

namespace {
    std::pair< std::vector<double>, std::vector<double> > eval_average(tbb::concurrent_vector< std::vector<mypair2> > const & mcresult)
    {
        // モンテカルロ・シミュレーションの平均試行回数の結果を格納した可変長配列
        std::vector<double> trialavg(ROWCOLUMN);

        // モンテカルロ・シミュレーションのi回目の試行で、埋まっているマスの数を格納した可変長配列
        std::vector<double> fillavg(ROWCOLUMN);

        // 行・列の総数分繰り返す
        for (auto i = 0U; i < ROWCOLUMN; i++) {
            // 総和を0で初期化
            auto trialsum = 0;
            auto fillsum = 0;

            // 試行回数分繰り返す
            for (auto j = 0U; j < MCMAX; j++) {
                // j回目の結果を加える
                trialsum += mcresult[j][i].first;
                fillsum += mcresult[j][i].second;
            }

            // 平均を算出してi行・列目のtrialavg、fillavgに代入
            trialavg[i] = static_cast<double>(trialsum) / static_cast<double>(MCMAX);
            fillavg[i] = static_cast<double>(fillsum) / static_cast<double>(MCMAX);
        }

        return std::make_pair(std::move(trialavg), std::move(fillavg));
    }

    std::int32_t eval_median(tbb::concurrent_vector< std::vector<mypair2> > const & mcresult)
    {
        // 中央値を求めるために必要な可変長配列
        std::vector<std::int32_t> medtmp(MCMAX);

        // 中央値を求めるために必要な可変長配列を、モンテカルロ法の結果から生成
        boost::transform(
            mcresult,
            medtmp.begin(),
            [](auto const & res) { return res[ROWCOLUMN - 1].first; });

        // 中央値を求めるためにソートする
        boost::sort(medtmp);

        // 中央値を求める
        if (MCMAX % 2) {
            // 要素が奇数個なら中央の要素を返す
            return medtmp[(MCMAX - 1) / 2];
        }
        else {
            // 要素が偶数個なら中央二つの平均を返す
            return (medtmp[(MCMAX / 2) - 1] + medtmp[MCMAX / 2]) / 2;
        }
    }

    std::pair<std::int32_t, mymap> eval_mode(tbb::concurrent_vector< std::vector<mypair2> > const & mcresult)
    {
        // 10個目の行・列が埋まったときの分布
        std::unordered_map<std::int32_t, std::int32_t> distmap;

        // distmapを埋める
        boost::for_each(
            mcresult,
            [&distmap](auto const & res) {
                // 10個目の行・列が埋まったときの回数をkeyとする
                auto const key = res[ROWCOLUMN - 1].first;

                // keyが存在するかどうか
                if (distmap.find(key) == distmap.end()) {
                    // keyが存在しなかったので、そのキーでハッシュを拡張（値1）
                    distmap.emplace(key, 1);
                }
                else {
                    // keyが指す値を更新
                    distmap[key]++;
                }
        });

        // 最頻値を探索
        auto const mode = boost::max_element(
            distmap,
            [](auto const & p1, auto const & p2) { return p1.second < p2.second; })->first;

        // 最頻値と10個目の行・列が埋まったときの分布をpairにして返す
        return std::make_pair(mode, mymap(distmap.begin(), distmap.end()));
    }

    double eval_std_deviation(double avgten, tbb::concurrent_vector< std::vector<mypair2> > const & mcresult)
    {
        // 標準偏差を求めるために必要な可変長配列
        std::vector<double> devtmp(MCMAX);

        // 標準偏差の計算
        boost::transform(
            mcresult,
            devtmp.begin(),
            [avgten](auto const & res) {
                auto const val = static_cast<double>(res[ROWCOLUMN - 1].first);
                return (val - avgten) * (val - avgten);
        });

        // 標準偏差を求める
        return std::sqrt(boost::accumulate(devtmp, 0.0) / static_cast<double>(MCMAX));
    }

    auto makeboard()
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

        // 自作乱数クラスを初期化
        myrandom::MyRand mr(1, BOARDSIZE);

        // 試行回数分繰り返す
        for (auto i = 0U; i < MCMAX; i++) {
            // モンテカルロ・シミュレーションの結果を代入
            mcresult.push_back(montecarloImpl(mr));
        }

        // モンテカルロ・シミュレーションの結果を返す
        return mcresult;
    }

    std::vector<mypair2> montecarloImpl(myrandom::MyRand & mr)
    {
        // ビンゴボードを生成
        auto board(makeboard());

        // その行・列が既に埋まっているかどうかを格納する可変長配列
        // ROWCOLUMN個の要素をfalseで初期化
        std::vector<bool> rcfill(ROWCOLUMN, false);

        // 行・列が埋まるまでに要した回数と、その時点で埋まったマスを格納した
        // 可変長配列
        std::vector<mypair2> fillnum;

        // ROWCOLUMN個の容量を確保
        fillnum.reserve(ROWCOLUMN);

        // その時点で埋まっているマスを計算するためのラムダ式
        auto const sum = [](auto const & vec) {
            auto cnt = 0;
            for (auto & e : vec) {
                if (e.second) {
                    cnt++;
                }
            }

            return cnt;
        };

        // 無限ループ
        for (auto i = 1; ; i++) {
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
            for (auto j = 0U; j < ROW; j++) {
                // 各行が埋まったかどうかのフラグ
                auto rowflag = true;

                // 各行が埋まったかどうかをチェック
                for (auto k = 0U; k < COLUMN; k++) {
                    rowflag &= board[COLUMN * j + k].second;
                }

                // 行の処理
                if (rowflag &&
                    // その行は既に埋まっているかどうか
                    !rcfill[j]) {
                    // その行は埋まったとして、フラグをtrueにする
                    rcfill[j] = true;

                    // 要した試行回数と、その時点で埋まったマスの数を格納
                    fillnum.push_back(std::make_pair(i, sum(board)));
                }

                // 各列が埋まったかどうかのフラグ
                auto columnflag = true;

                // 各列が埋まったかどうかをチェック    
                for (auto k = 0U; k < ROW; k++) {
                    columnflag &= board[j + COLUMN * k].second;
                }

                // 列の処理
                if (columnflag &&
                    // その列は既に埋まっているかどうか
                    !rcfill[j + ROW]) {

                    // その列は埋まったとして、フラグをtrueにする
                    rcfill[j + ROW] = true;

                    // 要した試行回数と、その時点で埋まったマスの数を格納
                    fillnum.push_back(std::make_pair(i, sum(board)));
                }
            }

            // 全ての行・列が埋まったかどうか
            if (fillnum.size() == ROWCOLUMN) {
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
        // 複数のスレッドが同時にアクセスする可能性があるためtbb::concurrent_vectorを使う
        tbb::concurrent_vector< std::vector<mypair2> > mcresult;

        // MCMAX個の容量を確保
        mcresult.reserve(MCMAX);

        // MCMAX回のループを並列化して実行
        cilk_for (auto i = 0U; i < MCMAX; i++) {
            // 自作乱数クラスを初期化
            myrandom::MyRand mr(1, BOARDSIZE);

            // モンテカルロ・シミュレーションの結果を代入
            mcresult.push_back(montecarloImpl(mr));
        }

        // モンテカルロ・シミュレーションの結果を返す
        return mcresult;
    }

    void outputcsv(mymap const & distmap)
    {
        std::ofstream ofs("result.csv");

        boost::transform(
            distmap,
            std::ostream_iterator<std::string>(ofs, "\n"),
            [](auto const & p) { return (boost::format("%d,%d") % p.first % p.second).str(); });
    }
}
