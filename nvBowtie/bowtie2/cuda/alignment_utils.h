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

#include <nvBowtie/bowtie2/cuda/string_utils.h>
#include <nvbio/alignment/alignment.h>
#include <nvbio/alignment/batched.h>
#include <nvbio/io/utils.h>
#include <nvbio/basic/vector_wrapper.h>

namespace nvbio {
namespace bowtie2 {
namespace cuda {

namespace detail {

///@addtogroup nvBowtie
///@{

///@addtogroup Alignment
///@{

///@addtogroup AlignmentDetail
///@{

///
/// Frame the opposite mate alignment's direction & orientation
///
/// \param policy       input paired end policy
/// \param anchor       0 iff anchor is mate 1, 1 otherwise
/// \param anchor_fw    true iff anchor aligned forward
/// \param left         true iff opposite mate must be to the left of the anchor
/// \param fw           true iff opposite mate must align forward
///
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
void frame_opposite_mate(
    const int       policy,
	const uint32    anchor,
	const bool      anchor_fw,
	      bool&     left,
	      bool&     fw)
{
    const bool anchor_1 = (anchor == 0);

    switch(policy)
    {
        case io::PE_POLICY_FF:
        {
			left = (anchor_1 != anchor_fw);
			fw   =  anchor_fw;
			break;
		}
		case io::PE_POLICY_RR:
        {
			left = (anchor_1 == anchor_fw);
			fw   =  anchor_fw;
			break;
		}
		case io::PE_POLICY_FR:
        {
			left = !anchor_fw;
			fw   = !anchor_fw;
			break;
		}
        case io::PE_POLICY_RF:
        {
			left =  anchor_fw;
			fw   = !anchor_fw;
			break;
		}
	}
}

///
/// compute the target minimum score for a valid extension of a paired-end alignment
///
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE 
int32 compute_target_score(const io::BestPairedAlignments& best, const int32 a_worst_score, const int32 o_worst_score)
{
    if (best.has_second_paired() == false)
        return a_worst_score + o_worst_score;

    // reproduce Bowtie2's score bounding behavior (under its default setting 'tighten = 3')
    const int32 delta = best.best_score() - best.second_score();
    return best.second_score() + delta * 3/4;
}

enum AlignmentStreamType
{
    SCORE_STREAM      = 0,
    TRACEBACK_STREAM  = 1,
};

///
/// A backtracking context, forming a CIGAR on the go.
/// As the length is unknown till the very end, the CIGAR is stored backwards.
///
template <typename vector>
struct Backtracker
{
    // constructor
    NVBIO_FORCEINLINE NVBIO_DEVICE 
    Backtracker(vector vec, const uint32 _capacity) : out(vec), size(0), prev(255), capacity(_capacity) {}

    // encode a soft clipping operation
    NVBIO_FORCEINLINE NVBIO_DEVICE 
    void clip(const uint32 l)
    {
        if (l)
        {
            NVBIO_CUDA_DEBUG_ASSERT( size < capacity, "Backtracker: exceeded maximum CIGAR size!\n" );
            out[size++] = io::Cigar( io::Cigar::SOFT_CLIPPING, l );
        }
    }

    // encode a new operation
    NVBIO_FORCEINLINE NVBIO_DEVICE 
    void push(uint8 type)
    {
        NVBIO_CUDA_DEBUG_ASSERT( type == aln::SUBSTITUTION ||
                                 type == aln::DELETION     ||
                                 type == aln::INSERTION,
                                 "Backtracker: unknown op type %u\n", uint32(type) );

        // perform run-length encoding of the symbol sequence
        if (prev == type)
            out[size-1u].m_len++;
        else
        {
            NVBIO_CUDA_DEBUG_ASSERT( size < capacity, "Backtracker: exceeded maximum CIGAR size!\n" );
            // encode a new symbol
            out[size++] = io::Cigar( type, 1u );
            prev        = type;
        }
    }

    vector out;
    uint32 size;
    uint8  prev;
    uint32 capacity;
};

/// Base class for the alignment contexts
///
template <AlignmentStreamType TYPE>
struct AlignmentStreamContext {};

/// Base class for the scoring alignment contexts
///
template <>
struct AlignmentStreamContext<SCORE_STREAM>
{
    aln::BestSink<int32>    sink;           ///< output alignment sink
};

/// Base class for the traceback alignment contexts
///
template <>
struct AlignmentStreamContext<TRACEBACK_STREAM>
{
    io::Cigar               cigar[1024];    ///< output CIGAR storage
    Backtracker<io::Cigar*> backtracer;     ///< output alignment backtracer
    aln::Alignment<int32>   alignment;      ///< output alignment

