/**
 * @file source_pcm.cpp
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
#include "source_pcm.hpp"
#include "wave.hpp"
#include "converters.hpp"

namespace majimix {

void SourcePCM::set_output_format(int samples_per_sec, int channels, int bits)
{
	ready = false;
	mixer_rate = samples_per_sec;
	mixer_channels = channels;
	mixer_bits = bits;
	configure();
}

bool SourcePCM::load_wave(const std::string &filename)
{
	bool done = false;

	/* reset previous format */
	format = AuFormat::none;
	ready = false;
	pcm.clear();
	data_size = 0;
	decoder = nullptr;

	wave::pcm_data pcm_data;

	if(wave::load_wave(filename, pcm_data))
	{
		wave::fmt_base &fmt = pcm_data.fmt;

		sample_rate         = fmt.nSamplesPerSec;
		sample_size         = fmt.nBlockAlign;
		channels            = fmt.nChannels;
		channel_size        = fmt.nBlockAlign / fmt.nChannels;

		data_size           = pcm_data.data.size();
		size                = pcm_data.data.size() / fmt.nBlockAlign;
		pcm                 = std::move(pcm_data.data);


		wave::WAVE_FORMAT wformat = wave::get_wave_format(fmt.wFormatTag);
		uint16_t wFormatTag_ex = 0;
		if(wformat == wave::WAVE_FORMAT::WAVE_FORMAT_EXTENSIBLE)
		{
			// FORMAT EXTENSIBLE : search format in ex part
			if(fmt.cbSize)
			{
				wFormatTag_ex = reinterpret_cast<uint16_t&>(fmt.SubFormat);
				wformat = wave::get_wave_format(wFormatTag_ex);
			}
		}
#ifdef DEBUG
		bool format_ex = fmt.cbSize && wformat == wave::WAVE_FORMAT::WAVE_FORMAT_EXTENSIBLE;
		std::cout << "format          " << wformat << "\n";
		std::cout << "channels        " << fmt.nChannels << "\n";
		std::cout << "rate            " << fmt.nSamplesPerSec << "\n";
		std::cout << "bits per sample (one channel) " << fmt.wBitsPerSample << "\n";
		std::cout << "bytes for one sample and all channels " << fmt.nBlockAlign << "\n";

		// review this check: case of 12bits which are aligned on 16bits (and treated as 16 bits)
		std::cout << "format " << wformat << (format_ex ? " (EX)": "")<< " bps " << fmt.wBitsPerSample << " bits, " << fmt.nChannels << " channel(s) : nAvgBytesPerSec " << fmt.nAvgBytesPerSec<< " : nBlockAlign " << fmt.nBlockAlign << std::endl;
		if(fmt.nAvgBytesPerSec != (fmt.nChannels * (fmt.wBitsPerSample>>3)) * fmt.nSamplesPerSec)
		{
			std::cerr << "format " << wformat << " bps " << fmt.wBitsPerSample << " bits, " << fmt.nChannels << " channel(s)" << " : nAvgBytesPerSec " << fmt.nAvgBytesPerSec << " mais il devrait être de " << ((fmt.nChannels * (fmt.wBitsPerSample>>3)) * fmt.nSamplesPerSec) << std::endl;
		}
		if(fmt.nBlockAlign * 8 != fmt.wBitsPerSample * fmt.nChannels)
		{
			std::cerr << "format " << wformat << " bps " << fmt.wBitsPerSample <<" bits, " << fmt.nChannels << " channel(s)" << " : nBlockAlign " << fmt.nBlockAlign << " => nBlockAlign * 8 ("<< (fmt.nBlockAlign * 8) <<")= wBitsPerSample * nChannels (" << (fmt.wBitsPerSample * fmt.nChannels) << ")"<< std::endl;
		}
#endif

		switch(wformat)
		{
		case wave::WAVE_FORMAT::WAVE_FORMAT_ALAW:
			format = AuFormat::alaw;
			break;
		case wave::WAVE_FORMAT::WAVE_FORMAT_MULAW:
			format = AuFormat::ulaw;
			break;
		case wave::WAVE_FORMAT::WAVE_FORMAT_PCM :
		{
			switch(fmt.wBitsPerSample)
			{
			case 8 :
				format = AuFormat::uint_8bits;
				break;
			case 12 : // ??? : 12 bits - first byte (less significant) the first 4 bits are 0 => we have to load int16 then (>>4)
			case 16 :
				format = AuFormat::int_16bits;
				break;
			case 24 :
				format = AuFormat::int_24bits;
				break;
			case 32 :
				format = AuFormat::int_32bits;
				break;
			default:
#ifdef DEBUG
				std::cerr << "format WAVE_FORMAT_PCM avec wBitsPerSample = " << fmt.wBitsPerSample << " not implemented" << std::endl;
#endif
				break;
			}
			break;
		}
		case wave::WAVE_FORMAT::WAVE_FORMAT_IEEE_FLOAT : {
			switch(fmt.wBitsPerSample)
			{
			case 32 :
				format = AuFormat::float_32bits;
				break;
			case 64 :
				format = AuFormat::float_64bits;
				break;
			default:
#ifdef DEBUG
				std::cerr << "Format WAVE_FORMAT_IEEE_FLOAT "<< fmt.wBitsPerSample << " bits not supported" << std::endl;
#endif
				break;
			}
			break;
		}

		//  WAVE_FORMAT_EXTENSIBLE format should be used whenever:
		//
		//  PCM data has more than 16 bits/sample.
		//  The number of channels is more than 2.
		//  The actual number of bits/sample is not equal to the container size.
		//  The mapping from channels to speakers needs to be specified.

		default:
#ifdef DEBUG
			std::cerr << "unsupported format WAVE_FORMAT_EXTENSIBLE" << std::endl;
#endif
			break;
		}
		done = format != AuFormat::none;
		if(done)
			configure();
	}
