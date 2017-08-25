/*! \file myrand.h
	\brief 自作乱数クラスの宣言

	Copyright © 2017 @dc1394 All Rights Reserved.
	This software is released under the BSD 2-Clause License.
*/

#ifndef _MYRANDAVX512_H_
#define _MYRANDAVX512_H_

#pragma once

#include <array>						// for std::array
#include <cstdint>                      // for std::int32_t, std::uint_least32_t
#include <functional>                   // for std::ref
#include <random>						// for std::random_device
#include <vector>                       // for std::vector
#include <immintrin.h>					// for _mm512_store_si512
#include <svrng.h>						// for svrng_new_uniform_distribution_int, svrng_new_mt19937_engine, svrng_generate16_int
#include <boost/range/algorithm.hpp>    // for boost::generate

namespace myrandom {
	//! A class.
	/*!
		自作乱数クラス
	*/
	class MyRandAvx512 final {
		// #region コンストラクタ・デストラクタ

	public:
		//! A constructor.
		/*!
			唯一のコンストラクタ
			\param min 乱数分布の最小値
			\param max 乱数分布の最大値
		*/
		MyRandAvx512(std::int32_t min, std::int32_t max);

		//! A destructor.
		/*!
			デフォルトデストラクタ
		*/
		~MyRandAvx512() = default;

		// #endregion コンストラクタ・デストラクタ

		// #region メンバ関数

		//!  A public member function.
		/*!
			[min, max]の閉区間で一様乱数を生成する
		*/
		std::int32_t myrand()
		{
			if (!cnt_) {
				_mm512_store_si512(rnd_.data(), svrng_generate16_int(randengine_, distribution_));
				return rnd_[cnt_++];
			}
			else if (cnt_ == AVXREGBYTE) {
				cnt_ = 0;
				_mm512_store_si512(rnd_.data(), svrng_generate16_int(randengine_, distribution_));
				return rnd_[cnt_++];
			}
			else {
				return rnd_[cnt_++];
			}
		}

		// #endregion メンバ関数

		// #region メンバ変数

	private:
		//! A private member variable.
		/*!
			AVX-512レジスタのバイト数
		*/
		static auto constexpr AVXREGBYTE = 16;

		//! A private member variable.
		/*!
			乱数のカウント
		*/
		std::int32_t cnt_ = 0;

		//! A private member variable.
		/*!
			乱数の分布
		*/
		svrng_distribution_t distribution_;
		
		//! A private member variable.
		/*!
			乱数エンジン
		*/
		svrng_engine_t randengine_;

		//! A private member variable.
		/*!
			乱数が格納されたstd::array
		*/
		alignas(AVXREGBYTE) std::array<std::int32_t, AVXREGBYTE> rnd_;
		
		// #region 禁止されたコンストラクタ・メンバ関数

		//! A private constructor (deleted).
		/*!
			デフォルトコンストラクタ（禁止）
		*/
		MyRandAvx512() = delete;

		//! A private copy constructor (deleted).
		/*!
			コピーコンストラクタ（禁止）
		*/
		MyRandAvx512(MyRandAvx512 const &) = delete;

		//! A private member function (deleted).
		/*!
			operator=()の宣言（禁止）
			\param コピー元のオブジェクト（未使用）
			\return コピー元のオブジェクト
		*/
		MyRandAvx512 & operator=(MyRandAvx512 const &) = delete;

		// #endregion 禁止されたコンストラクタ・メンバ関数
	};

	MyRandAvx512::MyRandAvx512(std::int32_t min, std::int32_t max) :
		distribution_(svrng_new_uniform_distribution_int(min, max + 1))
	{
		// ランダムデバイス
		std::random_device rnd;

		// 初期化用ベクタ
		std::vector<std::uint_least32_t> v(1);

		// ベクタの初期化
		// 非決定的な乱数でシード列を構築する
		boost::generate(v, std::ref(rnd));
		
        //srand(static_cast<std::uint32_t>(time(nullptr)));
        
		// 乱数エンジン
		randengine_ = svrng_new_mt19937_engine(v[0]);
	}
}

#endif  // _MYRANDAVX512_H_
