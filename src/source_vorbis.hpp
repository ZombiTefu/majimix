/**
 * @file source_vorbis.hpp
 * @author  François Jacobs
 * @date 2022-04-12
 *
 * @section majimix_lic LICENSE
 *
 * The MIT License (MIT)
 *
 * @copyright Copyright © 2022 - François Jacobs
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the “Software”), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef SOURCE_VORBIS_HPP_
#define SOURCE_VORBIS_HPP_

#include "interfaces.hpp"
#include <vorbis/vorbisfile.h>
#include <functional>
#include <fstream>

namespace majimix 
{
class SourceVorbis : public Source {

    std::string filename;

    /* sample decoder */
    std::function<int32_t(const char *)> decoder;
    int mixer_rate;
    int mixer_bits; // 16/24
    int mixer_channels;

    /* step of the Sample */
    // friend class SampleVorbis;
    friend class SampleVorbisF;

public:
    bool set_file(const std::string &filename);
    /* set the mixer format */
    void set_output_format(int samples_per_sec, int channels = 2, int bits = 16) override;
    /* create a SampleVorbis associated with this Source */
    std::unique_ptr<Sample> create_sample() override;
};

// class SampleVorbis : public Sample
// {
//     const SourceVorbis *source;
//     std::ifstream stream;
//     OggVorbis_File file;

//     /* sample rate : sample per sec */
//     int sample_rate;
//     /* size of one sample : 1 sample <=> format size (octets) x channels  : ex 16 bits stereo => 2 x 2 = 4 */
//     int sample_size;
//     /* channels count */
//     int channels;
//     /* size of one channel = sample_size / channels */
//     int channel_size;

//     uint_fast64_t sample_step;

//     /* multistream support */
//     int current_section, last_section;

//     uint_fast64_t sample_frac;
//     int32_t idx_1, idx_2, idx_lim;

//     bool initialized = false;
//     friend class SourceVorbis;

//     constexpr static int internal_buffer_size = 4096;
//     char internal_buffer[internal_buffer_size];
//     int32_t buffer_read_length = 0;

//     /* verifies and completes source initialization */
//     void configure();

// public:
//     SampleVorbis(const SourceVorbis &s);
//     ~SampleVorbis();
//     int32_t read(int32_t *buffer, int32_t sample_count) override;
//     //	int read_test(char* buffer, int sample_count);
//     void seek(long pos) override;
//     void seek_time(double pos) override;
//     /* duration in seconds */
//     double sample_time();
// };


class SampleVorbisF : public Sample
{
    const SourceVorbis *source;
    std::ifstream stream;
    OggVorbis_File file;

    /* sample rate : sample per sec */
    int sample_rate;
    /* size of one sample : 1 sample <=> format size (octets) x channels  : ex 16 bits stereo => 2 x 2 = 4 */
    int sample_size;
    /* channels count */
    int channels;
    /* size of one channel = sample_size / channels */
    int channel_size;

    double sample_step;

    /* multistream support */
    int current_section, last_section;

    int32_t min_buffer_data_size;
    int32_t max_buffer_idx;
    double sample_pos = 0;

    bool initialized = false;

    constexpr static int internal_buffer_size = 4096;
    char internal_buffer[internal_buffer_size];
    int32_t buffer_read_length; // = 0;

    /* verifies and completes source initialization */
    void configure();

public:
    SampleVorbisF(const SourceVorbis &s);
    ~SampleVorbisF();
    int32_t read(int32_t *buffer, int32_t sample_count) override;
    //	int read_test(char* buffer, int sample_count);
    void seek(long pos) override;
    void seek_time(double pos) override;
    /* duration in seconds */
    double sample_time();
};

}

#endif