#ifdef DEBUG
	else
	{
		std::cerr << "No file found\n";
	}
#endif
	return done;
}

void SourcePCM::configure()
{
	ready = false;
	/* source format & data */
	if(sample_rate   > 0 &&
	   sample_size     > 0 &&
	   channels        > 0 &&
	   channel_size    > 0 &&
	   data_size       > 0 &&
	   size            > 0 &&
	   pcm.size()     == static_cast<size_t>(data_size)&&
	   mixer_rate      > 0 &&
	   ((mixer_bits == 16) | (mixer_bits == 24)) &&
	   mixer_channels  > 0)
	{
#ifdef DEBUG
		std::cout << "configure :\n";
		std::cout << "sampling_rate  : " << sample_rate << "\n";
		std::cout << "sample_size    : " << sample_size << "\n";
		std::cout << "channels       : " << channels << "\n";
		std::cout << "channel_size   : " << channel_size << "\n";
		std::cout << "data_size      : " << data_size << "\n";
		std::cout << "size           : " << size << "\n";
		std::cout << "mixer_rate     : " << mixer_rate << "\n";
		std::cout << "mixer_bits     : " << mixer_bits << "\n";
		std::cout << "mixer_channels : " << mixer_channels << "\n";
#endif

		/* decoder */
		switch(format)
		{
		case AuFormat::alaw:
			decoder = mixer_bits == 16 ? converters::alaw : converters::alaw_i24;
		break;
		case AuFormat::ulaw:
			decoder = mixer_bits == 16 ? converters::ulaw : converters::ulaw_i24;
		break;
		case AuFormat::uint_8bits:
			decoder = mixer_bits == 16 ? converters::ui8_to_i16 : converters::ui8_to_i24;
		break;
		case AuFormat::int_16bits:
			decoder = mixer_bits == 16 ? converters::in_to_i16_le<2> : converters::in_to_i24_le<2>;
		break;
		case AuFormat::int_24bits:
			decoder = mixer_bits == 16 ? converters::in_to_i16_le<3> : converters::in_to_i24_le<3>;
		break;
		case AuFormat::int_32bits:
			decoder = mixer_bits == 16 ? converters::in_to_i16_le<4> : converters::in_to_i24_le<4>;
		break;
		case AuFormat::float_32bits:
			decoder = mixer_bits == 16 ? converters::float_to_i16<float> : converters::float_to_i24<float>;
		break;
		case AuFormat::float_64bits:
			decoder = mixer_bits == 16 ? converters::float_to_i16<double> : converters::float_to_i24<double>;
		break;
		default:
			return;
		}
		ready = true;
	}
}


