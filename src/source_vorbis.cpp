/**
 * @file source_vorbis.cpp
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
#include "source_vorbis.hpp"
#include "converters.hpp"
#include <cassert>
#include <vector>
#include <iostream>

namespace majimix 
{
// /*
//  * For fixed-point calculation
//  * FP_SHIFT should be between 10 and 32
//  */
// constexpr uint_fast64_t FP_SHIFT = 16;
// /*
//  * For fixed-point calculation
//  * Mask of FP_SHIFT
//  */
// constexpr uint_fast64_t FP_MASK = ((uint_fast64_t)1 << FP_SHIFT) - 1;

// Vorbis File Callbacks

static size_t ogg_read(void* buffer, size_t elementSize, size_t elementCount, void* dataSource) {
    assert(elementSize == 1);

    std::ifstream& stream = *static_cast<std::ifstream*>(dataSource);
    stream.read(static_cast<char*>(buffer), elementCount);
    const std::streamsize bytesRead = stream.gcount();
    stream.clear(); // In case we read past EOF
    return static_cast<size_t>(bytesRead);
}

static  int ogg_seek(void* dataSource, ogg_int64_t offset, int origin) {
    static const std::vector<std::ios_base::seekdir> seekDirections{
        std::ios_base::beg, std::ios_base::cur, std::ios_base::end
    };

    std::ifstream& stream = *static_cast<std::ifstream*>(dataSource);
    stream.seekg(offset, seekDirections.at(origin));
    stream.clear(); // In case we seeked to EOF
    return 0;
}

static long ogg_tell(void* dataSource) {
    std::ifstream& stream = *static_cast<std::ifstream*>(dataSource);
    const auto position = stream.tellg();
    assert(position >= 0);
    return static_cast<long>(position);
}

bool SourceVorbis::set_file(const std::string& filename)
{
    std::ifstream stream(filename, std::ios::binary);
	OggVorbis_File file;
	int result = ov_test_callbacks(&stream, &file, nullptr, 0, {ogg_read, ogg_seek, nullptr, ogg_tell});
	ov_clear(&file);
	if(!result)
	{
        this->filename = filename;
    }
    return !result;
}

void SourceVorbis::set_output_format(int samples_per_sec, int channels, int bits)
{
	mixer_rate = samples_per_sec;
	mixer_channels = channels;
	mixer_bits = bits;
	decoder = bits == 16 ? converters::in_to_i16_le<2> : converters::in_to_i24_le<2>;
}

std::unique_ptr<Sample> SourceVorbis::create_sample()
{
	// return std::make_unique<SampleVorbis>(*this);
	return std::make_unique<SampleVorbisF>(*this);
}

// SampleVorbis::SampleVorbis(const SourceVorbis &s)
// : source {&s},
//   sample_rate {0},
//   sample_size {0},
//   channels {0},
//   channel_size {0},
//   sample_step {0},
//   current_section{0},
//   last_section{-1},
//   sample_frac {0},
//   idx_1 {0},
//   idx_2 {0},
//   idx_lim {0}
// {
// 	stream.open(source->filename, std::ios::binary);
// 	if(stream)
// 	{
// 		int result = ov_open_callbacks(&stream, &file, nullptr, 0, {ogg_read, ogg_seek, nullptr, ogg_tell});
// 		if (result < 0) {
// #ifdef DEBUG
// 			std::cout << "Error opening file: " << result << std::endl;
// #endif
// 			return;
// 		}

// #ifdef DEBUG
// 		if(ov_seekable(&file))
// 		{
// 			std::cout << "Input bitstream contained " <<  ov_streams(&file) <<" logical bitstream section(s).\n";
// 			std::cout << "Total bitstream playing time: "<< (long)ov_time_total(&file,-1) <<" seconds\n\n";
// 		}
// 		else
// 		{
// 			std::cout << "Standard input was not seekable.\nFirst logical bitstream information:\n\n";
// 		}

// 		for(int i=0 ; i < ov_streams(&file); i++)
// 		{
// 			//     if multistream - rate and channels can change
// 			vorbis_info *vi = ov_info(&file,i);
// 			std::cout <<"\tlogical bitstream section " << (i+1) <<" information:\n";
// 			std::cout << "\t\t"<<vi->rate<<"Hz "<< vi->channels << " channels bitrate " << (ov_bitrate(&file,i)/1000) << "kbps serial number=" << ov_serialnumber(&file,i) <<"\n";
// 			std::cout << "\t\tcompressed length: "<<(long)(ov_raw_total(&file,i))<<" bytes " << " play time: " << (long)ov_time_total(&file,i) <<"s\n";
// 		}
// #endif
// 		initialized = true;
// 	}
// }

