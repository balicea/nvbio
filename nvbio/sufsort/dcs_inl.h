/*
 * nvbio
 * Copyright (C) 2011-2014, NVIDIA Corporation
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

namespace nvbio {

///
/// Routune for calculating the elements of a difference cover using the
/// technique of Colbourn and Ling.
///
/// See http://citeseer.ist.psu.edu/211575.html
///
/// \param  r           the r parameter, determining the DC size as in Corollary 2.3
/// \param  samples     output vector of DC samples
///
inline
void calculateColbournAndLingDC(const uint32 r, uint32& maxv, std::vector<uint32>& samples)
{
	maxv = 24*r*r + 36*r + 13; // Corollary 2.3

    uint32 numsamp = 6*r + 4;

    // allocate enough storage
    samples.resize( numsamp );

    // initialize samples to 0
    for (uint32 i = 0; i < numsamp; ++i)
        samples[i] = 0;

	// fill in the 1^r part of the B series
	for (uint32 i = 1; i < r+1; i++)
		samples[i] = samples[i-1] + 1;

	// fill in the (r + 1)^1 part
	samples[r+1] = samples[r] + r + 1;

    // fill in the (2r + 1)^r part
	for (uint32 i = r+2; i < r+2+r; i++)
		samples[i] = samples[i-1] + 2*r + 1;

    // fill in the (4r + 3)^(2r + 1) part
	for (uint32 i = r+2+r; i < r+2+r+2*r+1; i++)
		samples[i] = samples[i-1] + 4*r + 3;

    // fill in the (2r + 2)^(r + 1) part
	for (uint32 i = r+2+r+2*r+1; i < r+2+r+2*r+1+r+1; i++)
		samples[i] = samples[i-1] + 2*r + 2;

	// fill in the last 1^r part
	for (uint32 i = r+2+r+2*r+1+r+1; i < r+2+r+2*r+1+r+1+r; i++)
		samples[i] = samples[i-1] + 1;
}

// a utility SuffixHandler to rank the sorted suffixes
//
struct DCSSuffixRanker
{
    // constructor
    //
    DCSSuffixRanker(DCSView _dcs) : dcs( _dcs ), n_output(0) {}

    // process the next batch of suffixes
    //
    void process_batch(
        const uint32  n_suffixes,
        const uint32* d_suffixes)
    {
        // essentially, invert the suffix array
        thrust::scatter(
            thrust::make_counting_iterator<uint32>( n_output ),
            thrust::make_counting_iterator<uint32>( n_output ) + n_suffixes,
            thrust::make_transform_iterator(
                thrust::device_ptr<const uint32>( d_suffixes ),
                priv::DCS_string_suffix_index( dcs ) ),         // localize the index to the DCS
            thrust::device_ptr<uint32>( dcs.ranks ) );

        n_output += n_suffixes;
    }

    // process a sparse set of suffixes
    //
    void process_scattered(
        const uint32  n_suffixes,
        const uint32* d_suffixes,
        const uint32* d_slots)
    {
        // essentially, invert the suffix array
        thrust::scatter(
            thrust::device_ptr<const uint32>( d_slots ),
            thrust::device_ptr<const uint32>( d_slots ) + n_suffixes,
            thrust::make_transform_iterator(
                thrust::device_ptr<const uint32>( d_suffixes ),
                priv::DCS_string_suffix_index( dcs ) ),         // localize the index to the DCS
            thrust::device_ptr<uint32>( dcs.ranks ) );
    }

    const DCSView dcs;
    uint32        n_output;
};

// return the sampled position of a given suffix index
//
NVBIO_FORCEINLINE NVBIO_HOST_DEVICE
uint32 DCSView::index(const uint32 i) const
{
    const uint32 block_i = i / Q;
    const uint32 mod_i   = i & (Q-1);

    return block_i * N + pos[ mod_i ];
}

// constructor
//
template <uint32 QT>
void DCS::init()
{
    // build a table for our Difference Cover
    const uint32* h_dc = DCTable<QT>::S();

    Q = QT;
    N = DCTable<QT>::N;

    thrust::host_vector<uint8>    h_bitmask( Q, 0u );
    thrust::host_vector<uint32>   h_lut( Q*Q, 0u );
    thrust::host_vector<uint32>   h_pos( Q, 0u );

    // build the DC bitmask
    thrust::scatter(
        thrust::make_constant_iterator<uint32>(1u),
        thrust::make_constant_iterator<uint32>(1u) + N,
        h_dc,
        h_bitmask.begin() );

    // build the DC position table, mapping each entry in DC to its position (q -> i | DC[i] = q)
    thrust::scatter(
        thrust::make_counting_iterator<uint32>(0u),
        thrust::make_counting_iterator<uint32>(0u) + N,
        h_dc,
        h_pos.begin() );

    // build the LUT (i,j) -> l | [(i + l) in DC && (j + l) in DC]
    for (uint32 i = 0; i < Q; ++i)
    {
        for (uint32 j = 0; j < Q; ++j)
        {
            for (uint32 l = 0; l < Q; ++l)
            {
                if (h_bitmask[ (i + l) & (Q-1) ] &&
                    h_bitmask[ (j + l) & (Q-1) ])
                {
                    h_lut[ i * Q + j ] = l;
                    break;
                }
                if (l == Q-1)
                    throw nvbio::logic_error("DCS: could not find a period for (%u,%u)!\n", i, j);
            }
        }
    }

    // copy all tables to the device
    d_dc.resize( N );
    thrust::copy(
        h_dc,
        h_dc + N,
        d_dc.begin() );

    d_lut     = h_lut;
    d_pos     = h_pos;
    d_bitmask = h_bitmask;
}

} // namespace nvbio