// /*
//  * For fixed-point calculation
//  * FP_SHIFT should be between 10 and 32
//  */
// constexpr uint_fast64_t FP_SHIFT = 31;
// /*
//  * For fixed-point calculation
//  * Mask of FP_SHIFT
//  */
// constexpr uint_fast64_t FP_MASK = ((uint_fast64_t)1 << FP_SHIFT) - 1;


// void SourcePCMI::configure()
// {
// 	SourcePCM::configure();
// 	if(ready)
// 	{
// 		sample_step = (static_cast<uint_fast64_t>(sample_rate) << FP_SHIFT) /  mixer_rate;

// 		/* read function */
// 		if(mixer_channels == 1)
// 		{
// 			if(channels > 1)
// 				read_fn = std::bind(&SourcePCMI::read<true, false>, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
// 			else
// 				read_fn = std::bind(&SourcePCMI::read<false, false>, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
// 		}
// 		else if(mixer_channels == 2)
// 		{
// 			if(channels > 1)
// 				read_fn = std::bind(&SourcePCMI::read<true, true>, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
// 			else
// 				read_fn = std::bind(&SourcePCMI::read<false, true>, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
// 		}
// 	}
// }

// std::unique_ptr<Sample> SourcePCMI::create_sample()
// {
// 	if(ready)
// 		return std::make_unique<SamplePCMI>(*this);
// 	return nullptr;
// }

// template<bool STEREO_INPUT, bool STEREO_OUTPUT>
// int32_t SourcePCMI::read(int32_t* out_buffer, int32_t sample_count, int32_t &sample_idx, uint_fast64_t &sample_frac)
// {
// 	unsigned int sample_add;
// 	int32_t out_sample_count = 0;
// 	if(sample_idx < size)
// 	{
// 		uint32_t idx = sample_idx * sample_size;
// 		char *data = pcm.data();
// 		int32_t *out = out_buffer;

// 		if constexpr (STEREO_INPUT && STEREO_OUTPUT)
// 		{
// 			// stereo -> stereo
// 			int32_t vl = decoder(data + idx);
// 			int32_t vr = decoder(data + idx + channel_size);
// 			idx = (idx + sample_size) % data_size;
// 			int32_t vl2 = decoder(data + idx);
// 			int32_t vr2 = decoder(data + idx + channel_size);
// 			int32_t ll,lr;

// 			while(out_sample_count < sample_count)
// 			{
// 				// lerp
// 				ll = ((vl2 - vl) * sample_frac >> FP_SHIFT) + vl;
// 				lr = ((vr2 - vr) * sample_frac >> FP_SHIFT) + vr;
// 				*out++ = ll;
// 				*out++ = lr;
// 				++out_sample_count;

// 				sample_frac += sample_step;
// 				sample_add = sample_frac >> FP_SHIFT;
// 				if(sample_add)
// 				{
// 					sample_idx  += sample_add;
// 					sample_frac &= FP_MASK;
// 					if(sample_idx >= size)
// 						break;
// 					idx = (sample_idx  * sample_size)/* % data_size*/;
// 					vl = decoder(data + idx);
// 					vr = decoder(data + idx + channel_size);
// 					idx = (idx + sample_size) % data_size;
// 					vl2 = decoder(data + idx);
// 					vr2 = decoder(data + idx + channel_size);
// 				}
// 			}
// 		}
// 		else if constexpr (STEREO_OUTPUT)
// 		{
// 			// mono -> stereo
// 			int32_t l;
// 			int32_t v = decoder(data + idx);
// 			idx = (idx + sample_size)%data_size;
// 			int32_t w = decoder(data + idx);

// 			//do
// 			while(out_sample_count < sample_count)
// 			{
// 				l = ((w - v) * sample_frac >> FP_SHIFT) + v;
// 				*out++=l;
// 				*out++=l;
// 				++out_sample_count;

// 				sample_frac += sample_step;
// 				sample_add = sample_frac >> FP_SHIFT;
// 				if(sample_add)
// 				{
// 					sample_idx  += sample_add;
// 					sample_frac &= FP_MASK;
// 					if(sample_idx >= size)
// 						break;

