﻿/*! \file mabinogi_roulette_mc.cpp
    \brief マビノギのルーレットビンゴをモンテカルロ・シミュレーションする

    Copyright © 2015-2017 @dc1394 All Rights Reserved.
    This software is released under the BSD 2-Clause License.
*/

#include "../checkpoint/checkpoint.h"
#include "goexit/goexit.h"
#ifdef HAVE_SSE2
	#include "myrandom/myrandsfmt.h"
#else
	#include "myrandom/myrand.h"
#endif
#include <algorithm>                            // for std::shuffle
#include <cstdint>                              // for std::int32_t
#include <cmath>                                // for std::sqrt
#ifdef _MSC_VER
	#include <format>                           // for std::format
#endif
#include <fstream>                              // for std::ofstream
#include <iostream>                             // for std::cout
#include <iterator>                             // for std::begin, std::ostream_iterator
#include <map>                                  // for std::map
#include <random>                               // for std::mt19937
#include <unordered_map>                        // for std::unordered_map
#include <utility>                              // for std::make_pair, std::move
#include <vector>                               // for std::vector
#include <valarray>                             // for std::valarray
#include <boost/algorithm/cxx11/iota.hpp>       // for boost::algorithm::iota
#ifndef _MSC_VER
	#include <boost/format.hpp>                 // for boost::format
#endif
#include <boost/range/algorithm.hpp>            // for boost::find, boost::max_element, boost::transform
#include <tbb/concurrent_vector.h>              // for tbb::concurrent_vector
#include <tbb/parallel_for.h>                   // for tbb::parallel_for

namespace {
    //! A global variable (constant expression).
    /*!
        列のサイズ
    */
    static auto constexpr COLUMN = 5ULL;

    //! A global variable (constant expression).
    /*!
        行のサイズ
    */
    static auto constexpr ROW = 5ULL;

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
        数字と数字のstd::pair
    */
    using mypair2 = std::pair<std::int32_t, std::int32_t>;

    //! A typedef.
    /*!
        (n + 1)個目の行・列が埋まったときの分布を格納するためのmapの型
    */
    using mymap = std::map<std::int32_t, std::int32_t>;
    
	//! A function.
	/*!
		(n + 1)個目の行・列またはマスが埋まったときの平均試行回数、埋まっているマスまたは行・列の平均個数を求める
		\param mcresult モンテカルロ・シミュレーションの結果が格納された二次元可変長配列
		\param size 行・列またはマスの総数
		\return (n + 1)個目の行・列が埋まったときの平均試行回数、埋まっているマスの平均個数が格納された可変長配列のstd::pair
	*/
	std::pair< std::valarray<double>, std::valarray<double> > eval_average(tbb::concurrent_vector< std::vector<mypair2> > const & mcresult, std::size_t size);

	//! A function.
	/*!
		(n + 1)個目の行・列が埋まったときの中央値を求める
		\param (n + 1)個目の数値n
		\param mcresult モンテカルロ・シミュレーションの結果が格納された二次元可変長配列
		\return (n + 1)個目の行・列が埋まったときの中央値
	*/
	std::int32_t eval_median(tbb::concurrent_vector< std::vector<mypair2> > const & mcresult, std::int32_t n);

	//! A function.
	/*!
		(n + 1)個目の行・列が埋まったときの最頻値と分布を求める
		\param (n + 1)個目の数値n
		\param mcresult モンテカルロ・シミュレーションの結果が格納された二次元可変長配列
		\return (n + 1)個目の行・列が埋まったときの最頻値と分布のstd::pair
	*/
	std::pair<std::int32_t, std::map<std::int32_t, std::int32_t> > eval_mode(tbb::concurrent_vector< std::vector<mypair2> > const & mcresult, std::int32_t n);

	//! A function.
	/*!
		(n + 1)個目の行・列が埋まったときの標準偏差を求める
		\param avgten (n + 1)個目の行・列が埋まったときの平均試行回数
		\param (n + 1)個目の数値n
		\param mcresult モンテカルロ・シミュレーションの結果が格納された二次元可変長配列
		\return (n + 1)個目の行・列が埋まったときの標準偏差
	*/
	double eval_std_deviation(double avg, tbb::concurrent_vector< std::vector<mypair2> > const & mcresult, std::int32_t n);

