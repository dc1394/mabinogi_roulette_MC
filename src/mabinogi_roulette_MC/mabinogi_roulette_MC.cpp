#include "checkpoint.h"
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
    // �r���S�{�[�h�̃}�X��
    static auto constexpr BOARDSIZE = 25U;

    // �����e�J�����V�~�����[�V�����̎��s��
    static auto constexpr MCMAX = 100000;

    // �s�E��̑���
    static auto constexpr ROWCOLUMNSIZE = 10U;

    //! A using.
    /*!
        ���̃}�X�ɏ�����Ă���ԍ��ƁA���̃}�X�������������ǂ����������t���O
        ��std::pair
    */
    using mytype = std::pair<std::int32_t, bool>;

    //! A function.
    /*!
        �r���S�{�[�h�𐶐�����
        \return �r���S�{�[�h���i�[���ꂽ�ϒ��z��
    */
    std::vector<mytype> makeBoard();

    //! A function.
    /*!
        �����e�J�����E�V�~�����[�V�������s��
        \return �����e�J�����@�̌��ʂ��i�[���ꂽ�񎟌��ϒ��z��
    */
    std::vector< std::vector<std::int32_t> > montecarlo();

    //! A function.
    /*!
        �����e�J�����E�V�~�����[�V�����̎���
        \return �����e�J�����@�̌��ʂ��i�[���ꂽ�ϒ��z��
    */
    std::vector<std::int32_t> montecarloImpl();

    //! A function.
    /*!
        �����e�J�����E�V�~�����[�V������TBB�ŕ��񉻂��čs��
        \return �����e�J�����@�̌��ʂ��i�[���ꂽ�ϒ��z��
    */
    tbb::concurrent_vector< std::vector<std::int32_t> > montecarloTBB();
}

int main()
{
    checkpoint::CheckPoint cp;

    cp.checkpoint("�����J�n", __LINE__);
    
    // �����e�J�����E�V�~�����[�V�����̌��ʂ���
    auto const mcresult(montecarlo());

    cp.checkpoint("���񉻖���", __LINE__);
    
    // TBB�ŕ��񉻂��������e�J�����E�V�~�����[�V�����̌��ʂ���
    auto const mcresult2(montecarloTBB());

    cp.checkpoint("���񉻗L��", __LINE__);
        
    // �����e�J�����E�V�~�����[�V�����̕��ό��ʂ̂��߂̉ϒ��z��
    std::vector<double> avg(ROWCOLUMNSIZE);

    // �s��̑������J��Ԃ�
    for (auto i = 0; i < ROWCOLUMNSIZE; i++) {
        // ���a��0�ŏ�����
        auto sum = 0;

        // ���s�񐔕��J��Ԃ�
        for (auto j = 0; j < MCMAX; j++) {
            // j��ڂ̌��ʂ�������
            sum += mcresult[j][i];
        }

        // ���ς��Z�o����i�s��ڂ�avg�ɑ��
        avg[i] = static_cast<double>(sum) / static_cast<double>(MCMAX);
    }

    for (auto i = 0; i < ROWCOLUMNSIZE; i++) {
        auto const efficiency = avg[i] / static_cast<double>(i + 1);
        std::cout <<
            boost::format("%d�ڂɕK�v�ȕ��ω񐔁F %.1f��, ���� = %.1f�i��/�j\n")
            % (i + 1) % avg[i] % efficiency;
    }

    cp.checkpoint_print();

    return 0;
}

namespace {
    std::vector<mytype> makeBoard()
    {
        // ���̃r���S�{�[�h�𐶐�
        std::vector<std::int32_t> boardtmp(BOARDSIZE);

        // ���̃r���S�{�[�h��1�`25�̐�������
        boost::algorithm::iota(boardtmp, 1);

        // ���̃r���S�{�[�h�̐������V���b�t��
        std::shuffle(boardtmp.begin(), boardtmp.end(), std::mt19937());

        // �r���S�{�[�h�𐶐�
        std::vector<mytype> board(BOARDSIZE);

        // ���̃r���S�{�[�h����r���S�{�[�h�𐶐�����
        boost::transform(
            boardtmp,
            board.begin(),
            [](auto n) { return std::make_pair(n, false); });

        // �r���S�{�[�h��Ԃ�
        return board;
    }

