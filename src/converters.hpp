/**
 * @file converters.hpp
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
#ifndef CONVERTERS_HPP_
#define CONVERTERS_HPP_

 #include <cstdint>

namespace majimix::converters {

/* ---------- decoders i16 ---------- */

std::int32_t ui8_to_i16(const char *data);

/*
 * int in_to_i16_le(const char*)
 * Little-endian signed int (i16, i24 or i32) to signed i16 decoder
 *      N data audio integer size
 *      N = 2 : signed i16 little endian to signed i16
 *      N = 3 : signed i24 little endian to signed i16
 *      N = 4 : signed i32 little endian to signed i16
 * data Audio data (little-endian)
 * return the converted signed i16 value
 */
template <int N>
std::int32_t in_to_i16_le(const char *data);

/*
 * int alaw(const char*)
 * a-law to signed i16 decoder
 * data Audio data
 * return the converted signed i16 value
 */
std::int32_t alaw(const char *data);

/*
 * int ulaw(const char*)
 * μ-law to signed i16 decoder
 * data Audio data
 * return the converted signed i16 value
 */
std::int32_t ulaw(const char *data);

/*
 * int float_to_i16(const char*)
 * IEEE float to signed i16 decoder
 *         FLOAT_TYPE data audio float type
 *         FLOAT_TYPE = float  : float to signed i16
 * 		   FLOAT_TYPE = double : double to signed i16
 * data Audio data
 * return the converted signed i16 value
 *
 * FIXME : endianness
 */
template <typename FLOAT_TYPE>
std::int32_t float_to_i16(const char *data);


/* ---------- decoders i24 ---------- */

/*
 * int ui8_to_i24(const char*)
 * Unsigned i8 to signed i24 decoder
 * data Audio data
 * return the converted signed i24 value
 */
std::int32_t ui8_to_i24(const char *data);

/*
 * int in_to_i24_le(const char*)
 * Little-endian  int (unsigned i8 and signed i16, i24 or i32) to signed i24 decoder
 *         N data audio integer size
 *         *** specialization  *** N = 1 : unsigned i8 little endian to signed i24
 *         *** specialization  *** N = 2 : signed i16 little endian to signed i24
 *         N = 3 : signed i24 little endian to signed i24
 *         N = 4 : signed i32 little endian to signed i24
 * data Audio data (little-endian)
 * return the converted signed i24 value
 */
template <int N>
std::int32_t in_to_i24_le(const char *data);

// /*
//  * int in_to_i24_le<2>(const char*)
//  * Little-endian signed i16 to signed i24 decoder
//  * data Audio data (little-endian)
//  * return the converted signed i24 value
//  */
template <>
std::int32_t in_to_i24_le<2>(const char *data);

// /*
//  * @fn int in_to_i24_le<1>(const char*)
//  * @brief Unsigned i8 to signed i24 decoder
//  *        Specialization for uint8 - 8bits pcm format is supposed to be unsigned
//  *        ui8_to_i24 equivalent
//  * @param data Audio data
//  * @return The converted signed i24 value
//  */
template <>
std::int32_t in_to_i24_le<1>(const char *data);




/*
 * @fn int alaw_i24(const char*)
 * @brief a-law to signed i24 decoder
 * @param data Audio data
 * @return The converted signed i24 value
 */
std::int32_t alaw_i24(const char *data);

/*
 * @fn int ulaw_i24(const char*)
 * @brief μ-law to signed i24 decoder
 * @param data Audio data
 * @return The converted signed i24 value
 */
std::int32_t ulaw_i24(const char *data);

/*
 * @fn int float_to_i24(const char*)
 * @brief IEEE float to signed i24 decoder
 * @tparam FLOAT_TYPE data audio float type
 *         FLOAT_TYPE = float  : float to signed i24
 * 		   FLOAT_TYPE = double : double to signed i24
 * @param data Audio data
 * @return The converted signed i24 value
 *
 * FIXME : endianness
 */
template <typename FLOAT_TYPE>
std::int32_t float_to_i24(const char *data);


}

#include "converters.inl"

#endif