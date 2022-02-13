/*
 * wave.cpp
 *
 * @author  François Jacobs
 * @date 13/02/2022
 * @version 
 *
 * @section majimix_lic_cpp LICENSE
 *
 * The MIT License (MIT)
 * 
 * Copyright © 2022  - François Jacobs
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
 *
 */

#include "wave.hpp"

#include <fstream>



namespace majimix {
namespace wave {
const int num_one = 1;
const bool little_endian = (*(char *)&num_one) == 1;

//uint32_t reverse_nibbles(uint32_t x)
//{
//	uint32_t out = 0, i;
//	for(i = 0; i < 4; ++i)
//	{
//		const uint32_t byte = (x >> 8 * i) & 0xff;
//		out |= byte << (24 - 8 * i);
//	}
//	return out;
//}

template<typename T>
T reverse_nibbles(T x)
{
	T out = 0;
	unsigned int i;
	constexpr int mx = (sizeof(T) - 1) * 8;
	for(i = 0; i < sizeof(T) ; ++i)
	{
		const T byte = (x >> 8 * i) & 0xff;
		out |= byte << (mx - 8 * i);
	}
	return out;
}


static bool read_chunck(std::ifstream &is, std::string& chunck, uint32_t &chunck_size)
{
	chunck.assign(4, '\0');
	if(is.read(&chunck[0], 4))
	{
		if(is.read(reinterpret_cast<char*>(&chunck_size), 4))
		{
			if(!little_endian)
				chunck_size = reverse_nibbles(chunck_size);
			return true;
		}

	}
	return false;
}

static bool read_fmt(std::ifstream &is, uint32_t chunck_size, fmt_base &fmt)
{
	if(chunck_size >= 16)
	{
		if(is.read(reinterpret_cast<char*>(&fmt.wFormatTag), sizeof fmt.wFormatTag)
				&& is.read(reinterpret_cast<char*>(&fmt.nChannels), sizeof fmt.nChannels)
				&& is.read(reinterpret_cast<char*>(&fmt.nSamplesPerSec), sizeof fmt.nSamplesPerSec)
				&& is.read(reinterpret_cast<char*>(&fmt.nAvgBytesPerSec), sizeof fmt.nAvgBytesPerSec)
				&& is.read(reinterpret_cast<char*>(&fmt.nBlockAlign), sizeof fmt.nBlockAlign)
				&& is.read(reinterpret_cast<char*>(&fmt.wBitsPerSample), sizeof fmt.wBitsPerSample))
		{
			fmt.cbSize = 0;
			fmt.wValidBitsPerSample =0;
			fmt.dwChannelMask = 0;
			fmt.SubFormat[0] = '\0';
			if(!little_endian)
			{
				fmt.wFormatTag = reverse_nibbles(fmt.wFormatTag);
				fmt.nChannels = reverse_nibbles(fmt.nChannels);
				fmt.nSamplesPerSec = reverse_nibbles(fmt.nSamplesPerSec);
				fmt.nAvgBytesPerSec = reverse_nibbles(fmt.nAvgBytesPerSec);
				fmt.nBlockAlign = reverse_nibbles(fmt.nBlockAlign);
				fmt.wBitsPerSample = reverse_nibbles(fmt.wBitsPerSample);
			}

			if(chunck_size == 16)
				return true;

			if(is.read(reinterpret_cast<char*>(&fmt.cbSize), sizeof fmt.cbSize))
			{
				if(!little_endian)
					fmt.cbSize = reverse_nibbles(fmt.cbSize);

				if(fmt.cbSize==0)
					return true;
				if(fmt.cbSize==22)
				{
					if(is.read(reinterpret_cast<char*>(&fmt.wValidBitsPerSample), sizeof fmt.wValidBitsPerSample)
					&& is.read(reinterpret_cast<char*>(&fmt.dwChannelMask), sizeof fmt.dwChannelMask)
					&& is.read(reinterpret_cast<char*>(&fmt.SubFormat), sizeof fmt.SubFormat))
					{
						if(!little_endian)
						{
							fmt.wValidBitsPerSample = reverse_nibbles(fmt.wValidBitsPerSample);
							fmt.dwChannelMask = reverse_nibbles(fmt.dwChannelMask);
						}
						return true;
					}
				}
			}
		}
	}
	return false;
}


static bool read_RIFF(std::ifstream &is, pcm_data& audio)
{
	std::string chunck;
	uint32_t chunck_size;
	bool err = false;
	if(read_chunck(is, chunck, chunck_size))
	{
		if(chunck == "RIFF" && chunck_size >4)
		{
			//std::cout << " cur pos " << is.tellg() << " - sz "<<chunck_size<< std::endl;
			if(is.read(&chunck[0], 4) && chunck == "WAVE")
			{
				uint32_t position_max = 8 + chunck_size;
				bool done = false;
				bool data_loaded = false;
				bool fmt_loaded = false;
				while(!done && (position_max-is.tellg()) >8 && !err)
				{
					if(read_chunck(is, chunck, chunck_size))
					{
						if(chunck == "fmt ")
						{
							if(read_fmt(is, chunck_size, audio.fmt))
								fmt_loaded = true;
							else
								err = true;
						}
						else if(chunck == "fact")
						{
							// doit être de taille 4
							// dwSampleLength 	4 	Number of samples (per channel)
							//std::cout << "read fact" << std::endl;
							if(!is.read(reinterpret_cast<char*>(&audio.fmt.dwSampleLength), sizeof audio.fmt.dwSampleLength))
								break;
							if(!little_endian)
								audio.fmt.dwSampleLength = reverse_nibbles(audio.fmt.dwSampleLength);

						}
						else if(chunck == "data")
						{
							// read data
							audio.data.assign(chunck_size, 0);
							if(!is.read(&audio.data[0], chunck_size))
								err = true;


							if(!err)
							{
								data_loaded = true;
								// padding byte if odd
								if(chunck_size%2)
									is.get();
							}
						}
						else
						{
							// std::cout << "skip chunck " << chunck << " sz : " << chunck_size << std::endl;
							is.seekg(chunck_size, std::ios_base::cur);
						}
					}
					else
					{
						err = true;
					}
				}

				if(!data_loaded || !fmt_loaded)
					err = true;

				if(err)
				{
					if(data_loaded && fmt_loaded)
						std::cerr << "error while reading a chunk [data and format are loaded]" << std::endl;
					else if(fmt_loaded)
						std::cerr << "error while reading data [data not loaded]" << std::endl;
					else if(fmt_loaded)
						std::cerr << "error while reading data [format not loaded]" << std::endl;
					else
						std::cerr << "error while reading data [format and data not loaded]" << std::endl;
				}
			}
			else
			{
				// not a wave file
				err = true;
				std::cerr << "\"WAVE\" not found after RIFF chunck" << std::endl;
			}
		}
		else
		{
			err = true;
			if(chunck_size<=4)
				std::cerr << "invalid RIFF chunk size : " << chunck_size << " (too small)" << std::endl;
			else
				std::cerr << "invalid chunk : " << chunck << " (RIFF expected)" << std::endl;
		}
	}
	else
	{
		err = true;
		std::cerr << "can't read first RIFF chunk" << std::endl;
	}
	return !err;
}

/* test if the file seems to be a wave file */
bool test_wave(const std::string &file)
{
	bool done = false;
	std::ifstream wstream(file, std::ios::binary);
	if (wstream)
	{
		// needed to keep white spaces
		wstream.unsetf(std::ios_base::skipws);

		// ensure ifstream objects can throw exceptions:
		wstream.exceptions (std::ifstream::failbit | std::ifstream::badbit);

		std::string chunck;
		uint32_t chunck_size;
		if(read_chunck(wstream, chunck, chunck_size))
			if(chunck == "RIFF" && chunck_size >4)
				done = wstream.read(&chunck[0], 4) && chunck == "WAVE";
	}
	return done;
}


bool load_wave(const std::string &file, pcm_data& audio)
{
	//std::cout << "loading "<<file << std::endl;
	bool done = false;
	std::ifstream wstream(file, std::ios::binary);
	if (wstream)
	{
		// needed to keep white spaces
		wstream.unsetf(std::ios_base::skipws);

		// ensure ifstream objects can throw exceptions:
		wstream.exceptions (std::ifstream::failbit | std::ifstream::badbit);

		if(read_RIFF(wstream, audio))
		{
			audio.sample_size = audio.data.size() / audio.fmt.nBlockAlign;
			done = true;
		}
	}
	return done;
}


std::ostream& operator<<(std::ostream& out, const WAVE_FORMAT& f)
{
	 return out << (f == WAVE_FORMAT::WAVE_FORMAT_PCM ? 		"WAVE_FORMAT_PCM" :
					f == WAVE_FORMAT::WAVE_FORMAT_IEEE_FLOAT ?  "WAVE_FORMAT_IEEE_FLOAT" :
					f == WAVE_FORMAT::WAVE_FORMAT_ALAW ? 		"WAVE_FORMAT_ALAW" :
					f == WAVE_FORMAT::WAVE_FORMAT_MULAW ? 		"WAVE_FORMAT_MULAW" :
					f == WAVE_FORMAT::WAVE_FORMAT_EXTENSIBLE ?  "WAVE_FORMAT_EXTENSIBLE" :
																"WAVE_FORMAT_UNKNOW");
}

WAVE_FORMAT get_wave_format(unsigned int code)
{
	switch(code)
	{
	case 0x0001 : return WAVE_FORMAT::WAVE_FORMAT_PCM;
	case 0x0003 : return WAVE_FORMAT::WAVE_FORMAT_IEEE_FLOAT;
	case 0x0006 : return WAVE_FORMAT::WAVE_FORMAT_ALAW;
	case 0x0007 : return WAVE_FORMAT::WAVE_FORMAT_MULAW;
	case 0xFFFE : return WAVE_FORMAT::WAVE_FORMAT_EXTENSIBLE;
	default : return WAVE_FORMAT::WAVE_FORMAT_UNKNOW;
	}
}


int16_t ALaw_Decode(int8_t number)
{
	uint8_t sign = 0x00;
	uint8_t position = 0;
	int16_t decoded = 0;
	number^=0x55;
	if(number&0x80)
	{
		number&=~(1<<7);
		sign = -1;
	}
	position = ((number & 0xF0) >>4) + 4;
	if(position!=4)
	{
		decoded = ((1<<position)|((number&0x0F)<<(position-4))
				|(1<<(position-5)));
	}
	else
	{
		decoded = (number<<1)|1;
	}
	// FJ : add << 4 (volume too low) (12-bit magnitude data is used in A-law)
	return ((sign==0)?(decoded):(-decoded)) << 4;
}


int16_t MuLaw_Decode(int8_t number)
{
	constexpr uint16_t MULAW_BIAS = 33;
	uint8_t sign = 0, position = 0;
	int16_t decoded = 0;
	number = ~number;
	if (number & 0x80)
	{
		number &= ~(1 << 7);
		sign = -1;
	}
	position = ((number & 0xF0) >> 4) + 5;
	decoded = ((1 << position) | ((number & 0x0F) << (position - 4))
			| (1 << (position - 5))) - MULAW_BIAS;
	// FJ : add << 3 (volume too low) (13-bit magnitude data)
	return ((sign == 0) ? (decoded) : (-(decoded))) << 3;
}


} // namespace wave
} // namespace majimix




