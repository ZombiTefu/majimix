/**
 * @file converters.inl
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

#ifndef CONVERTERS_INL_
#define CONVERTERS_INL_

namespace majimix::converters {

/* ---------- decoders i16 ---------- */

template <int N>
std::int32_t in_to_i16_le(const char *data)
{
	return static_cast<unsigned char>(data[N - 2]) | (data[N - 1] << 8);
}

template <typename FLOAT_TYPE>
std::int32_t float_to_i16(const char *data)
{
	FLOAT_TYPE v = *reinterpret_cast<const FLOAT_TYPE *>(data);
	return v * 0x7FFF;
}

/* ---------- decoders i24 ---------- */

template <int N>
std::int32_t in_to_i24_le(const char *data)
{
	return static_cast<unsigned char>(data[N - 3]) | static_cast<unsigned char>(data[N - 2]) << 8 | (data[N - 1] << 16);
}


template <typename FLOAT_TYPE>
std::int32_t float_to_i24(const char *data)
{
	FLOAT_TYPE v = *(reinterpret_cast<const FLOAT_TYPE *>(data));
	return v * 0x7FFFFF;
}
}

#endif