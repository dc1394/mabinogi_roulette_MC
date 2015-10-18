#include "myrandom/myrand.h"
#include <algorithm>                            // for std::shuffle
#include <cstdint>                              // for std::int32_t
#include <iostream>                             // for std::cout
#include <random>                               // for std::mt19937
#include <utility>                              // for std::make_pair
#include <vector>                               // for std::vector
#include <boost/algorithm/cxx11/iota.hpp>       // for boost::iota
#include <boost/format.hpp>                     // for boost::format
#include <boost/range/algorithm/transform.hpp>  // boost::transform
#include <tbb/concurrent_vector.h>              // for tbb::concurrent_vector
#include <tbb/parallel_for.h>                   // for tbb::parallel_for

namespace {
    static auto constexpr BOARDSIZE = 25U;
    static auto constexpr MCMAX = 100000;
    static auto constexpr ROWCOLUMNSIZE = 10U;

    using mytype = std::pair<std::int32_t, bool>;

    std::vector<mytype> makeBoard();
    std::vector< std::vector<std::int32_t> > montecarlo();
    std::vector<std::int32_t> montecarloImpl();
    tbb::concurrent_vector< std::vector<std::int32_t> > montecarloTBB();
}

int main()
{
    auto const mcresult(montecarloTBB());

    std::vector<double> avg(ROWCOLUMNSIZE);
    for (auto i = 0; i < ROWCOLUMNSIZE; i++) {
        auto sum = 0;
        
        for (auto j = 0; j < MCMAX; j++) {
            sum += mcresult[j][i];
        }

        avg[i] = static_cast<double>(sum) / static_cast<double>(MCMAX);
    }

    for (auto i = 0; i < ROWCOLUMNSIZE; i++) {
        std::cout << boost::format("%d個目に必要な平均回数： %.1f回, %.1f（回/個）\n") % (i + 1) % avg[i] % (avg[i] / static_cast<double>(i + 1));
    }

    return 0;
}

namespace {
    std::vector<mytype> makeBoard()
    {
        std::vector<std::int32_t> boardtmp(BOARDSIZE);

        boost::algorithm::iota(boardtmp, 1);
        std::shuffle(boardtmp.begin(), boardtmp.end(), std::mt19937());

        std::vector<mytype> board(BOARDSIZE);
        boost::transform(
            boardtmp,
            board.begin(),
            [](auto n) { return std::make_pair(n, false); });

        return board;
    }

    std::vector< std::vector<std::int32_t> > montecarlo()
    {
        std::vector< std::vector<std::int32_t> > mcresult;
        mcresult.reserve(MCMAX);

        for (auto i = 0; i < MCMAX; i++) {
            mcresult.push_back(montecarloImpl());
        }

        return mcresult;
    }

    std::vector<std::int32_t> montecarloImpl()
    {
        auto board(makeBoard());

        myrandom::MyRand mr(1, BOARDSIZE);
        std::vector<bool> rcsuccess(ROWCOLUMNSIZE, false);
        std::vector<std::int32_t> successnum;
        successnum.reserve(ROWCOLUMNSIZE);

        for (auto i = 0; true; i++) {
            auto itr = boost::find(board, std::make_pair(mr.myrand(), false));
            if (itr != board.end()) {
                itr->second = true;
            }
            else {
                continue;
            }

            for (auto j = 0; j < 5; j++) {
                if (board[5 * j].second &&
                    board[5 * j + 1].second &&
                    board[5 * j + 2].second &&
                    board[5 * j + 3].second &&
                    board[5 * j + 4].second &&
                    !rcsuccess[j]) {
                    rcsuccess[j] = true;
                    successnum.push_back(i);
                }

                if (board[j].second &&
                    board[j + 5].second &&
                    board[j + 10].second &&
                    board[j + 15].second &&
                    board[j + 20].second &&
                    !rcsuccess[j + 5]) {
                    rcsuccess[j + 5] = true;
                    successnum.push_back(i);
                }
            }

            if (successnum.size() == ROWCOLUMNSIZE) {
                break;
            }
        }

        return successnum;
    }

    tbb::concurrent_vector< std::vector<std::int32_t> > montecarloTBB()
    {
        tbb::concurrent_vector< std::vector<std::int32_t> > mcresult;
        mcresult.reserve(MCMAX);

        tbb::parallel_for(
            0,
            MCMAX,
            1,
            [&mcresult](auto n) { mcresult.push_back(montecarloImpl()); });

        return mcresult;
    }
}