/**
 * @file converters.cpp
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

#include "converters.hpp"
#include "wave.hpp"


namespace majimix::converters {

/* ---------- decoders i16 ---------- */

std::int32_t ui8_to_i16(const char *data)
{
	return ((unsigned char)*data << 8) - 0x8000;
}

std::int32_t alaw(const char *data)
{
	return wave::ALaw_Decode(*data);
}

std::int32_t ulaw(const char *data)
{
	return wave::MuLaw_Decode(*data);
}


/* ---------- decoders i24 ---------- */

std::int32_t ui8_to_i24(const char *data)
{
	return ((unsigned char)*data << 16) - 0x800000;
}
template <>
std::int32_t in_to_i24_le<2>(const char *data)
{
	return static_cast<unsigned char>(data[0]) << 8 | (data[1] << 16);
}
 
template <>
std::int32_t in_to_i24_le<1>(const char *data)
{
	return ((unsigned char)*data << 16) - 0x800000;
}
std::int32_t alaw_i24(const char *data)
{
	return wave::ALaw_Decode(*data) << 8;
}

std::int32_t ulaw_i24(const char *data)
{
	return wave::MuLaw_Decode(*data) << 8;
}

}
