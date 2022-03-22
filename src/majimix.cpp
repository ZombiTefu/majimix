/**
 * @file majimix.cpp
 *
 * @section majimix_desc_cpp DESCRIPTION
 *
 * This file contains the implementation of the Majimix mixer for the PortAudio library.
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


#include "majimix.hpp"
#include "wave.hpp"
#include <cstring>
#include <portaudio.h>
#include <cassert>
#include <vorbis/vorbisfile.h>
#include <fstream>
#include <functional>
#include <atomic>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include "kss.hpp"


namespace majimix {


/*
 * For fixed-point calculation
 * FP_SHIFT should be between 10 and 32
 */
constexpr uint_fast64_t FP_SHIFT = 16;
/*
 * For fixed-point calculation
 * Mask of FP_SHIFT
 */
constexpr uint_fast64_t FP_MASK = ((uint_fast64_t)1 << FP_SHIFT) - 1;

/* ---------- decoders i16 ---------- */

static int ui8_to_i16(const char *data)
{
	return ((unsigned char)*data << 8) - 0x8000;
}

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
static int in_to_i16_le(const char *data)
{
	return static_cast<unsigned char>(data[N - 2]) | (data[N - 1] << 8);
}

/*
 * int alaw(const char*)
 * a-law to signed i16 decoder
 * data Audio data
 * return the converted signed i16 value
 */
static int alaw(const char *data)
{
	return wave::ALaw_Decode(*data);
}

/*
 * int ulaw(const char*)
 * μ-law to signed i16 decoder
 * data Audio data
 * return the converted signed i16 value
 */
static int ulaw(const char *data)
{
	return wave::MuLaw_Decode(*data);
}

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
static int float_to_i16(const char *data)
{
	FLOAT_TYPE v = *reinterpret_cast<const FLOAT_TYPE *>(data);
	return v * 0x7FFF;
}

/* ---------- decoders i24 ---------- */

/*
 * int ui8_to_i24(const char*)
 * Unsigned i8 to signed i24 decoder
 * data Audio data
 * return the converted signed i24 value
 */
static int ui8_to_i24(const char *data)
{
	return ((unsigned char)*data << 16) - 0x800000;
}

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
static int in_to_i24_le(const char *data)
{
	return static_cast<unsigned char>(data[N - 3]) | static_cast<unsigned char>(data[N - 2]) << 8 | (data[N - 1] << 16);
}

/*
 * int in_to_i24_le<2>(const char*)
 * Little-endian signed i16 to signed i24 decoder
 * data Audio data (little-endian)
 * return the converted signed i24 value
 */
template <>
int in_to_i24_le<2>(const char *data)
{
	return static_cast<unsigned char>(data[0]) << 8 | (data[1] << 16);
}

/*
 * @fn int in_to_i24_le<1>(const char*)
 * @brief Unsigned i8 to signed i24 decoder
 *        Specialization for uint8 - 8bits pcm format is supposed to be unsigned
 *        ui8_to_i24 equivalent
 * @param data Audio data
 * @return The converted signed i24 value
 */
template <>
int in_to_i24_le<1>(const char *data)
{
	return ((unsigned char)*data << 16) - 0x800000;
}

/*
 * @fn int alaw_i24(const char*)
 * @brief a-law to signed i24 decoder
 * @param data Audio data
 * @return The converted signed i24 value
 */
static int alaw_i24(const char *data)
{
	return majimix::wave::ALaw_Decode(*data) << 8;
}

/*
 * @fn int ulaw_i24(const char*)
 * @brief μ-law to signed i24 decoder
 * @param data Audio data
 * @return The converted signed i24 value
 */
static int ulaw_i24(const char *data)
{
	return majimix::wave::MuLaw_Decode(*data) << 8;
}

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
static int float_to_i24(const char *data)
{
	// FLOAT_TYPE v = *std::launder(reinterpret_cast<const FLOAT_TYPE*>(data));
	FLOAT_TYPE v = *(reinterpret_cast<const FLOAT_TYPE *>(data));
	return v * 0x7FFFFF;
}

/**
 * @brief Samples audio format
 *
 * Accepted WAVE audio format
 *
 */
enum class AuFormat
{
	none,		  /**< none */
	uint_8bits,	  /**< uint_8bits unsigned i8 format */
	int_16bits,	  /**< int_16bits signed i12 or i16 formats */
	int_24bits,	  /**< int_24bits signed i24 format */
	int_32bits,	  /**< int_32bits signed i32 format */
	float_32bits, /**< float_32bits IEEE float 32 bits format */
	float_64bits, /**< float_64bits IEEE float 64 bits format */
	alaw,		  /**< alaw a-law format */
	ulaw		  /**< ulaw µ-law format */
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
	virtual int read(int *buffer, int sample_count) = 0;

	/**
	 * @fn void seek(int)=0
	 * @brief Specify a specific point in the stream to begin or continue decoding.
	 *        Seeks to a specific audio sample number, specified in pcm samples.
	 * @param pos pcm sample number
	 */
	virtual void seek(int pos) = 0;

	/**
	 * @fn void seek_time(double)=0
	 * @brief Specify a specific point in the stream to begin or continue decoding.
	 *        Seeks to the specific time location in the stream, specified in seconds.
	 * @param pos time location in seconds
	 */
	virtual void seek_time(double pos) = 0;
};




/* ---------- SourcePCM & SamplePCM implementation ----------
 * PCM data is loaded from wave files and stored in memory
 */


class SamplePCM;
/**
 * Function pointer typedef for reading the sources
 */
using SourceReader = std::function<int(int*, int, int &, uint_fast64_t &)>;

/**
 * @class SourcePCM
 * @brief PCM Source implementation.
 *        All PCM data is stored in memory.
 *        Used with WAVE files.
 */
class SourcePCM : public Source {

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
	/** pcm data size in bytes (1 : 8 bits, 2 : 12/16 bits, 3 : 24 bits ... ) */
	int data_size;
	/** number of sample */
	int size;
	/** pcm data */
	std::vector<char> pcm;

	/** sample decoder */
	std::function<int(char*)> decoder;

	/** mixer format : rate */
	int mixer_rate;
	/** mixer format : bits  16 or 24 allowed */
	int mixer_bits;
	/** mixer format: channels - only 1 o 2 allowed */
	int mixer_channels;

	/** step of the Sample */
	uint_fast64_t sample_step;

	/** function pointer to the template read function */
	SourceReader read_fn;

	/**
	 * @fn void configure()
	 * @brief verifies and completes source initialization
	 */
	void configure();
	friend bool load_wave(const std::string &filename, SourcePCM &source);

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
	template<bool STEREO_INPUT, bool STEREO_OUTPUT>
	int read(int* out_buffer, int sample_count, int &sample_idx, uint_fast64_t &sample_frac);

	friend class SamplePCM;

public:
	void set_output_format(int samples_per_sec, int channels = 2, int bits = 16) override;
	
	/* create a SamplePCM associated with this Source */
	std::unique_ptr<Sample> create_sample() override;
};

/**
 * @class SamplePCM
 * @brief Sample associated with a SourcePCM
 *
 */
class SamplePCM : public Sample {

	/** SourcePCM associated with this sample */
	const SourcePCM *source;

	/** sample index */
	int sample_idx  = 0;

	/** fractional part of the sample index */
	uint_fast64_t sample_frac = 0;

public:
	SamplePCM(const SourcePCM &s);
	int read(int* buffer, int sample_count) override;
	void seek(int pos);
	void seek_time(double pos);

	/** duration in seconds */
	double sample_time() const;
};

/**
 * @fn bool load_wave(const std::string&, SourcePCM&)
 * @brief  Initialize a SourcePCM from a WAVE file.
 *
 * @param filename
 * @param source
 * @return true if the source has been correctly initialized
 */