    //! A function.
    /*!
        ビンゴボードを生成する
        \return ビンゴボードが格納された可変長配列
    */
    auto makeboard();

#ifdef _CHECK_PARALELL_PERFORM
    //! A function.
    /*!
        モンテカルロ・シミュレーションを行う
        \return モンテカルロ・シミュレーションの結果が格納された二次元可変長配列
    */
	std::pair<std::vector< std::vector<mypair2> >, std::vector< std::vector<mypair2> > > montecarlo();
#endif

    //! A function.
    /*!
        モンテカルロ・シミュレーションの実装
        \param mr 自作乱数クラスのオブジェクト
        \return モンテカルロ法の結果が格納された可変長配列
    */
	template <typename MyRandom>
	std::pair<std::vector<mypair2>, std::vector<mypair2> > montecarloImpl(MyRandom & mr);

    //! A function.
    /*!
        モンテカルロ・シミュレーションをTBBで並列化して行う
        \return モンテカルロ・シミュレーションの結果が格納された二次元可変長配列
    */
	std::pair<tbb::concurrent_vector< std::vector<mypair2> >, tbb::concurrent_vector< std::vector<mypair2> > > montecarloTBB();

    //! A function.
    /*!
        (n + 1)個目の行・列が埋まったときの分布をcsvファイルに出力する
		\param distmap (n + 1)個目の行・列が埋まったときの分布
		\param filename ファイル名
    */
    void outputcsv(mymap const & distmap, std::string const & filename);
}

int main()
{
    checkpoint::CheckPoint cp;

    cp.checkpoint("処理開始", __LINE__);

#ifdef _CHECK_PARALELL_PERFORM
    // モンテカルロ・シミュレーションの結果を代入
    auto const mcresult(montecarlo());

    cp.checkpoint("並列化無効", __LINE__);
#endif      
	
    // TBBで並列化したモンテカルロ・シミュレーションの結果を代入
    auto const mcresult2(montecarloTBB());

    cp.checkpoint("並列化有効", __LINE__);

    auto const [trialavg, fillavg] = eval_average(mcresult2.first, ROWCOLUMN);

    for (auto n = 0U; n < ROWCOLUMN; n++) {
		auto const [mode, distmap] = eval_mode(mcresult2.first, n);
#ifdef _MSC_VER
		outputcsv(distmap, std::format("result/distribution_{:d}個目.csv", n + 1));

        std::cout 
			<< std::format("ビンゴ{:d}個目に必要な平均試行回数：{:.1f}回, 効率：{:.1f}(回/個), ", n + 1, trialavg[n], trialavg[n] / static_cast<double>(n + 1))
			<< std::format("中央値：{:d}回, 最頻値：{:d}回, 標準偏差：{:.1f}, ", eval_median(mcresult2.first, n), mode, eval_std_deviation(trialavg[n], mcresult2.first, n))
			<< std::format("埋まっているマスの平均個数：{:.1f}個\n", fillavg[n]);
#else
        outputcsv(distmap, (boost::format("result/distribution_%d個目.csv") % (n + 1)).str());

        std::cout
            << boost::format("ビンゴ%d個目に必要な平均試行回数：%.1f回, 効率：%.1f(回/個), ")
            % (n + 1)
            % trialavg[n]
            % (trialavg[n] / static_cast<double>(n + 1))
            << boost::format("中央値：%d回, 最頻値：%d回, 標準偏差：%.1f, ")
            % eval_median(mcresult2.first, n)
            % mode
            % eval_std_deviation(trialavg[n], mcresult2.first, n)
            << boost::format("埋まっているマスの平均個数：%.1f個\n")
            % fillavg[n];
#endif
    }

	auto const [trialavg2, fillavg2] = eval_average(mcresult2.second, BOARDSIZE);

	for (auto n = 0U; n < BOARDSIZE; n++) {
		auto const [mode, distmap] = eval_mode(mcresult2.second, n);
#ifdef _MSC_VER
        outputcsv(distmap, std::format("result/distribution2_{:d}個目.csv", n + 1));

        std::cout
            << std::format("{:d}個目のマスに必要な平均試行回数：{:.1f}回, 効率：{:.1f}(回/個), ", n + 1, trialavg2[n], trialavg2[n] / static_cast<double>(n + 1))
            << std::format("中央値：{:d}回, 最頻値：{:d}回, 標準偏差：{:.1f}, ", eval_median(mcresult2.second, n), mode, eval_std_deviation(trialavg2[n], mcresult2.second, n))
            << std::format("埋まっている行・列の平均個数：{:.1f}個\n", fillavg2[n]);
#else
		outputcsv(distmap, (boost::format("result/distribution2_%d個目.csv") % (n + 1)).str());

		std::cout
			<< boost::format("%d個目のマスに必要な平均試行回数：%.1f回, 効率：%.1f(回/個), ")
			% (n + 1)
			% trialavg2[n]
			% (trialavg2[n] / static_cast<double>(n + 1))
			<< boost::format("中央値：%d回, 最頻値：%d回, 標準偏差：%.1f, ")
			% eval_median(mcresult2.second, n)
			% mode
			% eval_std_deviation(trialavg2[n], mcresult2.second, n)
			<< boost::format("埋まっている行・列の平均個数：%.1f個\n")
			% fillavg2[n];
#endif
	}

    cp.checkpoint("それ以外の処理", __LINE__);

    cp.checkpoint_print();

	goexit::goexit();

    return 0;
}