    std::vector< std::vector<std::int32_t> > montecarlo()
    {
        // �����e�J�����E�V�~�����[�V�����̌��ʂ��i�[���邽�߂̓񎟌��ϒ��z��
        std::vector< std::vector<std::int32_t> > mcresult;

        // MCMAX�̗e�ʂ��m��
        mcresult.reserve(MCMAX);

        // ���s�񐔕��J��Ԃ�
        for (auto i = 0; i < MCMAX; i++) {
            // �����e�J�����E�V�~�����[�V�����̌��ʂ���
            mcresult.push_back(montecarloImpl());
        }

        // �����e�J�����E�V�~�����[�V�����̌��ʂ�Ԃ�
        return mcresult;
    }

    std::vector<std::int32_t> montecarloImpl()
    {
        // �r���S�{�[�h�𐶐�
        auto board(makeBoard());

        // ���에���N���X��������
        myrandom::MyRand mr(1, BOARDSIZE);

        // ���̍s�E�񂪊��ɖ��܂��Ă��邩�ǂ������i�[����ϒ��z��
        // ROWCOLUMNSIZE�̗v�f��false�ŏ�����
        std::vector<bool> rcfill(ROWCOLUMNSIZE, false);

        // �s�E�񂪖��܂�܂łɗv�����񐔂��i�[�����ϒ��z��
        std::vector<std::int32_t> fillnum;

        // ROWCOLUMNSIZE�̗e�ʂ��m��
        fillnum.reserve(ROWCOLUMNSIZE);

        // �������[�v
        for (auto i = 0; true; i++) {
            // �����œ��������ŁA���܂��������ĂȂ��}�X������
            auto itr = boost::find(board, std::make_pair(mr.myrand(), false));

            // ���̂悤�ȃ}�X��������
            if (itr != board.end()) {
                // ���̃}�X�͓��������Ƃ��A�t���O��true�ɂ���
                itr->second = true;
            }
            // ���̂悤�ȃ}�X���Ȃ�����
            else {
                //���[�v���s
                continue;
            }

            // �e�s�E�񂪖��܂������ǂ������`�F�b�N
            for (auto j = 0; j < 5; j++) {
                // �s���`�F�b�N
                if (board[5 * j].second &&
                    board[5 * j + 1].second &&
                    board[5 * j + 2].second &&
                    board[5 * j + 3].second &&
                    board[5 * j + 4].second &&
                    // ���̍s�͊��ɖ��܂��Ă��邩�ǂ���
                    !rcfill[j]) {
                    // ���̍s�͖��܂����Ƃ��āA�t���O��true�ɂ���
                    rcfill[j] = true;
                    // �v�������s�񐔂��i�[
                    fillnum.push_back(i);
                }

                // ����`�F�b�N
                if (board[j].second &&
                    board[j + 5].second &&
                    board[j + 10].second &&
                    board[j + 15].second &&
                    board[j + 20].second &&
                    // ���̗�͊��ɖ��܂��Ă��邩�ǂ���
                    !rcfill[j + 5]) {
                    // ���̗�͖��܂����Ƃ��āA�t���O��true�ɂ���
                    rcfill[j + 5] = true;
                    // �v�������s�񐔂��i�[
                    fillnum.push_back(i);
                }
            }

            // �S�Ă̍s�E�񂪖��܂������ǂ���
            if (fillnum.size() == ROWCOLUMNSIZE) {
                // ���܂����̂Ń��[�v�E�o
                break;
            }
        }

        // �v�������s�֐��̉ϒ��z���Ԃ�
        return fillnum;
    }

    tbb::concurrent_vector< std::vector<std::int32_t> > montecarloTBB()
    {
        // �����e�J�����E�V�~�����[�V�����̌��ʂ��i�[���邽�߂̓񎟌��ϒ��z��
        // ��̃X���b�h�������ɃA�N�Z�X����\�������邽��tbb::concurrent_vector���g��
        tbb::concurrent_vector< std::vector<std::int32_t> > mcresult;

        // MCMAX�̗e�ʂ��m��
        mcresult.reserve(MCMAX);

        // MCMAX��̃��[�v����񉻂��Ď��s
        tbb::parallel_for(
            0,
            MCMAX,
            1,
            [&mcresult](auto n) { mcresult.push_back(montecarloImpl()); });

        // �����e�J�����E�V�~�����[�V�����̌��ʂ�Ԃ�
        return mcresult;
    }
}