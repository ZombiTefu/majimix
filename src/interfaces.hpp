/**
 * @file interfaces.hpp
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

#ifndef INTERFACES_HPP_
#define INTERFACES_HPP_

#include <memory>
#include <cstdint>

namespace majimix {

/**
 * @brief Samples audio format
 *
 * Accepted WAVE audio format
 *
 */
enum class AuFormat
{
    none,         /**< none */
    uint_8bits,   /**< uint_8bits unsigned i8 format */
    int_16bits,   /**< int_16bits signed i12 or i16 formats */
    int_24bits,   /**< int_24bits signed i24 format */
    int_32bits,   /**< int_32bits signed i32 format */
    float_32bits, /**< float_32bits IEEE float 32 bits format */
    float_64bits, /**< float_64bits IEEE float 64 bits format */
    alaw,         /**< alaw a-law format */
    ulaw          /**< ulaw µ-law format */
};

/* ------------ source & sample interfaces --------------- */

class Sample;

/**
 * @class Source
 * @brief A Source is an interface that supports the creation of Sample objects
 *
 */
class Source
{
public:
    virtual ~Source() = default;

    /**
     * @brief Set the output format for the Sample::read method.
     *        This is the format of the mixer.
     * @param samples_per_sec rate (samples per second)
     * @param channels number of channels (only 1 mono, 2 stereo are supported)
     * @param bits 16 ou 24
     */
    virtual void set_output_format(int samples_per_sec, int channels = 2, int bits = 16) = 0;

    /**
     * @brief Create a new Sample from this Source.
     * @return
     */
    virtual std::unique_ptr<Sample> create_sample() = 0;
};

/**
 * @class Sample
 * @brief Sample provides the sound data to the Majimix mixer.
 *
 */
class Sample
{
public:
    virtual ~Sample() = default;
    /**
     * Read the sample_count samples of this Sample and place the result into the buffer.
     * Returns the number of samples read. If this number is less than sample_count,
     * it means that the Sample has reached the end.
     * When a Sample reach the end, it will rewind automaticaly, the mixer can call Sample::read again to get data
     *
     * The size of a sample is equal to : number of channels. So the number of int elements
     * returned in out_buffer is equal to the number of channel x sample_count returned.
     *
     * @param[out] buffer output buffer
     * @param[in]  sample_count number of samples to process
     *
     * @return the number of sample processed (must be <= sample_count)
     *          (buffer will be filled with this returned value x nb_channels elements)
     */
    virtual int32_t read(int32_t *buffer, int32_t sample_count) = 0;

    /**
     * @fn void seek(int)=0
     * @brief Specify a specific point in the stream to begin or continue decoding.
     *        Seeks to a specific audio sample number, specified in pcm samples.
     * @param pos pcm sample number
     */
    virtual void seek(long pos) = 0;

    /**
     * @fn void seek_time(double)=0
     * @brief Specify a specific point in the stream to begin or continue decoding.
     *        Seeks to the specific time location in the stream, specified in seconds.
     * @param pos time location in seconds
     */
    virtual void seek_time(double pos) = 0;
};
}
#endif