// SampleVorbis::~SampleVorbis()
// {
// #ifdef DEBUG
// 	std::cout << "~SampleVorbis()()\n";
// #endif
// 	ov_clear(&file);
// }

// void SampleVorbis::configure()
// {
// 	initialized = false;

// 	/* Read file info */
// 	vorbis_info* vorbisInfo = ov_info(&file, -1);
// 	sample_rate = vorbisInfo->rate;
// 	channels    = vorbisInfo->channels;
// 	channel_size = 2;
// 	sample_size = channel_size * channels;
// 	sample_step = (static_cast<uint_fast64_t>(sample_rate) << FP_SHIFT) /  source->mixer_rate;

// 	initialized = true;

// #ifdef DEBUG
// 	std::cout << "File info: " << vorbisInfo->rate << "Hz, " << vorbisInfo->channels << " channels\n";
// 	std::cout << "configure :\n";
// 	std::cout << "sampling_rate  : " << sample_rate << "\n";
// 	std::cout << "sample_size    : " << sample_size << "\n";
// 	std::cout << "channels       : " << channels << "\n";
// 	std::cout << "channel_size   : " << channel_size << "\n";
// 	std::cout << "mixer_rate     : " << source->mixer_rate << "\n";
// 	std::cout << "mixer_bits     : " << source->mixer_bits << "\n";
// 	std::cout << "mixer_channels : " << source->mixer_channels << "\n";
// 	std::cout << "sample_step : " << sample_step << "\n";
// #endif
// }


// int32_t SampleVorbis::read(int32_t* buffer, int32_t sample_count)
// {
// 	int out_sample_count = 0;
// 	unsigned int sample_add;
// 	bool done = false;
// 	int *out = buffer;

// 	while(!done)
// 	{
// 		// buffer loading test
// 		if(idx_lim >= buffer_read_length)
// 		{
// 			if(idx_1 >= buffer_read_length)
// 			{
// 				// 1 : no usable data in the current buffer
// 				idx_1   -= buffer_read_length;
// 				buffer_read_length = ov_read(&file, internal_buffer, internal_buffer_size, 0, 2, 1, &current_section);
// 			}
// 			else
// 			{
// 				// cas 2 : data are partially present in the buffer
// 				// data to take over (to preserve)
// 				int preserve_length = buffer_read_length - idx_1;
// 				std::copy(internal_buffer + idx_1, internal_buffer  + buffer_read_length, internal_buffer);
// 				idx_1    = 0;     

// 				// read the buffer
// 				buffer_read_length = ov_read(&file, internal_buffer + preserve_length, internal_buffer_size - preserve_length, 0, 2, 1, &current_section);
// 				buffer_read_length += preserve_length;
// 			}
// 			if(current_section != last_section)
// 			{
// 				configure();
// 				last_section = current_section;
// 			}
// 			idx_2 = idx_1 + sample_size;
// 			idx_lim = idx_2 + sample_size;
// 		}

// 		if(buffer_read_length > 0)
// 		{
// 			if(source->mixer_channels == 1)
// 			{
// 				// (channels) => mono
// 				int v_1 = 0, v_2 = 0, l;
// 				int cs = 0;
// 				const int shift = (channels>>1);
// 				for( int c = 0; c < channels; ++c)
// 				{
// 					v_1 += source->decoder(internal_buffer + idx_1 + cs);
// 					v_2 += source->decoder(internal_buffer + idx_2 + cs);
// 					cs += channel_size;
// 				}
// 				l = ((v_2 - v_1) * sample_frac >> FP_SHIFT) + v_1;
// 				*out++= (l>>shift);
// 			}
// 			else
// 			{
// 				// we assume a stereo output : surround, 5.1, 7.1  - not supported (yet)
// 				if(channels > 1)
// 				{
// 					// stereo => stereo
// 					int vl_1 = source->decoder(internal_buffer + idx_1 );
// 					int vr_1 = source->decoder(internal_buffer + idx_1 + channel_size);
// 					int vl_2 = source->decoder(internal_buffer + idx_2 );
// 					int vr_2 = source->decoder(internal_buffer + idx_2 + channel_size);