namespace {
    std::pair< std::valarray<double>, std::valarray<double> > eval_average(tbb::concurrent_vector< std::vector<mypair2> > const & mcresult, std::size_t size)
    {
        // モンテカルロ・シミュレーションの平均試行回数の結果を格納した可変長配列
        std::valarray<double> trialavg(size);

        // モンテカルロ・シミュレーションのn回目の試行で、埋まっているマスの数を格納した可変長配列
        std::valarray<double> fillavg(size);

        // 行・列の総数分繰り返す
        for (auto n = 0U; n < size; n++) {
            // 総和を0で初期化
            auto trialsum = 0;
            auto fillsum = 0;

            // 試行回数分繰り返す
            for (auto j = 0U; j < MCMAX; j++) {
                // j回目の結果を加える
                trialsum += mcresult[j][n].first;
                fillsum += mcresult[j][n].second;
            }

            // 平均を算出してn行・列目のtrialavg、fillavgに代入
            trialavg[n] = static_cast<double>(trialsum) / static_cast<double>(MCMAX);
            fillavg[n] = static_cast<double>(fillsum) / static_cast<double>(MCMAX);
        }

        return std::make_pair(std::move(trialavg), std::move(fillavg));
    }

	std::int32_t eval_median(tbb::concurrent_vector< std::vector<mypair2> > const & mcresult, std::int32_t n)
	{
		// 中央値を求めるために必要な可変長配列
		std::vector<std::int32_t> medtmp(MCMAX);

		// 中央値を求めるために必要な可変長配列を、モンテカルロ法の結果から生成
		boost::transform(
			mcresult,
			medtmp.begin(),
			[n](auto const & res) { return res[n].first; });

		// 中央値を求めるためにソートする
		boost::sort(medtmp);

		// 中央値を求める
		if constexpr (MCMAX % 2) {
			// 要素が奇数個なら中央の要素を返す
			return medtmp[(MCMAX - 1) / 2];
		}
		else {
			// 要素が偶数個なら中央二つの平均を返す
			return (medtmp[(MCMAX / 2) - 1] + medtmp[MCMAX / 2]) / 2;
		}
	}

	std::pair<std::int32_t, mymap> eval_mode(tbb::concurrent_vector< std::vector<mypair2> > const & mcresult, std::int32_t n)
	{
		// (n + 1)個目の行・列が埋まったときの分布
		std::unordered_map<std::int32_t, std::int32_t> distmap;

		// distmapを埋める
		for (auto const & res : mcresult) {
			// (n + 1)個目の行・列が埋まったときの回数をkeyとする
			auto const key = res[n].first;

			// keyが存在するかどうか
			auto itr = distmap.find(key);
			if (itr == distmap.end()) {
				// keyが存在しなかったので、そのキーでハッシュを拡張（値1）
				distmap.emplace(key, 1);
			}
			else {
				// keyが指す値を更新
				itr->second++;
			}
		}

		// 最頻値を探索
		auto const mode = boost::max_element(
			distmap,
			[](auto const & p1, auto const & p2) { return p1.second < p2.second; })->first;

		// 最頻値と(n + 1)個目の行・列が埋まったときの分布をpairにして返す
		return std::make_pair(mode, mymap(distmap.begin(), distmap.end()));
	}

