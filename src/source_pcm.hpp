/**
 * @file source_pcm.hpp
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
#ifndef SOURCES_PCM_HPP_
#define SOURCES_PCM_HPP_

#include "interfaces.hpp"
#include <functional>

namespace majimix 
{

/**
 * @brief Base classe for PCM sources
 * 
 */
class SourcePCM : public Source 
{
protected:
    bool ready = false;

    /** sample format */
    AuFormat format;
    /** sample rate : sample per sec */
    int sample_rate;
    /** size of one sample in bytes : 1 sample <=> format size (octets) x channels  : ex 16 bits stereo => 2 x 2 = 4 */
    int sample_size;
    /** channels count */
    int channels;
    /** size of one channel = sample_size / channels */
    int channel_size;
    /** pcm data size in bytes  */
    int32_t data_size;
    /** number of sample */
    int32_t size;
    /** pcm data */
    std::vector<char> pcm;

    /** sample decoder */
    std::function<int32_t(const char *)> decoder;

    /** mixer format : rate */
    int mixer_rate;
    /** mixer format : bits  16 or 24 allowed */
    int mixer_bits;
    /** mixer format: channels - only 1 o 2 allowed */
    int mixer_channels;

    virtual void configure();

public:
    virtual ~SourcePCM() = default;
    void set_output_format(int samples_per_sec, int channels = 2, int bits = 16) override;
    bool load_wave(const std::string &filename);

    /* create a SamplePCM associated with this Source */
    virtual std::unique_ptr<Sample> create_sample() override = 0;
};





// /**
//  * @class SourcePCMI
//  * @brief PCM Source implementation using fixed point
//  *        All PCM data is stored in memory.
//  *        Used with WAVE files.
//  */
// class SourcePCMI : public SourcePCM
// {
//     /** Function pointer typedef for reading the sources */
//     using SourceReader = std::function<int32_t(int32_t *, int32_t, int32_t &, uint_fast64_t &)>;

//     /** step of the Sample */
//     uint_fast64_t sample_step;

//     /** function pointer to the template read function */
//     SourceReader read_fn;

//     /**
//      * @fn void configure()
//      * @brief verifies and completes source initialization
//      */
//     void configure() override;
 
//     /**
//      * @fn int read(int*, int, int&, uint_fast64_t&)
//      * @brief Read, decode the source, converts to the mixer format and fill the mixer output buffer
//      *
//      * @tparam STEREO_INPUT
//      * @tparam STEREO_OUTPUT
//      * @param out_buffer mixer output buffer
//      * @param sample_count number of output samples to process (buffer must be filled with sample_count x nb_mixer_channels elements)
//      * @param sample_idx
//      * @param sample_frac
//      * @return
//      */
//     template <bool STEREO_INPUT, bool STEREO_OUTPUT>
//     int32_t read(int32_t *out_buffer, int32_t sample_count, int32_t &sample_idx, uint_fast64_t &sample_frac);

//     friend class SamplePCMI;

// public:
//     /* create a SamplePCM associated with this Source */
//     std::unique_ptr<Sample> create_sample() override;
// };

// /**
//  * @class SamplePCMI
//  * @brief Sample associated with a SourcePCMI
//  *
//  */
// class SamplePCMI : public Sample
// {

//     /** SourcePCM associated with this sample */
//     const SourcePCMI *source;

//     /** sample index */
//     int32_t sample_idx = 0;

//     /** fractional part of the sample index */
//     uint_fast64_t sample_frac = 0;

// public:
//     SamplePCMI(const SourcePCMI &s);
//     int32_t read(int32_t *buffer, int32_t sample_count) override;
//     void seek(long pos) override;
//     void seek_time(double pos) override;

//     /** duration in seconds */
//     double sample_time() const;
// };




/**
 * @class SourcePCMF
 * @brief PCM Source implementation using floating point
 *        All PCM data is stored in memory.
 *        Used with WAVE files.
 */
class SourcePCMF : public SourcePCM
{
    /** Function pointer typedef for reading the sources */
    using SourceReader = std::function<int32_t(int32_t *, int32_t, double&)>;

    /** step of the Sample */
    double sample_step;

    /** function pointer to the template read function */
    SourceReader read_fn;

    /**
     * @fn void configure()
     * @brief verifies and completes source initialization
     */
    void configure() override;
 
    /**
     * @fn int read(int*, int, int&, uint_fast64_t&)
     * @brief Read, decode the source, converts to the mixer format and fill the mixer output buffer
     *
     * @tparam STEREO_INPUT
     * @tparam STEREO_OUTPUT
     * @param out_buffer mixer output buffer
     * @param sample_count number of output samples to process (buffer must be filled with sample_count x nb_mixer_channels elements)
     * @param sample_idx
     * @param sample_frac
     * @return
     */
    template <bool STEREO_INPUT, bool STEREO_OUTPUT>
    int32_t read(int32_t *out_buffer, int32_t sample_count, double &sample_idx) const;

    friend class SamplePCMF;

public:
    /* create a SamplePCM associated with this Source */
    std::unique_ptr<Sample> create_sample() override;
};

class SamplePCMF : public Sample
{
    /** SourcePCM associated with this sample */
    const SourcePCMF *source;

    /** sample index */
    double sample_idx = 0;

public:
    SamplePCMF(const SourcePCMF &s);
    int32_t read(int32_t *buffer, int32_t sample_count) override;
    void seek(long pos) override;
    void seek_time(double pos) override;

    /** duration in seconds */
    double sample_time() const;
};


}
#endif