    /// constructor
    ///
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
    AlignmentStreamContext<TRACEBACK_STREAM>() : backtracer( cigar, 1024u ) {}
};

///
/// A base stream class to be used in conjunction with the aln::BatchedAlignmentScore class.
/// This class implements the load_strings() method, which performs read and genome loading, and
/// defers the implementation of init_context() to its inheritors, BestScoreStream,
/// BestAnchorScoreStream, and BestOppositeScoreStream.
///
template <AlignmentStreamType TYPE, typename AlignerType, typename PipelineType>
struct AlignmentStreamBase
{
    typedef typename PipelineType::genome_iterator                                  genome_iterator;
    typedef typename PipelineType::read_batch_type                                  read_batch_type;
    typedef typename PipelineType::scheme_type                                      scheme_type;
    typedef AlignerType                                                             aligner_type;

    static const uint32 CACHE_SIZE = 64;
    typedef nvbio::lmem_cache_tag<CACHE_SIZE>                                       lmem_cache_type;

    typedef          ReadLoader<read_batch_type,lmem_cache_type >                   pattern_loader_type;
    typedef typename pattern_loader_type::string_type                               pattern_string;
    typedef typename pattern_string::qual_string_type                               qual_string;

    typedef GenomeLoader<genome_iterator,lmem_cache_type>                           text_loader_type;
    typedef typename text_loader_type::string_type                                  text_string;

    /// an alignment context
    ///
    struct context_type : AlignmentStreamContext<TYPE>
    {
        uint32                  idx;
        uint32                  mate;
        uint2                   read_range;
        uint32                  read_id;
        uint32                  read_rc;
        uint32                  genome_begin;
        uint32                  genome_end;
        int32                   min_score;
    };
    /// a container for the strings to be aligned
    ///
    struct strings_type
    {
        pattern_loader_type     pattern_loader;
        text_loader_type        text_loader;
        pattern_string          pattern;
        qual_string             quals;
        text_string             text;
    };

    /// constructor
    ///
    AlignmentStreamBase(
        const PipelineType          _pipeline,
        const aligner_type          _aligner,
        const ParamsPOD             _params) :
        m_pipeline( _pipeline ), m_aligner( _aligner ), m_params( _params ) {}

    /// get the aligner
    ///
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
    const aligner_type& aligner() const { return m_aligner; };

    /// return the pattern length
    ///
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
    uint32 pattern_length(const uint32 i, const context_type* context) const
    {
        return context->read_range.y - context->read_range.x;
    }

    /// return the text length
    ///
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
    uint32 text_length(const uint32 i, const context_type* context) const
    {
        return context->genome_end - context->genome_begin;
    }

    /// initialize the strings corresponding to the i-th context
    ///
    /// \param i                problem index
    /// \param window_begin     pattern window to load
    /// \param window_end       pattern window to load
    /// \param context          initialized context
    /// \param strings          output strings
    NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
    void load_strings(
        const uint32        i,
        const uint32        window_begin,
        const uint32        window_end,
        const context_type* context,
              strings_type* strings) const
    {
        // select the read batch based on context->mate
        read_batch_type reads = context->mate ?
            m_pipeline.reads_o :
            m_pipeline.reads;

        const uint2 read_subrange = make_uint2( window_begin, window_end );

        strings->pattern = strings->pattern_loader.load(
            reads,
            context->read_range,
            context->read_rc ? FORWARD    : REVERSE,        // the reads are loaded in REVERSE fashion, so we invert this flag here
            context->read_rc ? COMPLEMENT : STANDARD,
            read_subrange );

        strings->quals   = strings->pattern.qualities();
        strings->text    = strings->text_loader.load( m_pipeline.genome, context->genome_begin, context->genome_end - context->genome_begin );
    }

    PipelineType    m_pipeline;     ///< the pipeline object
    aligner_type    m_aligner;      ///< the aligner
    ParamsPOD       m_params;       ///< global parameters
};

///@}  // group AlignmentDetail
///@}  // group Alignment
///@}  // group nvBowtie

} // namespace detail

} // namespace cuda
} // namespace bowtie2
} // namespace nvbio
