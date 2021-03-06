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

// Author: Patrick Marks, David Alexander

#include "Mutation.hpp"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <cassert>
#include <string>
#include <vector>

#include <iostream>

#include "Types.hpp"
#include "PairwiseAlignment.hpp"
#include "Utils.hpp"

using std::max;

namespace ConsensusCore
{
    Mutation::Mutation(MutationType type, int position, char base)
        : type_(type),
          position_(position),
          base_(base)
    {
        if (!(base == 'A' ||
              base == 'C' ||
              base == 'G' ||
              base == 'T' ||
              base == '-'))
            throw InvalidInputError();
    }

    bool
    Mutation::IsSubstitution() const
    {
        return (type_ == SUBSTITUTION);
    }

    bool
    Mutation::IsInsertion() const
    {
        return (type_ == INSERTION);
    }

    bool
    Mutation::IsDeletion() const
    {
        return (type_ == DELETION);
    }

    int
    Mutation::Position() const
    {
        return position_;
    }

    char
    Mutation::Base() const
    {
        return base_;
    }

    MutationType
    Mutation::Type() const
    {
        return type_;
    }

    int Mutation::LengthDiff() const
    {
        if (IsInsertion())
            return 1;
        else if (IsDeletion())
            return -1;
        return 0;
    }

    std::string
    Mutation::ToString() const
    {
        using boost::str;
        using boost::format;

        switch (Type())
        {
            case INSERTION:    return str(format("Insertion (%c) @%d") % base_ % position_);
            case DELETION:     return str(format("Deletion @%d") % position_);
            case SUBSTITUTION: return str(format("Substitution (%c) @%d") % base_ % position_);
            default: ShouldNotReachHere();
        }
    }

    bool
    Mutation::operator==(const Mutation& other) const
    {
        return (Position() == other.Position() &&
                Type()     == other.Type()     &&
                Base()     == other.Base());
    }

    bool
    Mutation::operator<(const Mutation& other) const
    {
        if (Position() != other.Position()) { return Position() < other.Position(); }
        if (Type()     != other.Type())     { return Type()     < other.Type();     }
        return Base() < other.Base();
    }

    static void
    _ApplyMutationInPlace(const Mutation& mut, int position, std::string* tpl)
    {
        if (mut.IsSubstitution())
        {
            (*tpl)[position] = mut.Base();
        }
        else if (mut.IsDeletion())
        {
            (*tpl).erase(position, 1);
        }
        else if (mut.IsInsertion())
        {
            (*tpl).insert(position, 1, mut.Base());
        }
    }

    std::string
    ApplyMutation(const Mutation& mut, const std::string& tpl)
    {
        std::string tplCopy(tpl);
        _ApplyMutationInPlace(mut, mut.Position(), &tplCopy);
        return tplCopy;
    }

    struct compareMutationPointers {
        bool operator() (Mutation* lhs, Mutation* rhs) { return *lhs < *rhs; }
    };

    std::string
    ApplyMutations(const std::vector<Mutation*>& muts, const std::string& tpl)
    {
        std::string tplCopy(tpl);
        std::vector<Mutation*> sortedMuts(muts);
        std::sort(sortedMuts.begin(), sortedMuts.end(), compareMutationPointers());
        int runningLengthDiff = 0;
        foreach (const Mutation* mut, sortedMuts)
        {
            _ApplyMutationInPlace(*mut, mut->Position() + runningLengthDiff, &tplCopy);
            runningLengthDiff += mut->LengthDiff();
        }
        return tplCopy;
    }


    std::string MutationsToTranscript(const std::vector<Mutation*>& mutations,
                                      const std::string& tpl)
    {
        std::vector<Mutation*> sortedMuts(mutations);
        std::sort(sortedMuts.begin(), sortedMuts.end(), compareMutationPointers());

        // Build an alignnment transcript corresponding to these mutations.
        int tpos = 0;
        std::string transcript = "";
        foreach (const Mutation* m, sortedMuts)
        {
            for (; tpos < m->Position(); ++tpos)
            {
                transcript.push_back('M');
            }

            if (m->IsInsertion())
            {
                transcript.push_back('I');
            }
            else if (m->IsDeletion())
            {
                transcript.push_back('D');
                ++tpos;
            }
            else if (m->IsSubstitution())
            {
                transcript.push_back('R');
                ++tpos;
            }
            else
            {
                ShouldNotReachHere();
            }
        }
        for (; tpos < (int)tpl.length(); ++tpos)
        {
            transcript.push_back('M');
        }
        return transcript;
    }

    // MutatedTemplatePositions:
    //  * Returns a vector of length (tpl.length()+1), which, roughly speaking,
    //    indicates the positions in the mutated template tpl' of the characters
    //    in tpl.
    //  * More precisely, for any slice [s, e) of tpl, letting:
    //      - t[s, e) denote the induced substring of the template;
    //      - m[s, e) denote the subvector of mutations with Position
    //        in [s, e);
    //      - t' denote the mutated template; and
    //      - t[s, e)' denote the result of applying mutation m[s, e) to t[s, e),
    //    the resultant vector mtp satisfies t'[mtp[s], mtp[e]) == t[s,e)'.
    //  * Example:
    //               01234567                           0123456
    //              "GATTACA" -> (Del T@2, Ins C@5) -> "GATACCA";
    //    here mtp = 01223567, which makes sense, because for instance:
    //      - t[0,3)=="GAT" has become t'[0,2)=="GA";
    //      - t[0,2)=="GA"  remains "GA"==t'[0,2);
    //      - t[4,7)=="ACA" has become t[3,7)=="ACCA",
    //      - t[5,7)=="CA"  remains "CA"==t'[5,7).
    //
    std::vector<int> TargetToQueryPositions(const std::vector<Mutation*>& mutations,
                                            const std::string& tpl)
    {
        return TargetToQueryPositions(MutationsToTranscript(mutations, tpl));
    }
}