bool load_wave(const std::string &filename, SourcePCM &source)
{
	bool done = false;

	/* reset previous format */
	source.format = AuFormat::none;
	source.ready = false;
	source.pcm.clear();
	source.data_size = 0;
	source.decoder = nullptr;

	wave::pcm_data pcm;

	if(wave::load_wave(filename, pcm))
	{
		wave::fmt_base &fmt = pcm.fmt;

		source.sample_rate         = fmt.nSamplesPerSec;
		source.sample_size         = fmt.nBlockAlign;
		source.channels            = fmt.nChannels;
		source.channel_size        = fmt.nBlockAlign / fmt.nChannels;

		source.data_size           = pcm.data.size();
		source.size                = pcm.data.size() / fmt.nBlockAlign;
		source.pcm                 = std::move(pcm.data);


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

		// review this check: case of 12bits which are in principle aligned on 16bits (and treated as 16 bits)
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
			source.format = AuFormat::alaw;
			break;
		case wave::WAVE_FORMAT::WAVE_FORMAT_MULAW:
			source.format = AuFormat::ulaw;
			break;
		case wave::WAVE_FORMAT::WAVE_FORMAT_PCM :
		{
			switch(fmt.wBitsPerSample)
			{
			case 8 :
				source.format = AuFormat::uint_8bits;
				break;
			case 12 : // ??? : 12 bits - first byte (less significant) the first 4 bits are 0 => we have to load int16 then (>>4)
			case 16 :
				source.format = AuFormat::int_16bits;
				break;
			case 24 :
				source.format = AuFormat::int_24bits;
				break;
			case 32 :
				source.format = AuFormat::int_32bits;
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
				source.format = AuFormat::float_32bits;
				break;
			case 64 :
				source.format = AuFormat::float_64bits;
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
		done = source.format != AuFormat::none;
		if(done)
			source.configure();
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

		sample_step = (static_cast<uint_fast64_t>(sample_rate) << FP_SHIFT) /  mixer_rate;

		/* decoder */
		switch(format)
		{
		case AuFormat::alaw:
			decoder = mixer_bits == 16 ? alaw : alaw_i24;
		break;
		case AuFormat::ulaw:
			decoder = mixer_bits == 16 ? ulaw : ulaw_i24;
		break;
		case AuFormat::uint_8bits:
			decoder = mixer_bits == 16 ? ui8_to_i16 : ui8_to_i24;
		break;
		case AuFormat::int_16bits:
			decoder = mixer_bits == 16 ? in_to_i16_le<2> : in_to_i24_le<2>;
		break;
		case AuFormat::int_24bits:
			decoder = mixer_bits == 16 ? in_to_i16_le<3> : in_to_i24_le<3>;
		break;
		case AuFormat::int_32bits:
			decoder = mixer_bits == 16 ? in_to_i16_le<4> : in_to_i24_le<4>;
		break;
		case AuFormat::float_32bits:
			decoder = mixer_bits == 16 ? float_to_i16<float> : float_to_i24<float>;
		break;
		case AuFormat::float_64bits:
			decoder = mixer_bits == 16 ? float_to_i16<double> : float_to_i24<double>;
		break;
		default:
			return;
		}

		/* read function */
		if(mixer_channels == 1)
		{
			if(channels > 1)
				read_fn = std::bind(&SourcePCM::read<true, false>, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
			else
				read_fn = std::bind(&SourcePCM::read<false, false>, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
		}
		else if(mixer_channels == 2)
		{
			if(channels > 1)
				read_fn = std::bind(&SourcePCM::read<true, true>, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
			else
				read_fn = std::bind(&SourcePCM::read<false, true>, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
		}
		ready = true;
	}
}

void SourcePCM::set_output_format(int samples_per_sec, int channels, int bits)
{
	ready = false;
	mixer_rate = samples_per_sec;
	mixer_channels = channels;
	mixer_bits = bits;
	configure();
}

std::unique_ptr<Sample> SourcePCM::create_sample()
{
	if(ready)
		return std::make_unique<SamplePCM>(*this);
	return nullptr;
}

template<bool STEREO_INPUT, bool STEREO_OUTPUT>
int SourcePCM::read(int* out_buffer, int sample_count, int &sample_idx, uint_fast64_t &sample_frac)
{
	unsigned int sample_add;
	int out_sample_count = 0;
	if(sample_idx < size)
	{
		unsigned int idx = sample_idx * sample_size;
		char *data = pcm.data();
		int *out = out_buffer;

		if constexpr (STEREO_INPUT && STEREO_OUTPUT)
		{
			// stereo -> stereo
			int vl = decoder(data + idx);
			int vr = decoder(data + idx + channel_size);
			idx = (idx + sample_size) % data_size;
			int vl2 = decoder(data + idx);
			int vr2 = decoder(data + idx + channel_size);
			int ll,lr;

			while(out_sample_count < sample_count)
			{
				ll = ((vl2 - vl) * sample_frac >> FP_SHIFT) + vl;
				lr = ((vr2 - vr) * sample_frac >> FP_SHIFT) + vr;
				*out++=ll;
				*out++=lr;
				++out_sample_count;

				sample_frac += sample_step;
				sample_add = sample_frac >> FP_SHIFT;
				if(sample_add)
				{
					sample_idx  += sample_add;
					sample_frac &= FP_MASK;
					if(sample_idx >= size)
						break;
					idx = (sample_idx  * sample_size)/* % data_size*/;
					vl = decoder(data + idx);
					vr = decoder(data + idx + channel_size);
					idx = (idx + sample_size) % data_size;
					vl2 = decoder(data + idx);
					vr2 = decoder(data + idx + channel_size);
				}
			}
		}
		else if constexpr (STEREO_OUTPUT)
		{
			// mono -> stereo
			int l;
			int v = decoder(data + idx);
			idx = (idx + sample_size)%data_size;
			int w = decoder(data + idx);

			//do
			while(out_sample_count < sample_count)
			{
				l = ((w - v) * sample_frac >> FP_SHIFT) + v;
				*out++=l;
				*out++=l;
				++out_sample_count;

				sample_frac += sample_step;
				sample_add = sample_frac >> FP_SHIFT;
				if(sample_add)
				{
					sample_idx  += sample_add;
					sample_frac &= FP_MASK;
					if(sample_idx >= size)
						break;

					idx = (sample_idx  * sample_size);
					v = decoder(data + idx);
					idx = (idx + sample_size) % data_size;
					w = decoder(data + idx);
				}
			}
		}
		else
		{
			// mono/stereo -> mono
			int v = 0, w = 0, l;
			const int shift = (channels>>1);
			unsigned int idx2 = (idx + sample_size)%data_size;
			for( int c = 0; c < channels; ++c)
			{
				v += decoder(data + idx);
				w += decoder(data + idx2);
				idx += channel_size;
				idx2 += channel_size;
			}

			while(out_sample_count < sample_count)
			{
				l = ((w - v) * sample_frac >> FP_SHIFT) + v;
				*out++= (l>>shift);
				++out_sample_count;

				sample_frac += sample_step;
				sample_add = sample_frac >> FP_SHIFT;
				if(sample_add)
				{
					sample_idx  += sample_add;
					sample_frac &= FP_MASK;
					if(sample_idx >= size)
						break;

					idx = (sample_idx * sample_size);
					idx2 = (idx + sample_size) % data_size;
					v = 0;
					w = 0;
					for( int c = 0; c < channels; ++c)
					{
						v += decoder(data + idx);
						w += decoder(data + idx2);
						idx += channel_size;
						idx2 += channel_size;
					}
				}
			}
		}
	}
	return out_sample_count;
}

SamplePCM::SamplePCM(const SourcePCM &s) : source {&s}
{}


int SamplePCM::read(int* buffer, int sample_count)
{
	int r = source->read_fn(buffer, sample_count, sample_idx, sample_frac);
	if(r < sample_count)
	{
		// EOF - AUTOLOOP
		sample_idx  = 0;
		sample_frac = 0;
	}
	return r;
}

void SamplePCM::seek(int pos)
{
	if(source && pos < source->size && pos >= 0)
	{
		sample_idx  = pos;
		sample_frac = 0;
	}
}
void SamplePCM::seek_time(double pos)
{
	if(source && pos >= 0 && pos < sample_time())
		sample_idx = source->sample_rate * pos;
}

double SamplePCM::sample_time() const
{
	return  source && source->sample_rate ? static_cast<double>(source->size) / source->sample_rate : 0.;
}






/* ---------- VORBIS ----------
 
 */


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



class SourceVorbis : public Source {

	std::string filename;

	/* sample decoder */
	std::function<int(char*)> decoder; 
	int mixer_rate;
	int mixer_bits;    // 16/24
	int mixer_channels;

	/* step of the Sample */

	//void configure();
	//friend bool set_vorbis_file(const std::string &filename, SourceVorbis &source);
	friend class SampleVorbis; 


public:
	void set_file(const std::string& filename);
	/* set the mixer format */
	void set_output_format(int samples_per_sec, int channels = 2, int bits = 16) override;
	/* create a SampleVorbis associated with this Source */
	std::unique_ptr<Sample> create_sample() override;
};

class SampleVorbis : public Sample {
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

	uint_fast64_t sample_step;

	/* multistream support */
	int current_section, last_section;

	uint_fast64_t sample_frac;
	int idx_1, idx_2, idx_lim;

	bool initialized = false;
	friend class SourceVorbis;

	constexpr static int internal_buffer_size = 4096;
	char internal_buffer[internal_buffer_size];
	int buffer_read_length = 0;

	/* verifies and completes source initialization */
	void configure();

public:
	SampleVorbis(const SourceVorbis &s);
	~SampleVorbis();
	int read(int* buffer, int sample_count) override;
//	int read_test(char* buffer, int sample_count);
	void seek(int pos) override;
	void seek_time(double pos) override;
	/* duration in seconds */
	double sample_time();

};



void SourceVorbis::set_file(const std::string& filename)
{
	this->filename = filename;
}

void SourceVorbis::set_output_format(int samples_per_sec, int channels, int bits)
{
	mixer_rate = samples_per_sec;
	mixer_channels = channels;
	mixer_bits = bits;
	decoder = bits == 16 ? in_to_i16_le<2> : in_to_i24_le<2>;
}

std::unique_ptr<Sample> SourceVorbis::create_sample()
{
	return std::make_unique<SampleVorbis>(*this);
}

SampleVorbis::SampleVorbis(const SourceVorbis &s)
: source {&s},
  sample_rate {0},
  sample_size {0},
  channels {0},
  channel_size {0},
  sample_step {0},
  current_section{0},
  last_section{-1},
  sample_frac {0},
  idx_1 {0},
  idx_2 {0},
  idx_lim {0}
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
		initialized = true;
	}
}

SampleVorbis::~SampleVorbis()
{
#ifdef DEBUG
	std::cout << "~SampleVorbis()()\n";
#endif
	ov_clear(&file);
}

void SampleVorbis::configure()
{
	initialized = false;

	/* Read file info */
	vorbis_info* vorbisInfo = ov_info(&file, -1);
	sample_rate = vorbisInfo->rate;
	channels    = vorbisInfo->channels;
	channel_size = 2;
	sample_size = channel_size * channels;
	sample_step = (static_cast<uint_fast64_t>(sample_rate) << FP_SHIFT) /  source->mixer_rate;

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


int SampleVorbis::read(int* buffer, int sample_count)
{
	int out_sample_count = 0;
	unsigned int sample_add;
	bool done = false;
	int *out = buffer;

	while(!done)
	{
		// buffer loading test
		if(idx_lim >= buffer_read_length)
		{
			if(idx_1 >= buffer_read_length)
			{
				// 1 : no usable data in the current buffer
				idx_1   -= buffer_read_length;
				buffer_read_length = ov_read(&file, internal_buffer, internal_buffer_size, 0, 2, 1, &current_section);
			}
			else
			{
				// cas 2 : data are partially present in the buffer
				// data to take over (to preserve)
				int preserve_length = buffer_read_length - idx_1;
				std::copy(internal_buffer + idx_1, internal_buffer  + buffer_read_length, internal_buffer);
				idx_1    = 0;     

				// read the buffer
				buffer_read_length = ov_read(&file, internal_buffer + preserve_length, internal_buffer_size - preserve_length, 0, 2, 1, &current_section);
				buffer_read_length += preserve_length;
			}
			if(current_section != last_section)
			{
				configure();
				last_section = current_section;
			}
			idx_2 = idx_1 + sample_size;
			idx_lim = idx_2 + sample_size;
		}

		if(buffer_read_length > 0)
		{
			if(source->mixer_channels == 1)
			{
				// (channels) => mono
				int v_1 = 0, v_2 = 0, l;
				int cs = 0;
				const int shift = (channels>>1);
				for( int c = 0; c < channels; ++c)
				{
					v_1 += source->decoder(internal_buffer + idx_1 + cs);
					v_2 += source->decoder(internal_buffer + idx_2 + cs);
					cs += channel_size;
				}
				l = ((v_2 - v_1) * sample_frac >> FP_SHIFT) + v_1;
				*out++= (l>>shift);
			}
			else
			{
				// we assume a stereo output : surround, 5.1, 7.1  - not supported (yet)
				if(channels > 1)
				{
					// stereo => stereo
					int vl_1 = source->decoder(internal_buffer + idx_1 );
					int vr_1 = source->decoder(internal_buffer + idx_1 + channel_size);
					int vl_2 = source->decoder(internal_buffer + idx_2 );
					int vr_2 = source->decoder(internal_buffer + idx_2 + channel_size);

					int ll,lr;

					ll = ((vl_2 - vl_1) * sample_frac >> FP_SHIFT) + vl_1;
					lr = ((vr_2 - vr_1) * sample_frac >> FP_SHIFT) + vr_1;

					*out++=ll;
					*out++=lr;
				}
				else
				{
					// mono => stereo
					int v_1 = source->decoder(internal_buffer + idx_1 );
					int v_2 = source->decoder(internal_buffer + idx_2 );
					int l;
					l = ((v_2 - v_1) * sample_frac >> FP_SHIFT) + v_1;
					*out++=l;
					*out++=l;
				}
			}
			++out_sample_count;
			sample_frac += sample_step;
			sample_add = sample_frac >> FP_SHIFT;
			if(sample_add)
			{
				sample_frac &= FP_MASK;
				idx_1 += sample_add * sample_size;
				idx_2 = idx_1 + sample_size;
				idx_lim = idx_2 + sample_size;
			}

			if(out_sample_count ==  sample_count)
				done = true;
		}
		else
		{
			done = true;
			// EOF - AUTOLOOP
			seek(0);
		}
	}
	return out_sample_count;
}

void SampleVorbis::seek(int pos)
{
	buffer_read_length = 0;
	idx_1 = 0;
	ov_pcm_seek(&file, pos);
}
void SampleVorbis::seek_time(double pos)
{
	buffer_read_length = 0;
	idx_1 = 0;
	ov_time_seek(&file, pos);
}

double SampleVorbis::sample_time()
{
	return ov_time_total(&file,-1);
}


/* --------------------------------------------------------------------------- */
/* ------------------------------ V2 ----------------------------------------- */


/*  ---------- BufferedMixer ----------
 * 
 * Allows to use a thread (independent from PA) dedicated to
 * to mixing: retrieving data from samples and merging it into an internal buffer
 * The mixing method has to be implemented elsewhere, BufferedMixer just
 * to provide a write buffer to store these data (it will claim n samples)
 *
 * PA can access the mixed audio data without blocking through the read method
 */

class BufferedMixer {
	/** Buffer "packet" size (size in byte) */
	const int buffer_packet_size;
	/** Buffer "packet" sample size (size in sample) - ex :  24bits stereo =>  buffer_packet_size / (3 * 2) */
	const int buffer_packet_sample_size;  
	/** Size of one sample in byte (bits x channels) - ex  24bits stereo => 3x2=6 */
	const int sample_size; 
	/** Total buffer size in byte */
	const int buffer_total_size;

	/**
	 * The buffer
	 * Contains data read from sample.
	 */
	std::vector<char> buffer; 

	std::atomic<int> read_position;  
	int read_inrange_index;           // read index within the buffer [0, buffer_packet_size]
	std::atomic<int> write_position;
	std::atomic<bool> producer_on;
	std::atomic<bool> paused;

	/** producer thread : read data from the sample and fill the buffer */
	std::thread producer;
	std::mutex m;
	std::condition_variable cv;

	/** producer thread function : fills buffer whith audio data read from sample */
	void write();

	/** External mixing and encode function */
	using fn_mix = std::function<void(std::vector<char>::iterator it_out, int requested_sample_count)>;
	fn_mix mix;


public:
	/**
	 * Constructor
	 * @param buffer_count the number of buffer to use - 5 should be great
	 * @param buffer_sample_size Sample capacity of one buffer
	 * @param sample_size        Size in byte of one sample (ex: 24 bits stereo => 3 x 2 = 6)
	 */
	BufferedMixer(int buffer_count, int buffer_sample_size, int sample_size);
	~BufferedMixer();

	/**
	 * @return true if the mixer thread is started
	 */
	bool is_started() const;
	/**
	 * @return true if the mixer thread is paused
	 */
	bool is_paused() const;
	/**
	 * @return if the mixer thread is active - started and not paused
	 */
	bool is_active() const;


	int get_buffer_count() const;
	int get_buffer_packet_size() const;
	int get_buffer_packet_sample_size() const;

	/**
	 * Assign the dedicated mixing external function
	 * @param fn
	 */
	void set_mixer_function(fn_mix fn);

	/**
	 *  start the producer thread
	 */
	void start();

	/**
	 * pause / resume the producer thread - wait before return
	 * @param pause
	 */
	void pause(bool pause);

	/**
	 * stop the producer thread
	 */
	void stop();

	/**
	 * Initialize out_buffer with audio data in correct format
	 * @param out_buffer
	 * @param requested_sample_count
	 */
	void read(char *out_buffer, int requested_sample_count);
};


BufferedMixer::BufferedMixer(int buffer_count, int buffer_sample_size, int sample_size)
: /*buffer_packet_count {buffer_count},*/
  buffer_packet_size {buffer_sample_size * sample_size},
  buffer_packet_sample_size {buffer_sample_size},
  sample_size {sample_size},
  buffer_total_size {buffer_count * buffer_sample_size * sample_size},
  read_position {0},
  read_inrange_index {0},
  write_position {0},
  producer_on {false},
  paused {false}
{
#ifdef DEBUG
	std::cout << "BufferedMixer()\n";
#endif

	buffer.assign(buffer_total_size, 0);

#ifdef DEBUG
	std::cout <<"buffer count "<<buffer_count
			  << "\nbuffer_packet_sample_size " <<buffer_packet_sample_size
			  << "\nbuffer_packet_size " <<buffer_packet_size
			  <<"\ntotal buffers size "<<buffer_total_size
			  <<"\n";
#endif
}

BufferedMixer::~BufferedMixer()
{
#ifdef DEBUG
	std::cout << "~BufferedMixer()\n";
#endif

	stop();
}

bool BufferedMixer::is_started() const
{
	return producer_on;
}

bool BufferedMixer::is_paused() const
{
	return paused;
}

bool BufferedMixer::is_active() const
{
	return producer_on && !paused;
}

int BufferedMixer::get_buffer_count() const
{
	return buffer_packet_size ? buffer_total_size / buffer_packet_size : 0;
}

int BufferedMixer::get_buffer_packet_size() const
{
	return buffer_packet_size;
}

int BufferedMixer::get_buffer_packet_sample_size() const
{
	return buffer_packet_sample_size;
}


void BufferedMixer::set_mixer_function(fn_mix fn)
{
	// the mix function can be updated if the producer is stopped or paused
	if(!is_active())
		mix = fn;
}


void BufferedMixer::start()
{
#ifdef DEBUG
	std::cout << "BufferedMixer::start()\n";
#endif

	if(!producer_on && mix)
	{
		write_position = 0;
		read_position = 0;
		read_inrange_index = 0;
		producer_on = true;
		producer = std::thread(&BufferedMixer::write, this);
	}
}

void BufferedMixer::pause(bool paused)
{
	if(this->paused == paused)
		return;
	{
#ifdef DEBUG
		std::cout << (paused ? "BufferedMixer::pause\n" : "BufferedMixer::resume\n");
#endif
		std::lock_guard<std::mutex> lg(m);
		this->paused = paused;
		cv.notify_one();
	}
}

void BufferedMixer::stop()
{
#ifdef DEBUG
	std::cout << "BufferedMixer::stop()\n";
#endif

	if(producer_on)
	{
		producer_on = false;
		cv.notify_one();
		if(producer.joinable())
			producer.join();

#ifdef DEBUG
		std::cout << "BufferedMixer stopped on write_position " << write_position <<"\n";
#endif
	}
}

void BufferedMixer::write()
{
#ifdef DEBUG
	std::cout << "BufferedMixer::write() procucer started\n";
#endif

	int next;
	while(producer_on)
	{
		// lock of the critical section 
		std::unique_lock<std::mutex> lkp(m);

#ifdef PRODUCERDEBUG
		std::cout << "BufferedMixer::write producer writes in write_position "<< write_position << "\n";
#endif
		// sample mixing and audio data conversion
		mix( buffer.begin() + write_position, buffer_packet_sample_size);

		// release lock
		m.unlock();

		// Compute the next writing position
		next = (write_position + buffer_packet_size) % buffer_total_size;

		// check the new writing position and a possible pause
		// we use a while because of spurious wakeup
		while((next == read_position || paused) && producer_on)
		{
#ifdef PRODUCERDEBUG
			if(paused)
				std::cout << "BufferedMixer::write paused in write_position "<< write_position << "\n";
			else
				// wait for reader
				std::cout << "BufferedMixer::write producer waiting for reader to write in write_position "<< next << "\n";
#endif
			std::unique_lock<std::mutex> lk(m); 			// lock
			cv.wait(lk, [&] {return (next != read_position && !paused) || !producer_on ;});  // unlock/wait -> lock after wait 
			// unlock 
			// m.unlock();
		}

		// next range
		write_position = next;
#ifdef PRODUCERDEBUG
		std::cout << "BufferedMixer::write producer next write_position "<< write_position << "\n";
#endif
	}

#ifdef DEBUG
	std::cout << "BufferedMixer::write procucer stopped\n";
#endif
}

void BufferedMixer::read(char* out_buffer, int requested_sample_count)
{
#ifdef CONSUMERDEBUG
	std::cout << "BufferedMixer::read()\n";
#endif
	int out_count = 0;
	int remaining_out_count = requested_sample_count * sample_size;
	do
	{
		// test if the buffer is free for reading
		if(write_position == read_position)
		{
			// busy => we will still fill out_buffer with 0
#ifdef CONSUMERDEBUG
			if(producer_on)
				std::cout << "BufferedMixer::read underrun - can't wait for producer need to read read_position " << read_position << "\n";
			else
				std::cout << "BufferedMixer::read underrun - producer is off need to read read_position " << read_position << "\n";
#endif

#ifdef CONSUMERUNDERRUNDEBUG
			std::cerr << "underrun\n";
#endif

			std::fill(out_buffer + out_count, out_buffer + out_count + remaining_out_count, (char)0);
			return;
		}

		// number of bytes remaining in this buffer
		int remaining_range_count = buffer_packet_size - read_inrange_index;
		// number of bytes that we will recover
		int take_range_count = std::min(remaining_range_count, remaining_out_count);
		// position (bytes) in buffer
		int cur_range_position = read_position + read_inrange_index;

		// copy from internal buffer to output
		std::copy(buffer.begin() + cur_range_position, buffer.begin() + cur_range_position + take_range_count, out_buffer +  out_count);

		out_count += take_range_count;
		remaining_out_count   -= take_range_count;
		remaining_range_count -= take_range_count;

		if(remaining_range_count)
			read_inrange_index += take_range_count;
		else
		{
			// next
			read_inrange_index = 0;
			read_position = (read_position + buffer_packet_size) % buffer_total_size;
			cv.notify_one();
		}
	}
	while(remaining_out_count);
}




//
//int get_source_id(int handle) {return handle & 0xFFF;}
//int get_channel_id(int handle) {return (handle >> 12) & 0xFFF;}
//int get_handle(int source_id, int channel_id) {return ((channel_id & 0xFFF) << 12) | (source_id &0xFFF);}
// channel number or kss line  (12 bits)      source type  (4 bits)     source handle (12 bits)
//        0xFFFF                                        F                 FFF
// Handle int (at least 32 bits)
// bits  0-11 size 12 bits : source id (source index or kss cartdridge index)
// bits 12-15 size  4 bits : source type (0 : wave ogg 1: kss)
// bits 16-27 size 12 bits : channel number or kss line
static int get_untyped_source_id(int handle) { return handle & 0xFFF; }
static int get_source_id(int handle) { return handle & 0xFFFF; }
static int get_channel_id(int handle) { return (handle >> 16) & 0xFFF; }
static int get_handle(int source_id, int channel_id) { return ((channel_id & 0xFFF) << 16) | (source_id & 0xFFFF); }
static int get_kss_source_id(int source_id) { return (source_id | 0x1000) & 0xFFFF; }
static int get_source_type(int handle_or_source_id) { return (handle_or_source_id >> 12) & 0xF; }






bool Majimix::start_mixer() 
{
	return start_stop_mixer(true);
}

bool Majimix::stop_mixer() 
{
	return start_stop_mixer(false);
}

bool Majimix::pause_mixer() 
{
	return pause_resume_mixer(true);
}

bool Majimix::resume_mixer() 
{
	return pause_resume_mixer(false);
}

void Majimix::pause_playback(int play_handle)
{
	pause_resume_playback(play_handle, true);
}

void Majimix::resume_playback(int play_handle)
{
	pause_resume_playback(play_handle, false);
}


namespace pa {

/**
 * @class MixerChannel
 * @brief
 *
 */
class MixerChannel {
	std::atomic<bool> active;     // Set to true to activate the Channel. If PA thread is active, it is the only thread that can reset the value.
	std::atomic<bool> stopped;
	std::atomic<bool> paused;
	std::atomic<bool> loop;

	std::unique_ptr<Sample> sample;
	int sid; // FIXME atomic ! (cf stop_playback)
	friend class MajimixPa;

public:
	MixerChannel();

};



MixerChannel::MixerChannel()
: active  {false},
//  started  {false}
  stopped {true},
  paused  {false},
  loop    {false},
  sample  {nullptr},
  sid {0}

{}

/**
 * @class MajimixPa
 * @brief PortAudio implementation of Majimix.
 *
 */
class MajimixPa : public Majimix  {

	std::unique_ptr<BufferedMixer> mixer;
	std::vector<std::unique_ptr<Source>> sources;
	std::vector<std::unique_ptr<MixerChannel>> mixer_channels;
	// kss support - kss sources
	std::vector<std::unique_ptr<kss::CartridgeKSS>> kss_cartridges;



	/* mixer parameters */
	int sampling_rate = 44100;
	int channels = 2;
	int bits = 16; 			// 16 / 24

	/* 0 - 255 */
	std::atomic_int master_volume = 128;

	/* internal mixing data */
	std::vector<int> internal_mix_buffer;
	std::vector<int> internal_sample_buffer;

	/* audio converter */

	/**
	 * @tparam N     2 16 bits 3 24 bits
	 * @param it_int mixed input buffer
	 * @param it_out output buffer
	 * @param sample_count number of samples to convert
	 */
	template <int N>
	void encode_Nbits(std::vector<char>::iterator it_out);
	using fn_encode = std::function<void(std::vector<char>::iterator it_out)>;
	fn_encode encode;

	void mix(std::vector<char>::iterator it_out, int requested_sample_count);
	void read(char *out_buffer, int requested_sample_count);

	/* PortAudio stream */
	PaStream *m_stream {nullptr};

    /* Creates PortAudio stream */
	bool create_stream();

	/* PA callback */
	static int paCallback( const void *inputBuffer, void *outputBuffer,
						   unsigned long framesPerBuffer,
						   const PaStreamCallbackTimeInfo* timeInfo,
						   PaStreamCallbackFlags statusFlags,
						   void *userData );
	// // int paCallback2( const void *inputBuffer, void *outputBuffer,
	// // 					   unsigned long framesPerBuffer,
	// // 					   const PaStreamCallbackTimeInfo* timeInfo,
	// // 					   PaStreamCallbackFlags statusFlags,
	// // 					   void *userData );
	// bool set_playback(bool on) ;

// //	bool kss_cartridge_action(int kss_source_handle, bool need_sync, std::function<void(kss::CartridgeKSS&)> fn_action);

	/**
	 * @brief Get a CartridgeKSS an a KSSLine from a kss handle
	 *
	 * @param kss_handle
	 * @param need_line  True : Tell if the kss handle must represent a valid kss line. False : the kss handle must be a valid kss source (but can eventualy contains a line)
	 * @param cartridge
	 * @param line_id
	 * @return
	 */
	bool get_cartrigde_and_line(int kss_handle, bool need_line, kss::CartridgeKSS *&cartridge, int &line_id);
	// template<typename T>
	// T kss_cartridge_action(int kss_source_handle, bool need_sync, T default_ret_val, std::function<T(kss::CartridgeKSS&)> fn_action);
	// template<typename T>
	// T kss_cartridge_action_line(int kss_source_handle, bool need_sync, T default_ret_val, std::function<T(kss::CartridgeKSS&, int line_id)> fn_action);


// 	template<typename T>
// 	T kss_cartridge_action_nosync(int kss_source_handle, bool need_line, T default_ret_val, std::function<T(kss::CartridgeKSS&, int line_id)> fn_action);


	template<typename T>
	T kss_cartridge_action(int kss_source_handle, bool need_sync, bool need_line, T default_ret_val, std::function<T(kss::CartridgeKSS&, int line_id)> fn_action);



public:
	~MajimixPa();
	bool set_format(int rate, bool stereo = true, int bits = 16, int channel_count = 6) override;

	/* mixer */
	bool start_stop_mixer(bool start) override;
	bool pause_resume_mixer(bool pause) override;
	int get_mixer_status() override;


	/* obtain a source handle */

	/**
	 * @brief Add a source to the mixer (wave, ogg)
	 * 
	 * @param name the source filename
	 * @return int the handle
	 */
	int add_source(const std::string& name) override;

	/**
	 * @brief Add a kss source to the mixer
	 * 
	 * @param name kss file
	 * @param lines the number of lines (channels)
	 * @param silent_limit_ms silent duration for autostop detection
	 * @return int 
	 */
	int add_source_kss(const std::string &name, int lines, int silent_limit_ms) override;

	/**
	 * @brief Drop a source from the mixer. 
	 *        Can be called at any time.
	 * 
	 * @param source_handle 
	 * @return true the source has successfully been removed
	 * @return false the source handle is not valid
	 */
	bool drop_source(int source_handle) override;

	void set_master_volume(int v) override;
	int play_source(int source_handle, bool loop = false, bool paused = false) override;
	void stop_playback(int play_handle) override;
	void set_loop(int play_handle, bool loop) override;
    void pause_resume_playback(int play_handle, bool pause) override;

	void pause_producer(bool);
	bool set_mixer_buffer_parameters(int buffer_count, int buffer_sample_size) override;
	
	int play_kss_track(int kss_handle, int track, bool autostop = true, bool forcable = true, bool force = true) override;
	bool update_kss_track(int kss_handle, int new_track, bool autostop = true, bool forcable = true, int fade_out_ms = 0) override;

	/**
	 * @brief Update volume
	 *
	 * Update volume for a specific line of a kss source of for all lines of a kss source.
	 *
	 * @param [in] kss_handle A kss source handle or a kss track handle.
	 * @param [in] volume Volume value between 0 and 100
	 * @return True if successful / False for an invalid \c kss_track_handle.
	 */
	bool update_kss_volume(int kss_handle, int volume);
	bool update_kss_frequency(int kss_source_handle, int frequency);
	// bool set_pause_kss(int kss_handle, bool pause);
	int get_kss_active_lines_count(int kss_source_handle);
	int get_kss_playtime_millis(int kss_play_handle) override;

};


MajimixPa::~MajimixPa()
{
#ifdef DEBUG
	std::cout<<"~MajimixPa()\n";
#endif

	start_stop_mixer(false);
}


bool MajimixPa::set_format(int rate, bool stereo, int bits, int channel_count)
{
	if(!m_stream)
	{
		if(rate >= 8000 && rate <= 96000 && (bits == 16 || bits == 24) )
		{
			sampling_rate = rate;
			channels      = stereo ? 2 : 1;
			this->bits    = bits;
			mixer_channels.clear();
			mixer_channels.reserve(channel_count);
			for(int i = 0; i < channel_count; ++i)
				mixer_channels.push_back(std::make_unique<MixerChannel>());

			for(auto &source : sources)
				if(source)
					source->set_output_format(sampling_rate, channels, bits);
			for(auto &cartridge : kss_cartridges)
				if(cartridge)
					cartridge->set_output_format(sampling_rate, channels, bits /*, 300*/);

#ifdef DEBUG
			std::cout << "MajimixPa::set_format\n\tsampling_rate : "<<sampling_rate<<"\n\tchannels : "<<channels<<"\n\tbits : "<<bits<<"\n\tvoices : "<<channel_count<<"\n";
#endif

			if(bits == 16)
				encode = std::bind(&MajimixPa::encode_Nbits<2>, this, std::placeholders::_1);
			else
				encode = std::bind(&MajimixPa::encode_Nbits<3>, this, std::placeholders::_1);

			//  high latency : latency = bufsz * 5 * 1000  / 44100 = 100 ms (0.1 sec)
			// => bufsz = 100 * rate / (buffer_count * 1000)
			int buffer_count = 5;
			int buffer_sample_size = 100 * rate / buffer_count / 1000;
			if(mixer)
			{
				buffer_count = mixer->get_buffer_count();
				buffer_sample_size = mixer->get_buffer_packet_sample_size();
			}

			return set_mixer_buffer_parameters(buffer_count, buffer_sample_size);
		}
	}
	return false;
}

bool MajimixPa::set_mixer_buffer_parameters(int buffer_count, int buffer_sample_size)
{
	if(m_stream) return false;
	mixer = std::make_unique<BufferedMixer>(buffer_count, buffer_sample_size, channels * (bits >> 3));

	size_t buffer_size = static_cast<long>(mixer->get_buffer_packet_sample_size()) * channels;
	internal_sample_buffer.assign(buffer_size, 0);
	internal_mix_buffer.assign(buffer_size, 0);

	mixer->set_mixer_function(std::bind(&MajimixPa::mix, this, std::placeholders::_1, std::placeholders::_2));

	// FIXME:  KSS support -> update buffers size

	return true;
}

/* ------------------- MIXER ------------------------ */

bool MajimixPa::start_stop_mixer(bool start)
{
	if(start)
	{
		if (!m_stream && mixer)
		{
			if (create_stream())
			{
				mixer->start();
				if (mixer->is_started())
				{
					// return set_playback(true);
					return pause_resume_mixer(false);
				}
			}
		}
		return false;
	}
	
	// stop
	if (m_stream)
	{
#ifdef DEBUG
		std::cout << "stop MajimixPa" << std::endl;
#endif
		// set_playback(false);
		pause_resume_mixer(true);
		PaError err;
		err = Pa_CloseStream(m_stream);
		if (err != paNoError)
		{
			std::cerr << "Error while closing stream - code " << err << std::endl;
		}
		m_stream = nullptr;
	}

	if (mixer)
		mixer->stop();
	
	return true;
}

bool MajimixPa::pause_resume_mixer(bool pause)
{
	// no stream return true for pause and false for resume
	if(!m_stream)
		return pause;

	PaError err = paNoError;
	err = Pa_IsStreamActive(m_stream);
	if(err < 0)
		return false;

	if(err == 0 && !pause) {
		// off -> on
		err = Pa_StartStream(m_stream);

	} else if(err == 1 && pause) {
		// on -> off
		err = Pa_StopStream(m_stream);
	}
	return err == paNoError;
}

int MajimixPa::get_mixer_status()
{
	int status = MixerStopped;
	if(m_stream)
	{
		PaError err = paNoError;
		err = Pa_IsStreamActive(m_stream);
		if(err < 0)
			status = MixerError; 		// error
		else
			status = err ? MixerRunning : MixerPaused;
	}
	return status;
}


/* ------------------- SOURCES ------------------------ */


int MajimixPa::add_source(const std::string& name)
{
	int id = 0;
	std::unique_ptr<Source> source;
	
	/* check wave format */
	if(majimix::wave::test_wave(name))
	{
		auto s = std::make_unique<SourcePCM>();
		if(load_wave(name, *s))
			source = std::move(s);
	}
	else
	/* check Vorbis format */
	{
		std::ifstream stream(name, std::ios::binary);
		OggVorbis_File file;
		int result = ov_test_callbacks(&stream, &file, nullptr, 0, {ogg_read, ogg_seek, nullptr, ogg_tell});
		ov_clear(&file);
		if(!result)
		{
			auto s = std::make_unique<SourceVorbis>();
			s->set_file(name);
			source = std::move(s);
		}
	}

	if(source)
	{
		// add source
		source->set_output_format(sampling_rate, channels, bits);
		int i = 0;
		for(auto &src : sources)
		{
			if(!src)
			{
				sources[i] = std::move(source);
				id = i+1;
				break;
			}
			++i;
		}
		if(!id)
		{
			sources.push_back(std::move(source));
			id = i+1;
		}
	}

	return id;
}

int MajimixPa::add_source_kss(const std::string& name, int lines, int silent_limit_ms)
{
	if (lines <= 0)
		return -1;

	KSS *kss = kss::load_kss(name);
	if (!kss)
		return -1;

	auto cartridge = std::make_unique<kss::CartridgeKSS>(kss, lines, sampling_rate, channels, bits, silent_limit_ms);


	bool need_resume = mixer && mixer->is_active();
	if(need_resume)
		mixer->pause(true);


	int id = 0;
	int i = 0;

	for(auto &c : kss_cartridges)
	{
		if(!c)
		{
#ifdef DEBUG
			std::cout << "insert cartridge in slot "<<i<<"\n";
#endif
			c = std::move(cartridge);
			//  kss_cartridges[i] = std::move(cartridge);
			id = i+1;
			break;
		}
		++i;
	}

	if(!id)
	{
#ifdef DEBUG
		std::cout << "insert cartridge in new slot "<<i<<"\n";
#endif
		kss_cartridges.push_back(std::move(cartridge));
		id = i+1;
	}

	if(need_resume)
		mixer->pause(false);

	return get_kss_source_id(id);
}

bool MajimixPa::drop_source(int source_handle)
{
	int source_type = get_source_type(source_handle);
	int source_id = get_source_id(source_handle);
	int untyped_source_id = get_untyped_source_id(source_handle);

	//bool drop_all = source_handle == 0;
	bool dropped = false;

	bool active = mixer && mixer->is_active();
	if(active)
		mixer->pause(true);

	if (source_handle == 0)
	{
		// drop all
		for (auto &mix_channel : mixer_channels)
		{
			mix_channel->active = false;
			mix_channel->paused = false;
			mix_channel->loop = false;
			mix_channel->sample.reset();
			mix_channel->sid = 0;
		}

		for (auto &s : sources)
			s.reset();

		for (auto &c : kss_cartridges)
			c.reset();

		dropped = true;
	}
	else if (source_id > 0)
	{
		// Regular sources
		if (source_type == 0)
		{
			for (auto &mix_channel : mixer_channels)
			{
				if (mix_channel->sid == source_id)
				{
					mix_channel->active = false;
					mix_channel->paused = false;
					mix_channel->loop = false;
					mix_channel->sample.reset();
					mix_channel->sid = 0;
				}
			}

			if (untyped_source_id <= static_cast<int>(sources.size()))
			{
				sources[untyped_source_id - 1].reset();
				dropped = true;
			}
		}

		// KSS sources
		if (source_type == 1)
		{
			if (untyped_source_id <= static_cast<int>(kss_cartridges.size()))
			{
				kss_cartridges[untyped_source_id - 1].reset();
				dropped = true;
			}
		}
	}

	if (active)
		mixer->pause(false);

	return dropped;
}


/* ------------------- SAMPLES ------------------------ */

int MajimixPa::play_source(int source_handle, bool loop, bool paused)
{
	int source_id = get_source_id(source_handle);
	if(source_id > 0 && source_id <= static_cast<int>(sources.size()) && sources[source_id-1])
	{
		int pid = 0;
		for(auto& mix_channel : mixer_channels)
		{
			++pid;
			if(!mix_channel->active)
			{
				if(mix_channel->sid != source_id)
				{
					mix_channel->sid     = source_id;
					mix_channel->sample  = sources[source_id-1]->create_sample();
				}
				else
				{
					mix_channel->sample->seek(0);
				}
				mix_channel->stopped = false;
				mix_channel->loop    = loop;
				mix_channel->paused  = paused;
				mix_channel->active  = true;

				return get_handle(source_id, pid);
			}
		}
	}
	return 0;
}

int MajimixPa::play_kss_track(int kss_source_handle, int track, bool autostop, bool forcable, bool force)
{
	return kss_cartridge_action<int>(kss_source_handle, false, false, 0, [&](kss::CartridgeKSS &cartridge, int line_id) -> int {
		int id = cartridge.active_line(track, autostop, forcable);
		if(!id && force)
		{
			// no free line : we have to force
			bool need_reactive = mixer && mixer->is_active();
			if (need_reactive)
				mixer->pause(true);

			id = cartridge.force_line(track, autostop, forcable);

			if (need_reactive)
				mixer->pause(false);
		}

		if(id) 
		{
			// found a line : return the play_handle
			return get_handle(kss_source_handle, id);
		}
		return 0;
	});
}

bool MajimixPa::update_kss_track(int kss_handle, int new_track, bool autostop, bool forcable, int fade_out_ms)
{
	return kss_cartridge_action<bool>(kss_handle, true, true, false, [&new_track, &autostop, &forcable, &fade_out_ms](kss::CartridgeKSS &cartridge, int line_id) -> bool {
		return cartridge.update_line(line_id, new_track, autostop, forcable, fade_out_ms); 
	});
}

void MajimixPa::stop_playback(int play_handle)
{
	if (play_handle == 0)
	{
		// stop all

		// Channels
		for (auto& mix_channel : mixer_channels)
		{
			if (mix_channel->active)
			{
				mix_channel->stopped = true;
				mix_channel->paused = false;

				// TODO:  Please verify this !
				if (!m_stream) 
				{
					mix_channel->loop = false;   // XXX needed ?
					mix_channel->active = false; // XXX needed ?
				}
			}
		}

		// KSS
		for (auto& cartridge : kss_cartridges)
			if (cartridge)
				cartridge->stop_active();

	}
	else if (get_source_type(play_handle) == 1)
	{
		// KSS
		bool is_sample = get_channel_id(play_handle);
		kss_cartridge_action<bool>(play_handle, false, is_sample, false, [&is_sample](kss::CartridgeKSS &cartridge, int line_id) -> bool {
			
			if (is_sample)
				cartridge.stop(line_id);
			else
				cartridge.stop_active();

			return true;
		});
	}
	else
	{
		// Channels
		unsigned int source_id   = get_source_id(play_handle);
		unsigned int channel_id  = get_channel_id(play_handle);

		if (source_id)
		{
			if (channel_id)
			{
				auto &channel = mixer_channels[channel_id - 1];
				if (channel->active && static_cast<int>(source_id) == channel->sid)
				{
					channel->stopped = true;
					if(!m_stream) 
							channel->active = false;
				}
			}
			else
			{
				for (auto &channel : mixer_channels)
				{
					if (channel->active && static_cast<int>(source_id) == channel->sid)
					{
						channel->stopped = true;
						if(!m_stream) 
							channel->active = false;
					}
				}
			}
		}
	}
}

/* ---------------------- OTHERS ----------------------------- */


void MajimixPa::set_master_volume(int v)
{
	master_volume.store(v & 0xFF);

}

bool MajimixPa::update_kss_volume(int kss_handle, int volume)
{
	bool is_sample = get_channel_id(kss_handle);
	return kss_cartridge_action<bool>(kss_handle, true, is_sample, false, [&volume, &is_sample](kss::CartridgeKSS &cartridge, int line_id) -> bool {
		
		if(is_sample)
			cartridge.set_line_volume(line_id, volume);
		else
			cartridge.set_master_volume(volume);
		return true;

	});
}

void MajimixPa::pause_producer(bool pause) // test
{
	if(mixer)
		mixer->pause(pause);
}

void MajimixPa::set_loop(int play_handle, bool loop)
{
	unsigned int source_id   = get_source_id(play_handle);
	unsigned int channel_id  = get_channel_id(play_handle);
	if(source_id && channel_id)
	{
		mixer_channels[channel_id-1]->loop = loop;
	}
}


void MajimixPa::pause_resume_playback(int play_handle, bool pause)
{
	if (play_handle == 0)
	{
		// Pause/resume all samples (channels & KSS)

		// Channels
		for (auto &channel : mixer_channels)
			if (channel->active)
				channel->paused = pause;

		// KSS
		for (auto &cartridge : kss_cartridges)
			if (cartridge)
				cartridge->set_pause_active(pause);
	}
	else if (get_source_type(play_handle) == 1)
	{
		// KSS
		bool is_sample = get_channel_id(play_handle);
		bool bdone = kss_cartridge_action<bool>(play_handle, false, is_sample, false, [&pause, &is_sample](kss::CartridgeKSS &cartridge, int line_id) -> bool {
			if (is_sample)
				cartridge.set_pause(line_id, pause);
			else
				cartridge.set_pause_active(pause);
			return true;
		});
	}
	else
	{
		// Channels
		unsigned int source_id = get_source_id(play_handle);
		unsigned int channel_id = get_channel_id(play_handle);

		if (source_id)
		{
			if (channel_id)
			{
				auto &channel = mixer_channels[channel_id - 1];
				if (channel->active && static_cast<int>(source_id) == channel->sid)
					channel->paused = pause;
			}
			else
			{
				for (auto &channel : mixer_channels)
				{
					if (channel->active && static_cast<int>(source_id) == channel->sid)
						channel->paused = pause;
				}
			}
		}
	}
}


bool MajimixPa::create_stream() {
	// check no stream
	if(m_stream)
		return false;

//	int numDevices = Pa_GetDeviceCount();
//	const   PaDeviceInfo *deviceInfo;
//	for( int i=0; i<numDevices; ++i )
//	{
//	    deviceInfo = Pa_GetDeviceInfo( i );
//	    std::cout << "Device " << i << "\n";
//	    std::cout << deviceInfo->name << "\n";
//	    std::cout << "defaultHighOutputLatency "<< deviceInfo->defaultHighOutputLatency << "\n";
//	    std::cout << "defaultSampleRate "<< deviceInfo->defaultSampleRate << "\n";
//	}



	// initialize output parameters
	PaStreamParameters outputParameters;
	outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
	outputParameters.channelCount = channels;
	outputParameters.sampleFormat = bits == 24 ? paInt24 : paInt16;
	outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultHighOutputLatency; // Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
	outputParameters.hostApiSpecificStreamInfo = nullptr;
//	std::cout << "Pa_GetDeviceInfo( outputParameters.device )->defaultHighOutputLatency " << Pa_GetDeviceInfo( outputParameters.device )->defaultHighOutputLatency << "\n";
//	std::cout << "Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency " << Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency << "\n";
//	std::cout << "Pa_GetDeviceInfo( outputParameters.device )->maxOutputChannels " << Pa_GetDeviceInfo( outputParameters.device )->maxOutputChannels << "\n";
//	std::cout << "Pa_GetDeviceInfo( outputParameters.device )->defaultSampleRate " << Pa_GetDeviceInfo( outputParameters.device )->defaultSampleRate << "\n";
//	std::cout << "Pa_GetDeviceInfo( outputParameters.device )->name " << Pa_GetDeviceInfo( outputParameters.device )->name << "\n";

	// auto callback2 = std::bind(&MajimixPa::paCallback2, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6);

	// create stream
	PaError	err = Pa_OpenStream(
			&m_stream,
			nullptr, /* no input */
			&outputParameters,
			sampling_rate,
			paFramesPerBufferUnspecified, // <- best for PortAudio   
			paClipOff,      /* we won't output out of range samples so don't bother clipping them */
			&MajimixPa::paCallback,
			this);

	// check error
	if(err != paNoError) {
		std::cerr << "Error while creating portaudio stream - code "<<err<<std::endl;
		m_stream = nullptr;
	}

	return m_stream;
}

void MajimixPa::mix(std::vector<char>::iterator it_out, int requested_sample_count)
{
	//auto it_begin = it_out;
	int sample_count;
	bool deactivate;

	std::fill(internal_mix_buffer.begin(), internal_mix_buffer.end(), 0);

	for(auto& mix_channel : mixer_channels)
	{
		if(mix_channel->active)
		{
			sample_count = 0;
			deactivate =  false;
			if(mix_channel->stopped || !mix_channel->sample)
			{
				deactivate = true;
			}
			else if(!mix_channel->paused)
			{

				// TODO: stop using internal_sample_buffer but use directly internal_mix_buffer to avoid a copy ?

				sample_count = mix_channel->sample->read(&internal_sample_buffer[0], requested_sample_count);
				if(mix_channel->loop && sample_count < requested_sample_count)
				{
					while(sample_count < requested_sample_count)
					{
						// EOF - AUTOLOOP 
						long idx = static_cast<long>(sample_count) * channels;
						sample_count += mix_channel->sample->read(&internal_sample_buffer[0] + idx, requested_sample_count - sample_count);
					}
				}

				if(sample_count)
				{
					std::transform(internal_sample_buffer.begin(), internal_sample_buffer.begin() + static_cast<long>(sample_count) * channels, internal_mix_buffer.begin(), internal_mix_buffer.begin(), std::plus<int>());
				}
				if(sample_count < requested_sample_count)
				{
					deactivate = true;
				}
			}
			if(deactivate)
			{
				mix_channel->stopped = true;
				mix_channel->active = false;
			}
		}
	}

	// kss support

	for(auto &ck : kss_cartridges)
	{
		if(ck)
		{
			ck->read(internal_mix_buffer.begin(), requested_sample_count);
		}
	}

	// volume adjustment
	int vol = master_volume; // .load();
	std::for_each(internal_mix_buffer.begin(), internal_mix_buffer.end(), [&vol](int &n){ n = ((int_fast64_t) n * vol) >> 8; });

	encode(it_out);
}

void MajimixPa::read(char *out_buffer, int requested_sample_count)
{
	mixer->read(out_buffer, requested_sample_count);
}

template<int N>
void MajimixPa::encode_Nbits(std::vector<char>::iterator it_out)
{
	for(const auto &v : internal_mix_buffer)
	{
		*it_out++ = v & 0xFF;
		*it_out++ = (v >> 8) & 0xFF;
		if constexpr (N == 3)
			*it_out++ = (v >> 16) & 0xFF;
	}
}


/* This routine will be called by the PortAudio engine when audio is needed.
** It may called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
 */
int MajimixPa::paCallback(const void *input_buffer, void *output_buffer,
						   unsigned long frames_per_buffer,
						   const PaStreamCallbackTimeInfo* timeInfo,
						   PaStreamCallbackFlags statusFlags,
						   void *userData ) {
//	static_cast<MajimixPa*>(userData)->read((char *) output_buffer, frames_per_buffer);
	reinterpret_cast<MajimixPa*>(userData)->read((char *) output_buffer, frames_per_buffer);
	return paContinue;
}


/* --------------------------  KSS SUPPORT -------------------------- */

bool MajimixPa::get_cartrigde_and_line(int kss_handle, bool need_line, kss::CartridgeKSS *&cartridge, int &line_id)
{
	int idx;
	if(get_source_type(kss_handle) == 1 && (idx = get_untyped_source_id(kss_handle)))
	{
		--idx;
		if(static_cast<size_t>(idx) < kss_cartridges.size())
		{
			cartridge =  kss_cartridges[idx].get();
			if(cartridge)
			{
				line_id = get_channel_id(kss_handle);
				return !need_line || (line_id >0 && line_id <= cartridge->get_line_count());
			}
		}
	}
	return false;
}

template <typename T>
T MajimixPa::kss_cartridge_action(int kss_source_handle, bool need_sync, bool need_line, T default_ret_val, std::function<T(kss::CartridgeKSS &, int line_id)> fn_action)
{
	kss::CartridgeKSS *cartridge;
	int line_id;
	if (get_cartrigde_and_line(kss_source_handle, need_line, cartridge, line_id))
	{
		bool need_reactive = need_sync && mixer && mixer->is_active();
		if (need_reactive)
			mixer->pause(true);

		T ret_val = fn_action(*cartridge, line_id);

		if (need_reactive)
			mixer->pause(false);

		return ret_val;
	}
	return default_ret_val;
}

bool MajimixPa::update_kss_frequency(int kss_handle, int frequency)
{
	if(kss_handle)
	{
		bool is_sample = get_channel_id(kss_handle);
		return kss_cartridge_action<bool>(kss_handle, true, is_sample, false, [&frequency, &is_sample](kss::CartridgeKSS &cartridge, int line_id) -> bool {
			if(is_sample)					 			   
				cartridge.set_kss_line_frequency(line_id, frequency);
			else
				cartridge.set_kss_frequency(frequency);
			return true;

		});
	}

	bool need_reactive = mixer && mixer->is_active();
	if (need_reactive)
		mixer->pause(true);
	
	for(auto &c : kss_cartridges)
		if(c)
			c->set_kss_frequency(frequency);
	
	if (need_reactive)
		mixer->pause(false);
		
	return true;
}

int MajimixPa::get_kss_active_lines_count(int kss_source_handle)
{
	 return kss_cartridge_action<int>(kss_source_handle, false, false, 0, [](kss::CartridgeKSS& cartridge, int line_id) -> int 
	 {
		int nb = 0;
		for(auto &l : cartridge)
		{
			if(l->active)
				++nb;
		}
		return nb;
	});
}

int MajimixPa::get_kss_playtime_millis(int kss_play_handle) 
{
	return kss_cartridge_action<int>(kss_play_handle, false, true, 0, [](kss::CartridgeKSS& cartridge, int line_id) -> int 
	 {
		return cartridge.get_playtime_millis(line_id);
	});
}



/**
 *  create and return return MajimixPa mixer instance
 */
MAJIMIXAPI std::unique_ptr<Majimix> APIENTRY create_instance()
{
	return std::make_unique<MajimixPa>();
}

/**
 * Initialize PorAudio
 * Must be called before anay other majimix call
 */
MAJIMIXAPI void APIENTRY initialize()
{
	Pa_Initialize();
}

/**
 * PortAudio cleanup
 * This function deallocates all resources allocated by PortAudio since it was
 * initialized by a call to initialize_port_audio
 */
MAJIMIXAPI void APIENTRY terminate()
{
	Pa_Terminate();
}

} // namespace pa
} // namespace majimix
