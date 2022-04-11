/**
 * wave.hpp
 *
 * @author  François Jacobs
 * @date 07/03/2022
 *
 * @section majimix_lic_hpp LICENSE
 *
 * The MIT License (MIT)
 *
 * Copyright © 2022 - François Jacobs
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

#ifndef WAVE_HPP_
#define WAVE_HPP_

#include <vector>
#include <cstdint>
#include <iostream>

namespace majimix::wave
{

extern const bool little_endian;

// WAVE Format

struct fmt_base
{
	// fmt
	uint16_t wFormatTag = 0;	  // Format code
	uint16_t nChannels = 0;		  // Number of interleaved channels
	uint32_t nSamplesPerSec = 0;  // Sampling rate (blocks per second)
	uint32_t nAvgBytesPerSec = 0; // Data rate
	uint16_t nBlockAlign = 0;	  // Data block size (bytes) all channels (4: 16bits stereo 6: 24 bits stereo)
	uint16_t wBitsPerSample = 0;  // Bits per sample (one channel 8 16 32 24 12)
	// ---- ext 1
	uint16_t cbSize = 0; // Size of the extension (0 or 22)
	// -- ext 2
	uint16_t wValidBitsPerSample = 0; // Number of valid bits
	uint32_t dwChannelMask = 0;		  // Speaker position mask
	unsigned char SubFormat[16];	  // GUID, including the data format code

	// fact
	uint32_t dwSampleLength = 0; // Number of samples (per channel)
};

// PCM structures

struct pcm_data
{
	fmt_base fmt;
	std::vector<char> data;
	uint32_t sample_size; // total size : number of sample in data
	int out_bits = 0;			  // out bits per sample : same as fmt.wBitsPerSample except for μ-law and a-law formats : fmt.wBitsPerSample = 8 and out_bits = 16
	uint16_t out_nBlockAlign = 0; // taille d'un sample en sortie (= nBlockAlign sauf pour Alaw et μ-law ou on a une entree 8bits et un sortie 16 bits => out_nBlockAlign = 2 x nBlockAlign)
};

/* test if the file seems to be a wave file */
bool test_wave(const std::string &file);
bool load_wave(const std::string &file, pcm_data &audio);

/*
 * wave format :  http://www-mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html
 * WAVE_FORMAT_PCM data is always signed EXCEPT when it is 8-bits per sample.
 * WAVE format RIFF : little endian (If the file starts with RIFF, then it's little endian. If it starts with FFIR or RIFX, then it's probably not.)
 * format
 * 0x0001 	WAVE_FORMAT_PCM 	PCM
 * 0x0003 	WAVE_FORMAT_IEEE_FLOAT 	IEEE float
 * 0x0006 	WAVE_FORMAT_ALAW 	8-bit ITU-T G.711 A-law
 * 0x0007 	WAVE_FORMAT_MULAW 	8-bit ITU-T G.711 µ-law
 * 0xFFFE 	WAVE_FORMAT_EXTENSIBLE 	Determined by SubFormat
 *
 */
enum class WAVE_FORMAT
{
	WAVE_FORMAT_PCM,
	WAVE_FORMAT_IEEE_FLOAT,
	WAVE_FORMAT_ALAW,
	WAVE_FORMAT_MULAW,
	WAVE_FORMAT_EXTENSIBLE,
	WAVE_FORMAT_UNKNOW,
};

std::ostream &operator<<(std::ostream &out, const WAVE_FORMAT &f);

WAVE_FORMAT get_wave_format(unsigned int code);

int16_t ALaw_Decode(int8_t number);

// 2.1. µ-Law Expanding (Decoding) Algorithm
int16_t MuLaw_Decode(int8_t number);

}


#endif /* WAVE_HPP_ */
