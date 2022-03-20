/**
 * kss.hpp
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

#ifndef KSS_HPP_
#define KSS_HPP_

#include "kssplay.h"
#include <cstring>
#include <vector>
#include <string>
#include <atomic>
#include <memory>
#include <functional>


/**
 * \namespace majimix::kss
 * \brief Majimix KSS support
 * \details KSS files are audio data from old computers.
 * 			The classes of this package allow to read these files using the libkss library (https://github.com/digital-sound-antiques/libkss).
 * 			Unlike standard sources (WAVE or Vorbis OGG), the processing of KSS sources is done using a dedicated class: CartridgeKSS.
 *
 */
namespace majimix::kss {

KSS *load_kss(const std::string & filename);
// void kss_deleter(KSS* kss);
// void kssplay_deleter(KSSPLAY* kssplay);

/**
 * @brief KSSLine represents a voice (line) in which a track (music or sound) is associated.
 * @details KSSLine - triplet {KSS, KSSPLAY, track number} to extract 16-bit PCM audio from a track in the KSS file
 *          The PCM audio data of the track can be recovered by the \a read method of the \a CartridgeKSS class.
 */
struct KSSLine
{
	/** @brief Activation id of the \a line. */
	int id;
	
	/** @brief the KSS pointer (c.f. libkss) */
	std::unique_ptr<KSS, decltype(&KSS_delete)> kss_ptr;
	// std::unique_ptr<KSS, decltype(&kss_deleter)> kss_ptr;
	
	/** @brief the KSSPLAY pointer (c.f. libkss) */
	std::unique_ptr<KSSPLAY, decltype(&KSSPLAY_delete)> kssplay_ptr;
	// std::unique_ptr<KSSPLAY, decltype(&kssplay_deleter)> kssplay_ptr;
	
	/** @brief Activation state of the \a line. */
	std::atomic_bool active;
	
	/** @brief pause flag */
	std::atomic_bool pause;
	
	/** Automatic line stop indicator - the line is automatically deactivated when there is no more data to extract */
	std::atomic_bool autostop;
	
	/** indicate if the active line can be forced - track replacement on active line */
	bool forcable;
	
	/** kss track number */
	uint8_t current_track;
	
	/** */
	int32_t transition_fadeout;
	
	/** */
	uint8_t next_track;

	KSSLine()
		: id{0},
		//   kss_ptr{nullptr, &kss_deleter},
		//   kssplay_ptr{nullptr, &kssplay_deleter},
		  kss_ptr{nullptr, &KSS_delete},
		  kssplay_ptr{nullptr, &KSSPLAY_delete},
		  active{false},
		  pause{false},
		  forcable{true},
		  autostop{false},
		  current_track{0},
		  transition_fadeout{0},
		  next_track{0}
	{
	}

	/**
	 * @brief Assigning / replacing the KSS pointer to the line
	 * @warning KSSLine takes the ownership of the KSS pointer.
	 *          The caller <b>must not</b> perform a delete on this pointer.
	 *
	 * @param kss
	 */
	void set_kss(KSS *kss);

	/**
	 * @brief Assigning / replacing the KSSPLAY pointer to the line
	 * @warning KSSLine takes the ownership of the KSSPLAY pointer.
	 *          The caller <b>must not</b> perform a delete on this pointer.
	 *
	 * @param kssplay
	 */
	void set_kssplay(KSSPLAY *kssplay);
};

/**
 * @brief 
 * 
 */
class CartridgeKSS {
	// KSS output format
	constexpr static uint8_t m_kss_bits = 16;
	constexpr static uint32_t m_kss_cpu_speed = 0; //  0:auto n:[1..8]

	/** Number of lines of the cartridge */
	uint8_t m_lines_count;

	/** sample rate */
	uint32_t m_rate;

	/** 1 mono 2 stereo */
	uint8_t m_channels;

	// output format 16 / 24 
	uint8_t m_bits;

	// silence duration
	unsigned int m_silent_limit_ms;