// 					idx = (sample_idx  * sample_size);
// 					v = decoder(data + idx);
// 					idx = (idx + sample_size) % data_size;
// 					w = decoder(data + idx);
// 				}
// 			}
// 		}
// 		else
// 		{
// 			// mono/stereo -> mono
// 			int32_t v = 0, w = 0, l;
// 			// const int shift = (channels>>1);
//             constexpr int shift = STEREO_INPUT ? 2 : 1;
// 			uint32_t idx2 = (idx + sample_size)%data_size;
// 			for( int c = 0; c < channels; ++c)
// 			{
// 				v += decoder(data + idx);
// 				w += decoder(data + idx2);
// 				idx += channel_size;
// 				idx2 += channel_size;
// 			}

// 			while(out_sample_count < sample_count)
// 			{
// 				l = ((w - v) * sample_frac >> FP_SHIFT) + v;
// 				*out++= (l>>shift);
// 				++out_sample_count;

// 				sample_frac += sample_step;
// 				sample_add = sample_frac >> FP_SHIFT;
// 				if(sample_add)
// 				{
// 					sample_idx  += sample_add;
// 					sample_frac &= FP_MASK;
// 					if(sample_idx >= size)
// 						break;

// 					idx = (sample_idx * sample_size);
// 					idx2 = (idx + sample_size) % data_size;
// 					v = 0;
// 					w = 0;
// 					for( int c = 0; c < channels; ++c)
// 					{
// 						v += decoder(data + idx);
// 						w += decoder(data + idx2);
// 						idx += channel_size;
// 						idx2 += channel_size;
// 					}
// 				}
// 			}
// 		}
// 	}
// 	return out_sample_count;
// }

// SamplePCMI::SamplePCMI(const SourcePCMI &s) : source {&s}
// {}


// int32_t SamplePCMI::read(int32_t* buffer, int32_t sample_count)
// {
// 	int32_t r = source->read_fn(buffer, sample_count, sample_idx, sample_frac);
// 	if(r < sample_count)
// 	{
// 		// EOF - AUTOLOOP
// 		sample_idx  = 0;
// 		sample_frac = 0;
// 	}
// 	return r;
// }

// void SamplePCMI::seek(long pos)
// {
// 	if(source && pos < source->size && pos >= 0)
// 	{
// 		sample_idx  = pos;
// 		sample_frac = 0;
// 	}
// }
// void SamplePCMI::seek_time(double pos)
// {
// 	if(source && pos >= 0 && pos < sample_time())
// 		sample_idx = source->sample_rate * pos;
// }

// double SamplePCMI::sample_time() const
// {
// 	return  source && source->sample_rate ? static_cast<double>(source->size) / source->sample_rate : 0.;
// }


