// Copyright (c) 2011, Pacific Biosciences of California, Inc.
//
// All rights reserved.
//
// THIS SOFTWARE CONSTITUTES AND EMBODIES PACIFIC BIOSCIENCES' CONFIDENTIAL
// AND PROPRIETARY INFORMATION.
//
// Disclosure, redistribution and use of this software is subject to the
// terms and conditions of the applicable written agreement(s) between you
// and Pacific Biosciences, where "you" refers to you or your company or
// organization, as applicable.  Any other disclosure, redistribution or
// use is prohibited.
//
// THIS SOFTWARE IS PROVIDED BY PACIFIC BIOSCIENCES AND ITS CONTRIBUTORS "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL PACIFIC BIOSCIENCES OR ITS
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: David Alexander

#pragma once

#include <algorithm>
#include <utility>
#include <string>

#include "Types.hpp"
#include "Quiver/QuiverConfig.hpp"

namespace ConsensusCore {

    /// \brief An exception indicating the Alpha and Beta matrices could
    /// not be matched up by the Recursor
    class AlphaBetaMismatchException : public ExceptionBase
    {
        std::string Message() const throw() { return "Alpha and beta could not be mated."; }
    };


    /// Take the convex hull of two ranges --- returning the smallest range containing
    /// range1 and range2.
    inline std::pair<int, int>
    RangeUnion(const std::pair<int, int>& range1, const std::pair<int, int>& range2)
    {
        return std::make_pair(std::min(range1.first, range2.first),
                              std::max(range1.second, range2.second));
    }

    namespace detail {

    /// \brief A base class for recursors, providing some functionality
    ///        based on polymorphic virtual private methods.
    template <typename M, typename E, typename C>
    class RecursorBase
    {
    public:  // Types
        typedef M MatrixType;
        typedef E EvaluatorType;
        typedef C CombinerType;

    public:
        //
        // API methods
        //

        /// \brief Calculate the recursion score by "linking" partial alpha and/or
        ///        beta matrices.
        virtual float LinkAlphaBeta(const E& e,
                                    const M& alpha, int alphaColumn,
                                    const M& beta, int betaColumn,
                                    int absoluteColumn) const = 0;

        /// \brief Fill the alpha and beta matrices.
        /// This routine will fill the alpha and beta matrices, ensuring
        /// that the score computed from the alpha and beta recursions are
        /// identical, refilling back-and-forth if necessary.
        virtual void
        FillAlphaBeta(const E& e, M& alpha, M& beta) const
            throw(AlphaBetaMismatchException);

        /// \brief Raw FillAlpha, provided primarily for testing purposes.
        ///        Client code should use FillAlphaBeta.
        virtual void FillAlpha(const E& e, const M& guide, M& alpha) const = 0;

        /// \brief Raw FillBeta, provided primarily for testing purposes.
        ///        Client code should use FillAlphaBeta.
        virtual void FillBeta(const E& e, const M& guide, M& beta) const = 0;

        /// \brief Compute two columns of the alpha matrix starting at columnBegin,
        ///        storing the output in ext.
        virtual void ExtendAlpha(const E& e, const M& alphaIn, int columnBegin, M& ext) const = 0;


        /// \brief Read out the alignment from the computed alpha matrix.
        const PairwiseAlignment* Alignment(const E& e, const M& alpha) const;


        RecursorBase(int movesAvailable, const BandingOptions& banding);
        virtual ~RecursorBase();

    protected:
        int movesAvailable_;
        BandingOptions bandingOptions_;
    };
}}