	int m_next_line_id;
	int m_master_volume;
	std::vector<std::unique_ptr<KSSLine>> m_lines;
	std::vector<int16_t> m_lines_buffer;

	template<int N, bool ADD>
	int read_line_convert(std::vector<int>::iterator it_out, KSSLine &line, int requested_sample_count);

	template<int N, bool ADD>
	int read_lines_convert(std::vector<int>::iterator it_out, int requested_sample_count);

	using fn_read_line = std::function<int(CartridgeKSS *c,std::vector<int>::iterator it_out,KSSLine &line, int requested_sample_count)>;
	fn_read_line read_line;

	using fn_read_lines = std::function<int(CartridgeKSS *c, std::vector<int>::iterator it_out, int requested_sample_count)>;
	fn_read_lines read_lines;

	KSS* create_copy();
	void init_line(KSS *kss_ref, KSSLine &line);
	void activate(KSSLine &line, uint8_t track, bool autostop, bool forcable = true, int fadeout_ms = 0);
	void set_kss_line_frequency(KSSLine *l, int frequency);

	int read(std::vector<int>::iterator it_out, KSSLine &line, int requested_sample_count);

public:
	CartridgeKSS(KSS* kss, int nb_lines = 1, int rate = 44100, int channels = 2, int bits = 16, int silent_limit_ms = 500);
	bool set_output_format(int samples_per_sec, int channels, int bits/*, int silent_limit_ms*/);
	bool set_lines_count(int nb_lines);
	int get_line_count() const;
	int read(std::vector<int>::iterator it_out,  int requested_sample_count);

	std::vector<std::unique_ptr<KSSLine>>::iterator begin();
	std::vector<std::unique_ptr<KSSLine>>::iterator end();


	/**
	 * @brief Activation of a line (thread safe)
	 *
	 * @param track the song number to play
	 * @param autostop true : automatic disabling of the line when the track (sound playback) is finished.
	 * @param forcable the line can be activated with another track if no other line is available (inactive)
	 * @return the index (1 based) of the activated \c line or 0 if no \c line could be activated.
	 */
	int active_line(int track, bool autostop = true, bool forcable = true);

	/**
	 * @brief Force the activation of a \c line.
	 *
	 * Only the forcable lines are searched
	 *
	 * @warning Not thread safe : it is necessary to synchronize the mixer with this call - pausing the mixer then resuming after the call for example
	 *
	 * @param track The soundtrack to be associated with the \e line.
	 * @param autostop Automatic disabling of the line when the track (sound playback) is finished.
	 * @param forcable the line can be activated with another track if no other line is available (inactive)
	 * @return the index (1 based) of the activated \c line or 0 if no \c line could be activated.
	 */
	int force_line(int track, bool autostop = true, bool forcable = true);

	/**
	 * Update a line identified by line_id
	 *
	 * @warning Not thread safe : it is necessary to synchronize the mixer with this call - pausing the mixer then resuming after the call for example
	 *
	 * @param line_id 1 based line index
	 * @param new_track
	 * @param autostop
	 * @param fade_out_ms
	 * @return
	 */
	bool update_line(int line_id, int new_track, bool autostop = true, bool forcable = true, int fade_out_ms = 0);

	/**
	 * @brief Pause / Resume a specific line
	 *
	 * @param line_id
	 * @param pause
	 * @return
	 */
	void set_pause(int line_id, bool pause);
	void set_pause_active(bool pause);
	void stop(int line_id);
	void stop_active();


    /** master volume control (volume for all lines of CartdrigeKSS) */
	void set_master_volume(int volume);
	
	/** line volume control  (volume for a specific line) */
	void set_line_volume(int line_id, int volume);

	/** frequencies control */
	void set_kss_frequency(int frequency);

	/** control of the frequency of a line */
	void set_kss_line_frequency(int line_id, int frequency);

	/** playing time of a line for a track*/
	int get_playtime_millis(int line_id);
};
}




#endif /* KSS_HPP_ */