void SourcePCMF::configure()
{
	SourcePCM::configure();
	if(ready)
	{
		sample_step = static_cast<double>(sample_rate) / mixer_rate;

		/* read function */
		if(mixer_channels == 1)
		{
			if(channels > 1)
				read_fn = std::bind(&SourcePCMF::read<true, false>, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
			else
				read_fn = std::bind(&SourcePCMF::read<false, false>, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
		}
		else if(mixer_channels == 2)
		{
			if(channels > 1)
				read_fn = std::bind(&SourcePCMF::read<true, true>, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
			else
				read_fn = std::bind(&SourcePCMF::read<false, true>, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
		}
	}
}

std::unique_ptr<Sample> SourcePCMF::create_sample()
{
	if(ready)
		return std::make_unique<SamplePCMF>(*this);
	return nullptr;
}


template<bool STEREO_INPUT, bool STEREO_OUTPUT>
int32_t SourcePCMF::read(int32_t* out_buffer, int32_t sample_count, double &sample_idx) const
{
	int32_t out_sample_count = 0;
	if (sample_idx < size)
	{
		
		const char *data = pcm.data();
		int32_t *out = out_buffer;

		// auto idx_start = sample_idx * sample_size;
		// !!! il faut raisonner en sample auto step = sample_step_d * sample_size;

		int32_t max_sample_remaining = static_cast<int32_t>((size - sample_idx - 1) / sample_step);
		sample_count = std::min(sample_count, max_sample_remaining);
		int32_t idx;
		double idx_d;
		double alpha;

		if constexpr (STEREO_INPUT && STEREO_OUTPUT)
		{
			// stereo -> stereo
			int32_t cl1, cr1, cl2, cr2, cl, cr;

			for(int sample_number = 0; sample_number < sample_count; ++sample_number)
			{
				idx_d = sample_idx + sample_number * sample_step;
				idx = static_cast<int32_t>(idx_d);
				alpha = idx_d - idx;

				idx *= sample_size; 

				cl1 = decoder(data + idx);
				cr1 = decoder(data + idx + channel_size);

				// max_remaining : idx = (idx + sample_size) % data_size;
				idx += sample_size;

				cl2 = decoder(data + idx);
				cr2 = decoder(data + idx + channel_size);

				// imprecise
				cl = cl1 + alpha * (cl2 - cl1);
				cr = cr1 + alpha * (cr2 - cr1);

				// precise
				// cl = (1. - alpha) * cl1 + alpha * cl2;
				// cr = (1. - alpha) * cr1 + alpha * cr2;

				*out++ = cl;
				*out++ = cr;
				// ++out_sample_count;
			}
			out_sample_count = sample_count;
			sample_idx +=  sample_count * sample_step;
		}
		else if constexpr (STEREO_OUTPUT)
		{
			// mono -> stereo
			int32_t input_val1, input_val2, output_val;
			for(int sample_number = 0; sample_number < sample_count; ++sample_number)
			{
				idx_d = sample_idx + sample_number * sample_step;
				idx = static_cast<int32_t>(idx_d);
				alpha = idx_d - idx;

				idx *= sample_size; 

				input_val1 = decoder(data + idx);
				idx += sample_size;
				input_val2 = decoder(data + idx);

				output_val = input_val1 + alpha * (input_val2 - input_val1);
				*out++ = output_val;
				*out++ = output_val;
			}
			out_sample_count = sample_count;
			sample_idx +=  sample_count * sample_step;

		}
		else if constexpr (STEREO_INPUT)
		{
			// stereo -> mono
			int32_t input_l1, input_l2, input_r1, input_r2, output_val;
			for(int sample_number = 0; sample_number < sample_count; ++sample_number)
			{
				idx_d = sample_idx + sample_number * sample_step;
				idx = static_cast<int32_t>(idx_d);
				alpha = idx_d - idx;

				idx *= sample_size; 

				input_l1 = decoder(data + idx);
				input_r1 = decoder(data + idx + channel_size);

				idx += sample_size;

				input_l2 = decoder(data + idx);
				input_r2 = decoder(data + idx + channel_size);

				output_val = (input_l1 + input_r1 + alpha * (input_l2 - input_l1 + input_r2 - input_r1)) * 0.5;
				*out++ = output_val;
			}
			out_sample_count = sample_count;
			sample_idx +=  sample_count * sample_step;
		}
		else
		{
			// mono -> mono
			int32_t input_v1, input_v2, output_val;
			for(int sample_number = 0; sample_number < sample_count; ++sample_number)
			{
				idx_d = sample_idx + sample_number * sample_step;
				idx = static_cast<int32_t>(idx_d);
				alpha = idx_d - idx;

				idx *= sample_size; 

				input_v1 = decoder(data + idx);
				idx += sample_size;
				input_v2 = decoder(data + idx);

				output_val = input_v1 + alpha * (input_v2 - input_v1);
				*out++ = output_val;
			}
			out_sample_count = sample_count;
			sample_idx +=  sample_count * sample_step;
		}



	}
	return out_sample_count;
}

SamplePCMF::SamplePCMF(const SourcePCMF &s) : source {&s}
{}


int32_t SamplePCMF::read(int32_t* buffer, int32_t sample_count)
{
	int32_t r = source->read_fn(buffer, sample_count, sample_idx);
	if(r < sample_count)
	{
		// EOF - AUTOLOOP
		sample_idx  = 0;
	}
	return r;
}

void SamplePCMF::seek(long pos)
{
	if(source && pos < source->size && pos >= 0)
	{
		sample_idx  = static_cast<double>(pos);
	}
}
void SamplePCMF::seek_time(double pos)
{
	if(source && pos >= 0 && pos < sample_time())
		sample_idx = source->sample_rate * pos;
}

double SamplePCMF::sample_time() const
{
	return  source && source->sample_rate ? static_cast<double>(source->size) / source->sample_rate : 0.;
}
}