// 					int ll,lr;

// 					ll = ((vl_2 - vl_1) * sample_frac >> FP_SHIFT) + vl_1;
// 					lr = ((vr_2 - vr_1) * sample_frac >> FP_SHIFT) + vr_1;

// 					*out++=ll;
// 					*out++=lr;
// 				}
// 				else
// 				{
// 					// mono => stereo
// 					int v_1 = source->decoder(internal_buffer + idx_1 );
// 					int v_2 = source->decoder(internal_buffer + idx_2 );
// 					int l;
// 					l = ((v_2 - v_1) * sample_frac >> FP_SHIFT) + v_1;
// 					*out++=l;
// 					*out++=l;
// 				}
// 			}
// 			++out_sample_count;
// 			sample_frac += sample_step;
// 			sample_add = sample_frac >> FP_SHIFT;
// 			if(sample_add)
// 			{
// 				sample_frac &= FP_MASK;
// 				idx_1 += sample_add * sample_size;
// 				idx_2 = idx_1 + sample_size;
// 				idx_lim = idx_2 + sample_size;
// 			}

// 			if(out_sample_count ==  sample_count)
// 				done = true;
// 		}
// 		else
// 		{
// 			done = true;
// 			// EOF - AUTOLOOP
// 			seek(0);
// 		}
// 	}
// 	return out_sample_count;
// }

// void SampleVorbis::seek(long pos)
// {
// 	buffer_read_length = 0;
// 	idx_1 = 0;
// 	ov_pcm_seek(&file, pos);
// }
// void SampleVorbis::seek_time(double pos)
// {
// 	buffer_read_length = 0;
// 	idx_1 = 0;
// 	ov_time_seek(&file, pos);
// }

// double SampleVorbis::sample_time()
// {
// 	return ov_time_total(&file,-1);
// }






SampleVorbisF::SampleVorbisF(const SourceVorbis &s)
: source {&s},
  sample_rate {0},
  sample_size {0},
  channels {0},
  channel_size {0},
  sample_step {0},
  current_section{0},
  last_section{-1},
  min_buffer_data_size{0},
  max_buffer_idx{0},
  sample_pos{0.0},
  buffer_read_length{0}
{
	stream.open(source->filename, std::ios::binary);
	if(stream)
	{
		int result = ov_open_callbacks(&stream, &file, nullptr, 0, {ogg_read, ogg_seek, nullptr, ogg_tell});
		if (result < 0) {
#ifdef DEBUG
			std::cout << "Error opening file: " << result << std::endl;
#endif
			return;
		}

#ifdef DEBUG
		if(ov_seekable(&file))
		{
			std::cout << "Input bitstream contained " <<  ov_streams(&file) <<" logical bitstream section(s).\n";
			std::cout << "Total bitstream playing time: "<< (long)ov_time_total(&file,-1) <<" seconds\n\n";
		}
		else
		{
			std::cout << "Standard input was not seekable.\nFirst logical bitstream information:\n\n";
		}

		for(int i=0 ; i < ov_streams(&file); i++)
		{
			//     if multistream - rate and channels can change
			vorbis_info *vi = ov_info(&file,i);
			std::cout <<"\tlogical bitstream section " << (i+1) <<" information:\n";
			std::cout << "\t\t"<<vi->rate<<"Hz "<< vi->channels << " channels bitrate " << (ov_bitrate(&file,i)/1000) << "kbps serial number=" << ov_serialnumber(&file,i) <<"\n";
			std::cout << "\t\tcompressed length: "<<(long)(ov_raw_total(&file,i))<<" bytes " << " play time: " << (long)ov_time_total(&file,i) <<"s\n";
		}
#endif
		configure();
		// initialized = true;
	}
}

SampleVorbisF::~SampleVorbisF()
{
#ifdef DEBUG
	std::cout << "~SampleVorbis()()\n";
#endif
	ov_clear(&file);
}