	double eval_std_deviation(double avg, tbb::concurrent_vector< std::vector<mypair2> > const & mcresult, std::int32_t n)
	{
		// 標準偏差を求めるために必要な可変長配列
		std::valarray<double> devtmp(MCMAX);

		// 標準偏差の計算
		boost::transform(
			mcresult,
			std::begin(devtmp),
			[avg, n](auto const & res) {
			auto const val = static_cast<double>(res[n].first);
			return (val - avg) * (val - avg);
		});

		// 標準偏差を求める
		return std::sqrt(devtmp.sum() / static_cast<double>(MCMAX));
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

#ifdef _CHECK_PARALELL_PERFORM
	std::pair<std::vector< std::vector<mypair2> >, std::vector< std::vector<mypair2> > > montecarlo()
    {
        // モンテカルロ・シミュレーションの結果を格納するための二次元可変長配列
		std::pair<std::vector< std::vector<mypair2> >, std::vector< std::vector<mypair2> > > mcresult;

		// MCMAX個の容量を確保
		mcresult.first.reserve(MCMAX);
		mcresult.second.reserve(MCMAX);

#ifdef HAVE_SSE2
		// 自作乱数クラスを初期化
		myrandom::MyRandSfmt mr(1, BOARDSIZE);
#else
		// 自作乱数クラスを初期化
		myrandom::MyRand mr(1, BOARDSIZE);
#endif
        // 試行回数分繰り返す
        for (auto n = 0U; n < MCMAX; n++) {
			// モンテカルロ・シミュレーションの結果を代入
			auto const [resf, ress] = montecarloImpl(mr);
			mcresult.first.emplace_back(resf);
			mcresult.second.emplace_back(ress);
        }

        // モンテカルロ・シミュレーションの結果を返す
        return mcresult;
    }
#endif

	template <typename MyRandom>
	std::pair<std::vector<mypair2>, std::vector<mypair2> > montecarloImpl(MyRandom & mr)
    {
        // ビンゴボードを生成
        auto board(makeboard());

        // その行・列が既に埋まっているかどうかを格納する可変長配列
        // ROWCOLUMN個の要素をfalseで初期化
        std::vector<bool> rcfill(ROWCOLUMN, false);

        // 行・列が埋まるまでに要した回数と、その時点で埋まったマスを格納した
        // 可変長配列
        std::vector<mypair2> fillnum;

		// (n + 1)個目のマスが埋まったときの回数と、その時点で埋まった行・列を格納した
        // 可変長配列
		std::vector<mypair2> fillnum2;

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
        for (auto n = 1; ; n++) {
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
                    fillnum.emplace_back(n, sum(board));
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
                    fillnum.emplace_back(n, sum(board));
                }
            }

			// 要した試行回数と、その時点で埋まっている行・列の数を格納
			fillnum2.emplace_back(n, static_cast<std::int32_t>(fillnum.size()));

            // 全ての行・列が埋まったかどうか
            if (fillnum.size() == ROWCOLUMN) {
                // 埋まったのでループ脱出
                break;
            }
        }

        // 要した試行関数の可変長配列を返す
        return std::make_pair(std::move(fillnum), std::move(fillnum2));
    }

    std::pair<tbb::concurrent_vector< std::vector<mypair2> >, tbb::concurrent_vector< std::vector<mypair2> > > montecarloTBB()
    {
        // モンテカルロ・シミュレーションの結果を格納するための二次元可変長配列
        // 複数のスレッドが同時にアクセスする可能性があるためtbb::concurrent_vectorを使う
        std::pair<tbb::concurrent_vector< std::vector<mypair2> >, tbb::concurrent_vector< std::vector<mypair2> > > mcresult;

        // MCMAX個の容量を確保
        mcresult.first.reserve(MCMAX);
		mcresult.second.reserve(MCMAX);

        // MCMAX回のループを並列化して実行
        tbb::parallel_for(
            0U,
            MCMAX,
            1U,
            [&mcresult](auto) {

#ifdef HAVE_SSE2
			// 自作乱数クラスを初期化
			myrandom::MyRandSfmt mr(1, BOARDSIZE);
#else
			// 自作乱数クラスを初期化
			myrandom::MyRand mr(1, BOARDSIZE);
#endif

            // モンテカルロ・シミュレーションの結果を代入
			auto const [resf, ress] = montecarloImpl(mr);
            mcresult.first.emplace_back(resf);
			mcresult.second.emplace_back(ress);
        });

        // モンテカルロ・シミュレーションの結果を返す
        return mcresult;
    }

    void outputcsv(mymap const & distmap, std::string const & filename)
    {
        std::ofstream ofs(filename);

        boost::transform(
            distmap,
            std::ostream_iterator<std::string>(ofs, "\n"),
#ifdef _MSC_VER
            [](auto const& p) { return std::format("{:d},{:d}", p.first, p.second); });
#else
            [](auto const & p) { return (boost::format("%d,%d") % p.first % p.second).str(); });
#endif
    }
}

