/*
 * nvbio
 * Copyright (C) 2012-2014, NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#pragma once

#include <nvbio/basic/packedstream.h>
#include <nvbio/alignment/sink.h>

namespace nvbio {
namespace aln {

///@addtogroup Alignment
///@{

///@defgroup Utilities
///@{

/// A meta-function returning the type of the checkpoint cells for a given \ref Aligner "Aligner"
///
/// \tparam aligner_type        the queries \ref Aligner "Aligner" type
///
template <typename aligner_type> struct checkpoint_storage_type {
    typedef null_type type;    ///< the type of the checkpoint cells
};
template <AlignmentType TYPE, typename algorithm_tag>                        struct checkpoint_storage_type< EditDistanceAligner<TYPE,algorithm_tag> >                { typedef  int16 type; };
template <AlignmentType TYPE, typename scoring_type, typename algorithm_tag> struct checkpoint_storage_type< SmithWatermanAligner<TYPE,scoring_type,algorithm_tag> >  { typedef  int16 type; };
template <AlignmentType TYPE, typename scoring_type, typename algorithm_tag> struct checkpoint_storage_type< GotohAligner<TYPE,scoring_type,algorithm_tag> >          { typedef short2 type; };

/// A meta-function returning the type of the column cells for a given \ref Aligner "Aligner"
///
/// \tparam aligner_type        the queries \ref Aligner "Aligner" type
///
template <typename aligner_type> struct column_storage_type {
    typedef null_type type;    ///< the type of the column cells
};
template <AlignmentType TYPE, typename algorithm_tag>                        struct column_storage_type< EditDistanceAligner<TYPE,algorithm_tag> >                { typedef  int16 type; };
template <AlignmentType TYPE, typename scoring_type, typename algorithm_tag> struct column_storage_type< SmithWatermanAligner<TYPE,scoring_type,algorithm_tag> >  { typedef  int16 type; };
template <AlignmentType TYPE, typename scoring_type, typename algorithm_tag> struct column_storage_type< GotohAligner<TYPE,scoring_type,algorithm_tag> >          { typedef short2 type; };

/// A meta-function returning the number of bits required to represent the direction vectors
/// for a given \ref Aligner "Aligner"
///
/// \tparam aligner_type        the queries \ref Aligner "Aligner" type
///
template <typename aligner_type> struct direction_vector_traits {
    static const uint32 BITS = 2;   ///< the number of bits needed to encode direction vectors
};

/// A meta-function returning the number of bits required to represent the direction vectors
/// for a GotohAligner.
///
/// \tparam aligner_type        the queries \ref Aligner "Aligner" type
///
template <AlignmentType TYPE, typename scoring_type, typename algorithm_tag> struct direction_vector_traits<GotohAligner<TYPE,scoring_type,algorithm_tag> > {
    static const uint32 BITS = 4;   ///< the number of bits needed to encode direction vectors
};

///@}

///@defgroup Utilities
///@{

///
/// A simple implementation of the \ref SmithWatermanScoringScheme model
///
struct SimpleSmithWatermanScheme
{
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE SimpleSmithWatermanScheme() {}
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE SimpleSmithWatermanScheme(
        const int32 match, const int32 mm, const int32 del, const int32 ins) :
        m_match(match), m_mismatch(mm), m_deletion(del), m_insertion(ins) {}

    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE int32 match(const uint8 q = 0)      const { return m_match; };
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE int32 mismatch(const uint8 q = 0)   const { return m_mismatch; };
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE int32 mismatch(const uint8 a, const uint8 b, const uint8 q = 0)   const { return m_mismatch; };
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE int32 deletion()                    const { return m_deletion; };
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE int32 insertion()                   const { return m_insertion; };

    int32 m_match;
    int32 m_mismatch;
    int32 m_deletion;
    int32 m_insertion;
};

///
/// A simple implementation of the \ref GotohScoringScheme model
///
struct SimpleGotohScheme
{
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE SimpleGotohScheme() {}
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE SimpleGotohScheme(
        const int32 match, const int32 mm, const int32 gap_open, const int32 gap_ext) :
        m_match(match), m_mismatch(mm), m_gap_open(gap_open), m_gap_ext(gap_ext) {}

    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE int32 match(const uint8 q = 0)      const { return m_match; };
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE int32 mismatch(const uint8 q = 0)   const { return m_mismatch; };
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE int32 mismatch(const uint8 a, const uint8 b, const uint8 q = 0)   const { return m_mismatch; };
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE int32 pattern_gap_open()            const { return m_gap_open; };
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE int32 pattern_gap_extension()       const { return m_gap_ext; };
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE int32 text_gap_open()               const { return m_gap_open; };
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE int32 text_gap_extension()          const { return m_gap_ext; };

    int32 m_match;
    int32 m_mismatch;
    int32 m_gap_open;
    int32 m_gap_ext;
};

///
/// Calculate the maximum possible number of pattern gaps that could occur in a
/// given score boundary
///
template <AlignmentType TYPE, typename scoring_scheme_type, typename algorithm_tag>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE 
uint32 max_pattern_gaps(
    const SmithWatermanAligner<TYPE,scoring_scheme_type,algorithm_tag>& aligner,
	int32                                                               min_score,
    int32                                                               pattern_len);

///
/// Calculate the maximum possible number of reference gaps that could occur in a
/// given score boundary
///
template <AlignmentType TYPE, typename scoring_scheme_type, typename algorithm_tag>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE 
uint32 max_text_gaps(
    const SmithWatermanAligner<TYPE,scoring_scheme_type, algorithm_tag>& aligner,
	int32                                                                min_score,
    int32                                                                pattern_len);

///
/// Calculate the maximum possible number of pattern gaps that could occur in a
/// given score boundary
///
template <AlignmentType TYPE, typename scoring_scheme_type, typename algorithm_tag>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE 
uint32 max_pattern_gaps(
    const GotohAligner<TYPE,scoring_scheme_type,algorithm_tag>& aligner,
	int32                                                       min_score,
    int32                                                       pattern_len);

///
/// Calculate the maximum possible number of reference gaps that could occur in a
/// given score boundary
///
template <AlignmentType TYPE, typename scoring_scheme_type, typename algorithm_tag>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE 
uint32 max_text_gaps(
    const GotohAligner<TYPE,scoring_scheme_type,algorithm_tag>& aligner,
	int32                                                       min_score,
    int32                                                       pattern_len);

///
/// Calculate the maximum possible number of pattern gaps that could occur in a
/// given score boundary
///
template <AlignmentType TYPE, typename algorithm_tag>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE 
uint32 max_pattern_gaps(
    const EditDistanceAligner<TYPE,algorithm_tag>&  aligner,
	int32                                           min_score,
    int32                                           pattern_len);

///
/// Calculate the maximum possible number of reference gaps that could occur in a
/// given score boundary
///
template <AlignmentType TYPE, typename algorithm_tag>
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE 
uint32 max_text_gaps(
    const EditDistanceAligner<TYPE,algorithm_tag>&  aligner,
	int32                                           min_score,
    int32                                           pattern_len);

///
/// A trivial implementation of a quality string, constantly zero
///
struct trivial_quality_string
{
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE 
    uint8 operator[] (const uint32 i) const { return 0u; }
};

///
/// A trivial implementation of a quality string set, constantly zero
///
struct trivial_quality_string_set
{
    typedef trivial_quality_string string_type;

    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE 
    string_type operator[] (const uint32 i) const { return string_type(); }
};

///@} // end of Utilities group
///@} // end of Alignment group

} // namespace aln
} // namespace nvbio

#include <nvbio/alignment/utils_inl.h>