void SampleVorbisF::configure()
{
	initialized = false;

	/* Read file info */
	vorbis_info* vorbisInfo = ov_info(&file, -1);
	sample_rate = vorbisInfo->rate;
	channels    = vorbisInfo->channels;
	channel_size = 2;
	sample_size = channel_size * channels;
	// sample_step = (static_cast<uint_fast64_t>(sample_rate) << FP_SHIFT) /  source->mixer_rate;
	sample_step = static_cast<double>(sample_rate) / source->mixer_rate;
	min_buffer_data_size = sample_size * 2;
	max_buffer_idx = -1;

	initialized = true;

#ifdef DEBUG
	std::cout << "File info: " << vorbisInfo->rate << "Hz, " << vorbisInfo->channels << " channels\n";
	std::cout << "configure :\n";
	std::cout << "sampling_rate  : " << sample_rate << "\n";
	std::cout << "sample_size    : " << sample_size << "\n";
	std::cout << "channels       : " << channels << "\n";
	std::cout << "channel_size   : " << channel_size << "\n";
	std::cout << "mixer_rate     : " << source->mixer_rate << "\n";
	std::cout << "mixer_bits     : " << source->mixer_bits << "\n";
	std::cout << "mixer_channels : " << source->mixer_channels << "\n";
	std::cout << "sample_step : " << sample_step << "\n";
#endif
}

int32_t SampleVorbisF::read(int32_t *out, int32_t sample_count)
{
	int32_t out_sample_count = 0;
	bool done = false;

	while(!done)
	{
		auto sample_idx = static_cast<int32_t>(sample_pos);
		double alpha = sample_pos - sample_idx;
		int32_t buffer_idx = sample_idx * sample_size;

		if(buffer_idx > max_buffer_idx)
		{
			int32_t buffer_remaining = buffer_read_length - buffer_idx;

			if(buffer_remaining > 0)
				std::copy(internal_buffer + buffer_idx, internal_buffer  + buffer_read_length, internal_buffer);
			int32_t r = std::max(buffer_remaining, 0);
			long read_val = ov_read(&file, internal_buffer + r, internal_buffer_size - r, 0, 2, 1, &current_section);
			if(read_val == 0)
			{
				// EOF
				done = true;
				seek(0); // auto loop
			}
			else if(read_val > 0)
			{
				if (current_section != last_section)
				{
					configure();
					last_section = current_section;
				}
				int32_t obr = buffer_read_length - r;
				buffer_read_length  = read_val + r;	
				max_buffer_idx = buffer_read_length - min_buffer_data_size;

				buffer_idx -= obr;
				sample_idx = buffer_idx / sample_size;
				sample_pos = sample_idx + alpha;				
			}
		}

		if(buffer_idx <= max_buffer_idx)
		{
			if(source->mixer_channels == 1)
			{
				// (channels) => mono
				int32_t ina = 0, inb = 0;
				char *b =  internal_buffer + buffer_idx;
				for( int c = 0; c < channels; ++c)
				{
					ina += source->decoder(b);
					inb += source->decoder(b + sample_size);
					b += channel_size;
				}
				*out++ = static_cast<int32_t>((ina + alpha * (inb - ina)) / channels);
			}
			else
			{
				// we assume a stereo output : surround, 5.1, 7.1  - not supported (yet)
				if(channels > 1)
				{
					// stereo => stereo
					char *b =  internal_buffer + buffer_idx;

					auto la = source->decoder(b);
					auto ra = source->decoder(b + channel_size);
					auto lb = source->decoder(b + sample_size);
					auto rb = source->decoder(b + sample_size + channel_size);

					*out++ = static_cast<int32_t>(la + alpha * (lb - la));
					*out++ = static_cast<int32_t>(ra + alpha * (rb - ra));
				}
				else
				{
					// mono => stereo
					auto ina = source->decoder(internal_buffer + buffer_idx );
					auto inb = source->decoder(internal_buffer + buffer_idx + sample_size );
					auto l = static_cast<int32_t>(ina + alpha * (inb - ina));
					*out++ = l;
					*out++=l;
				}
			}
			++out_sample_count;
			done = sample_count == out_sample_count;
			sample_pos += sample_step;
		}
	}
	return out_sample_count;
}

void SampleVorbisF::seek(long pos)
{
	max_buffer_idx = -1;
	buffer_read_length = 0;
	sample_pos = 0;
	//configure();
	ov_pcm_seek(&file, pos);
}
void SampleVorbisF::seek_time(double pos)
{
	max_buffer_idx = -1;
	buffer_read_length = 0;
	sample_pos = 0;
	//configure();
	ov_time_seek(&file, pos);
}

double SampleVorbisF::sample_time()
{
	return ov_time_total(&file,-1);
}
}