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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <boost/assign.hpp>
#include <boost/assign/std/set.hpp>
#include <boost/range/as_array.hpp>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "Utils.hpp"
#include "Mutation.hpp"

using std::string;
using std::vector;
using std::cout;
using std::endl;

using namespace boost::assign;  // NOLINT
using namespace ConsensusCore;  // NOLINT

using ::testing::ElementsAreArray;

// Test that mutations get correctly applied to strings

TEST(MutationTest, BasicTest)
{
    string tpl = "ACGTACGTACGT";
    Mutation m(SUBSTITUTION, 0, 'C');
    EXPECT_EQ("CCGTACGTACGT", ApplyMutation(m, tpl));
    EXPECT_EQ("ACGTACGTACGT", tpl);
}

TEST(MutationTest, DeleteTest)
{
    string tpl = "ACGTACGTACGT";
    Mutation m(DELETION, 4, 'C');
    EXPECT_EQ("ACGTCGTACGT", ApplyMutation(m, tpl));
    EXPECT_EQ("ACGTACGTACGT", tpl);
}

TEST(MutationTest, InsertTest)
{
    string tpl = "ACGTACGTACGT";
    Mutation m(INSERTION, 0, 'C');
    EXPECT_EQ("CACGTACGTACGT", ApplyMutation(m, tpl));
    EXPECT_EQ("ACGTACGTACGT", tpl);
}

TEST(MutationTest, ApplyMutationsTest)
{
    string tpl = "GATTACA";
    Mutation m1(INSERTION, 0, 'G');
    Mutation m2(INSERTION, 2, 'T');
    Mutation m3(INSERTION, 3, 'C');
    Mutation m4(DELETION, 4, '-');
    Mutation m5(SUBSTITUTION, 6, 'T');

    EXPECT_TRUE(m1 < m2);
    EXPECT_TRUE(m2 < m3);
    EXPECT_TRUE(m3 < m4);
    EXPECT_TRUE(m4 < m5);

    std::vector<Mutation*> muts;
    muts += &m3, &m2, &m1, &m5, &m4;  // put in arbitrary order

    EXPECT_EQ("GGATTCTCT", ApplyMutations(muts, tpl));
    EXPECT_EQ("GATTACA", tpl);
}


TEST(MutationTest, ApplyMutationsToSamePositionTest)
{
    // Test the very real scenario of Ins@x, Subs@x.
    string tpl = "GATTACA";
    Mutation m1(INSERTION, 2, 'T');
    Mutation m2(SUBSTITUTION, 2, 'A');

    std::vector<Mutation*> muts;
    muts += &m2, &m1;

    EXPECT_EQ("GATATACA", ApplyMutations(muts, tpl));
}

TEST(MutationTest, MutationsToTranscript)
{
    //                 0123456
    std::string tpl = "GATTACA";
    Mutation insertMutation1(INSERTION, 1, 'T');
    Mutation insertMutation2(INSERTION, 5, 'C');

    std::vector<Mutation*> muts;
    ASSERT_EQ("MMMMMMM", MutationsToTranscript(muts, tpl));

    muts += &insertMutation2, &insertMutation1;
    ASSERT_EQ("MIMMMMIMM", MutationsToTranscript(muts, tpl));

    std::vector<Mutation*> muts2;
    muts2 += new Mutation(DELETION, 2, '-'),
             new Mutation(INSERTION, 5, 'C'),
             new Mutation(SUBSTITUTION, 4, 'G');
    ASSERT_EQ("MMDMRIMM", MutationsToTranscript(muts2, tpl));
}

TEST(MutationTest, MutatedTemplatePositionsTest)
{
    // Test spec comment:
    //               01234567                           0123456
    //              "GATTACA" -> (Del T@2, Ins C@5) -> "GATACCA";
    //    here mtp = 01223567.
    string tpl = "GATTACA";
    std::vector<Mutation*> muts;
    muts += new Mutation(DELETION, 2, '-'),
            new Mutation(INSERTION, 5, 'C'),
            new Mutation(SUBSTITUTION, 4, 'G');

    int expectedMtp[] = { 0, 1, 2, 2, 3, 5, 6, 7 };
    ASSERT_THAT(TargetToQueryPositions(muts, tpl), ElementsAreArray(expectedMtp));

    // "GG" -> (Ins A@0) -> "AGG": mtp = 123
    std::string tpl2 = "GG";
    std::vector<Mutation*> muts2;
    muts2 += new Mutation(INSERTION, 0, 'A');
    int expectedMtp2[] = { 1, 2, 3 };
    ASSERT_THAT(TargetToQueryPositions(muts2, tpl2), ElementsAreArray(expectedMtp2));

    // "AGG" -> (Del A@0) -> "GG": mtp = 0012
    std::string tpl3 = "AGG";
    std::vector<Mutation*> muts3;
    muts3 += new Mutation(DELETION, 0, '-');
    int expectedMtp3[] = { 0, 0, 1, 2 };
    ASSERT_THAT(TargetToQueryPositions(muts3, tpl3), ElementsAreArray(expectedMtp3));
}
