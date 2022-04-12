/**
 * @file majimix.cpp
 *
 * @section majimix_desc_cpp DESCRIPTION
 *
 * This file contains the implementation of the Majimix mixer for the PortAudio library.
 *
 * @author  François Jacobs
 * @date 2022-04-12
 *
 * @section majimix_lic LICENSE
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
// #include <cstring>
#include <portaudio.h>
// #include <cassert>
// #include <vorbis/vorbisfile.h>
// #include <fstream>
// #include <functional>
// #include <atomic>
// #include <vector>
#include "kss.hpp"
#include "converters.hpp"
#include "interfaces.hpp"
#include "source_pcm.hpp"
#include "source_vorbis.hpp"
#include "mixer_buffer.hpp"
// #include <cstdint>


namespace majimix {
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

/**
 * @class MixerChannel
 * @brief
 *
 */
struct MixerChannel {
	std::atomic<bool> active;     // Set to true to activate the Channel. If PA thread is active, it is the only thread that can reset the value.
	std::atomic<bool> stopped;
	std::atomic<bool> paused;
	std::atomic<bool> loop;
	

	std::unique_ptr<Sample> sample;
	int sid; // FIXME atomic ! (cf stop_playback)
//	friend class MajimixPa;
// public:

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

namespace pa {


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
	std::vector<int32_t> internal_mix_buffer;
	std::vector<int32_t> internal_sample_buffer;

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
		if(rate >= 1000 && rate <= 96000 && (bits == 16 || bits == 24) )
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
#ifdef MAJIMIX_USE_FLOATING_POINT
		std::cout << "FLOATING POINT\n";
		auto s = std::make_unique<SourcePCMF>();
#else
		std::cout << "FIXED POINT\n";
		auto s = std::make_unique<SourcePCMI>();
#endif
		// FIXME: implementer totalement read 
		//if(load_wave(name, *s))
		if(s->load_wave(name))
			source = std::move(s);
	}
	else
	/* check Vorbis format */
	{
		auto s = std::make_unique<SourceVorbis>();
		if(s->set_file(name))
			source = std::move(s);

		// std::ifstream stream(name, std::ios::binary);
		// OggVorbis_File file;
		// int result = ov_test_callbacks(&stream, &file, nullptr, 0, {ogg_read, ogg_seek, nullptr, ogg_tell});
		// ov_clear(&file);
		// if(!result)
		// {
		// 	auto s = std::make_unique<SourceVorbis>();
		// 	s->set_file(name);
		// 	source = std::move(s);
		// }
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
		/*bool bdone = */ kss_cartridge_action<bool>(play_handle, false, is_sample, false, [&pause, &is_sample](kss::CartridgeKSS &cartridge, int line_id) -> bool {
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
