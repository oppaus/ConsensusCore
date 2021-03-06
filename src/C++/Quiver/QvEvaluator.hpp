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


#include <xmmintrin.h>
#include <pmmintrin.h>

#include <algorithm>
#include <cassert>
#include <cfloat>
#include <climits>
#include <cmath>
#include <iostream>
#include <string>
#include <utility>

#include "Quiver/detail/SseMath.hpp"
#include "Quiver/QuiverConfig.hpp"
#include "Quiver/PBFeatures.hpp"
#include "Types.hpp"
#include "Utils.hpp"

#ifndef SWIG
using std::min;
using std::max;
#endif  // SWIG

#define NEG_INF -FLT_MAX

namespace ConsensusCore
{
    //
    // Evaluator classes
    //

    /// \brief An Evaluator that can compute move scores using a QvSequenceFeatures
    class QvEvaluator
    {
    public:
        typedef QvSequenceFeatures FeaturesType;
        typedef QvModelParams      ParamsType;

    public:
        QvEvaluator(const QvSequenceFeatures& features,
                    const std::string& tpl,
                    const QvModelParams& params,
                    bool pinStart = true,
                    bool pinEnd = true)
            : features_(features),
              params_(params),
              tpl_(tpl),
              pinStart_(pinStart),
              pinEnd_(pinEnd)
        {}

        ~QvEvaluator()
        {}

        std::string Read() const
        {
            return features_.Sequence();
        }

        std::string Template() const
        {
            return tpl_;
        }

        void Template(std::string tpl)
        {
            tpl_ = tpl;
        }


        int ReadLength() const
        {
            return features_.Length();
        }

        int TemplateLength() const
        {
            return tpl_.length();
        }

        bool PinEnd() const
        {
            return pinEnd_;
        }

        bool PinStart() const
        {
            return pinStart_;
        }

        bool IsMatch(int i, int j) const
        {
            assert(0 <= i && i < ReadLength());
            assert (0 <= j && j < TemplateLength());
            return (features_[i] == tpl_[j]);
        }

        float Inc(int i, int j) const
        {
            assert(0 <= j && j < TemplateLength() &&
                   0 <= i && i < ReadLength() );
            return (IsMatch(i, j)) ?
                    params_.Match :
                    params_.Mismatch + params_.MismatchS * features_.SubsQv[i];
        }

        float Del(int i, int j) const
        {
            assert(0 <= j && j < TemplateLength() &&
                   0 <= i && i <= ReadLength() );
            if ( (!PinStart() && i == 0) || (!PinEnd() && i == ReadLength()) )
            {
                return 0.0f;
            }
            else
            {
                float tplBase = tpl_[j];
                return (i < ReadLength() && tplBase == features_.DelTag[i]) ?
                        params_.DeletionWithTag + params_.DeletionWithTagS * features_.DelQv[i] :
                        params_.DeletionN;
            }
        }

        float Extra(int i, int j) const
        {
            assert(0 <= j && j <= TemplateLength() &&
                   0 <= i && i < ReadLength() );
            return (j < TemplateLength() && IsMatch(i, j)) ?
                    params_.Branch + params_.BranchS * features_.InsQv[i] :
                    params_.Nce + params_.NceS * features_.InsQv[i];
        }

        float Merge(int i, int j) const
        {
            assert(0 <= j && j < TemplateLength() - 1 &&
                   0 <= i && i < ReadLength() );
            if (!(features_[i] == tpl_[j] && features_[i] == tpl_[j + 1]) )
            {
                return -FLT_MAX;
            }
            else
            {
                return params_.Merge + params_.MergeS * features_.MergeQv[i];
            }
        }

        float Burst(int i, int j, int hpLength) const
        {
            NotYetImplemented();
        }

        //
        // SSE
        //

        __m128 Inc4(int i, int j) const
        {
            assert (0 <= i && i <= ReadLength() - 4);
            assert (0 <= j && j < TemplateLength());
            float tplBase = tpl_[j];
            __m128 match = _mm_set_ps1(params_.Match);
            __m128 mismatch = AFFINE4(params_.Mismatch, params_.MismatchS, &features_.SubsQv[i]);
            // Mask to see it the base is equal to the template
            __m128 mask = _mm_cmpeq_ps(_mm_loadu_ps(&features_.SequenceAsFloat[i]),
                                       _mm_set_ps1(tplBase));
            return MUX4(mask, match, mismatch);
        }

        __m128 Del4(int i, int j) const
        {
            assert (0 <= i && i <= ReadLength());
            assert (0 <= j && j < TemplateLength());
            if (i != 0 && i + 3 != ReadLength())
            {
                float tplBase = tpl_[j];
                __m128 delWTag = AFFINE4(params_.DeletionWithTag,
                                         params_.DeletionWithTagS,
                                         &features_.DelQv[i]);
                __m128 delNoTag = _mm_set_ps1(params_.DeletionN);
                __m128 mask = _mm_cmpeq_ps(_mm_loadu_ps(&features_.DelTag[i]),
                                           _mm_set_ps1(tplBase));
                return MUX4(mask, delWTag, delNoTag);
            }
            else
            {
                // Have to do PinStart/PinEnd logic, and weird
                // logic for last row.  Punt.
                __m128 res = _mm_setr_ps(Del(i + 0, j),
                                         Del(i + 1, j),
                                         Del(i + 2, j),
                                         Del(i + 3, j));
                return res;
            }
        }

        __m128 Extra4(int i, int j) const
        {
            __m128 res = _mm_setr_ps(Extra(i + 0, j),
                                     Extra(i + 1, j),
                                     Extra(i + 2, j),
                                     Extra(i + 3, j));
            return res;
        }

        __m128 Merge4(int i, int j) const
        {
            assert(0 <= i && i <= ReadLength() - 4);
            assert(0 <= j && j < TemplateLength() - 1);
            __m128 merge =  AFFINE4(params_.Merge,
                                    params_.MergeS,
                                    &features_.MergeQv[i]);
            __m128 noMerge = _mm_set_ps1(-FLT_MAX);
            float tplBase     = tpl_[j];
            float tplBaseNext = tpl_[j + 1];
            if (tplBase == tplBaseNext)
            {
                __m128 mask = _mm_cmpeq_ps(_mm_loadu_ps(&features_.SequenceAsFloat[i]),
                                           _mm_set_ps1(tplBase));
                return MUX4(mask, merge, noMerge);
            }
            else
            {
                return noMerge;
            }
        }

        __m128 Burst4(int i, int j, int hpLength) const
        {
            NotYetImplemented();
        }

    protected:
        QvSequenceFeatures features_;
        QvModelParams params_;
        std::string tpl_;
        bool pinStart_;
        bool pinEnd_;
    };
}

