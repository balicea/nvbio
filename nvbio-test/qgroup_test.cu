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

// qgroup_test.cu
//
//#define CUFMI_CUDA_DEBUG
//#define CUFMI_CUDA_ASSERTS

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>
#include <nvbio/basic/timer.h>
#include <nvbio/basic/console.h>
#include <nvbio/basic/vector_wrapper.h>
#include <nvbio/basic/packedstream.h>
#include <nvbio/basic/shared_pointer.h>
#include <nvbio/io/reads/reads.h>
#include <nvbio/qgroup/qgroup.h>

namespace nvbio {

int qgroup_test(int argc, char* argv[])
{
    uint32 len   = 10000000;
    char*  reads = "./data/SRR493095_1.fastq.gz";

    for (int i = 0; i < argc; ++i)
    {
        if (strcmp( argv[i], "-length" ) == 0)
            len = atoi( argv[++i] )*1000;
        if (strcmp( argv[i], "-reads" ) == 0)
            reads = argv[++i];
    }

    fprintf(stderr, "qgroup test... started\n");

    const io::QualityEncoding qencoding = io::Phred33;

    fprintf(stderr, "loading reads... started\n");

    SharedPointer<io::ReadDataStream> read_data_file(
        io::open_read_file(
            reads,
            qencoding,
            uint32(-1),
            uint32(-1) ) );

    if (read_data_file == NULL || read_data_file->is_ok() == false)
    {
        log_error(stderr, "    failed opening file \"%s\"\n", reads);
        return 1u;
    }

    const uint32 batch_size = 512*1024;
    const uint32 batch_bps  = len;

    // load a batch of reads
    SharedPointer<io::ReadData> h_read_data( read_data_file->next( batch_size, batch_bps ) );
    
    // build its device version
    io::ReadDataCUDA d_read_data( *h_read_data );

    fprintf(stderr, "loading reads... done\n");

    // fetch the actual string
    typedef io::ReadData::const_read_stream_type string_type;

    const uint32      string_len = d_read_data.bps();
    const string_type string     = string_type( d_read_data.read_stream() );

    // build the Q-Group
    QGroupDevice qgroup;

    Timer timer;
    timer.start();

    qgroup.build<io::ReadData::READ_BITS>(
        8u,
        string_len,
        string );

    cudaDeviceSynchronize();
    timer.stop();
    const float time = timer.seconds();

    fprintf(stderr, "qgroup test... done: %.1f M qgrams/s\n", 1.0e-6f * float( string_len ) / time );
    return 0;
}

} // namespace nvbio
