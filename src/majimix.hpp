/**
 * @file majimix.hpp
 *
 * @section majimix_desc_hpp DESCRIPTION
 *
 * Simple audio mixer to play sound sources (WAVE and Vorbis files)<br>
 * The mixer must first be initialized: rate, channels, type (16 or 24 bits) and the number of simultaneously playable channels.<br>
 * Sources can then be added (WAVE or Vorbis files).<br>
 * The mixer is then able to play these sources simultaneously (sound, background music, loop).<br>
 * This version uses the PortAudio and Xiph VorbisFile libraries.<br>
 *
 * Support for KSS files is also available (KSS files are sound files ripped from MSX games and other ancient)<br>
 * Unlike the classic sources (WAVE and Vorbis), the use of KSS sources is done through dedicated functions. It is of course possible<br>
 * to simultaneously play (mix) KSS tracks together with WAVE and Ogg.
 *
 *
 * @author  François Jacobs
 * @date 13/02/2022
 * @version 
 *
 * @section majimix_lic_cpp LICENSE
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
 *
 *
 *
 */



#ifndef MAJIMIX_PA_HPP_
#define MAJIMIX_PA_HPP_

#include <string>
#include <memory>


#ifdef _WIN32

  /* You should define ADD_EXPORTS *only* when building the DLL. */
  #ifdef MAJIMIX_EXPORTS
		#define MAJIMIXAPI __declspec(dllexport)
  #elif defined(MAJIMIX_IMPORTS)
		#define MAJIMIXAPI __declspec(dllimport)
  #else
		#define MAJIMIXAPI
  #endif

  /* Define calling convention in one place, for convenience. */
  #define APIENTRY __cdecl

#else
  /* Define with no value on non-Windows OSes. */
  #define MAJIMIXAPI
  #define APIENTRY

#endif



/*!
 * \namespace majimix
 *
 *	<b>Usage example :</b>
 *
 * \code{.cpp}
 *		// initialize majimix
 *		majimix::pa::initialize();
 *
 *		// create a majimix instance
 *		auto majimix_ptr = majimix::pa::create_instance();
 *
 *		// set the majimix output format : rate 44,1 KHz Stéreo 16 bits
 *		// and use 3 channels : you can play 3 sounds simultaneously
 *		if(majimix_ptr->set_format( 44100, true, 16, 3))
 *		{
 *			// You can also configure the internal buffers here - this is optional and allows to manage the latency
 *			// In this example: 3 buffers of 4410 samples - for a latency of 3 tenths of a second
 *			majimix_ptr->set_mixer_buffer_parameters(3, 4410);
 *			// ... to reduce latency, we could use 6 buffers of 147 samples for a latency of 2 hundredths of a second (see set_mixer_buffer_parameters for more informations).
 *
 *			// add source and get the source handle
 *			int source_handle = majimix_ptr->add_source("my_sound.ogg");
 *
 *			// ... add other sources ...
 *
 *			// start majimix instance
 *			if(majimix_ptr->start())
 *			{
 *				// play a sound : my_sound.ogg
 *				int play_handle = majimix_ptr->play_source(source_handle, true, false);
 *
 *				// ... app loop ...
 *
 *				// stop majimix
 *				majimix_ptr->stop();
 *			}
 *		}
 *		// dispose
 *		majimix::pa::terminate();
 * \endcode
 *
 */
namespace majimix {

/**
 * @class Majimix
 * @brief The Majimix mixer interface
 *
 *
 *  <b>Role</b><br>
 *  <br>
 *  	This interface allows to play simultaneously sounds or musics coming from different sources (WAVE, OGG and KSS since version 0.5).<br>
 *  	You can for example play a music and add sound effects while the music is playing<br>
 *  <br>
 *  <b>Initialization</b><br>
 *  <br>
 *     Before you can play sounds/music, you have to add sound sources to Majimix via the method \c add_source.<br>
 *     With this method you can play WAVE or Vorbis ogg files (it is also possible to use KSS files with the method add_source_kss but we will see this in a second step).<br>
 *     Majimix accepts sources with different formats:
 *     <ul>
 *     <li>rate, [44100, 22500, 48000 ...]</li>
 *     <li>mono/stereo</li>
 *     <li>format</li>
 *     <ul>
 *     <li> unsigned int 8 bits</li>
 *     <li>signed int 16 bits</li>
 *     <li>signed int 24 bits</li>
 *     <li>signed 32 bits</li>
 *     <li>signed 64 bits</li>
 *     <li>float 32 bits IEEE</li>
 *     <li>float 64 bits IEEE.</li>
 *     </ul>
 *     </ul>
 *
 *     <br>
 *     An output format must be defined:<br>
 *     <ul>
 *     <li>rate : [8000, 96000] Hz</li>
 *     <li>channel : mono or stereo (no more at the moment)</li>
 *     <li>sample : 16 or 24 bits</li>
 *     <li>number of simultaneously playable voices : ≥ 0</li>
 *     </ul>
 *     <br>
 *     Majimix must then be started using the \c start method.<br>
 *     <br>
 *     You can then play the sounds or music using the \c play_source method.<br>
 *
 *
 *
 *
 *
 */

constexpr int MixerError   = -1;
constexpr int MixerStopped =  0;
constexpr int MixerPaused  =  1;
constexpr int MixerRunning =  2;



class Majimix {

public:
	virtual ~Majimix() = default;

	/* ---------------- CONFIGURATION -------------------*/

	/**
	 *
	 * @brief Set the Majimix mixer format
	 *
	 * \warning This method can only be called up when the mixer is stopped or not yet started.
	 *
	 * @param rate Number of sample per seconds
	 * @param stereo Mono / Stereo (Majimix doesn't support more than 2 channels.)
	 * @param bits   Output format - must be 16 or 24
	 * @param channel_count Number of mixer channels. This is the maximum number of samples that can be played simultaneously.
	 *
	 * @return True if the parameters are valid and false if not.
	 */
	virtual bool set_format(int rate, bool stereo = true, int bits = 16, int channel_count = 6) = 0;

	/**
	 * @brief Set internal mixer buffers parameters
	 *
	 * Internal memory buffers are used by majimix to perform audio mixing.
	 * A dedicated thread mixes the audio data and places it into the buffers, while another thread retrieves the audio data from the buffers and sends it to the speakers.
	 * The default setting is: 5 buffers size (in number of samples) and latency of 100 ms (high latency).
	 * If this latency is not a problem, you can keep this setting or even increase the size of the buffers, which should make processing lighter.
	 * If you need lower latency, you can increase the number of buffers and decrease their size.
	 * Example: With a sample rate of 44100 Hz, for a latency of 2 hundredths of a second and using 6 buffers the size will be : <tt>44100 * 2 / 100 / 6 = 147</tt>
	 * @code
	 * 			const int sample_rate  = 44100;
	 * 			const int latency_ms   = 20;
	 * 			const int buffer_count = 6
	 * 			const int buffer_size = sample_rate * latency_ms / 1000 / buffer_count;  // => 147
	 * @endcode
	 * 
	 * To sum up :
	 * If low latency is not necessary: 3 buffers of size greater than or equal to the spleen.
	 * If low latency is necessary: try to increase the number of buffers and decrease their size.
	 * 
	 * @warning This method can only be called up when the mixer is stopped or not yet started.
	 *
	 *
	 * @param[in] count Number of buffers
	 * @param[in] buffer_sample_size The size of each buffer.
	 * @return True if the setting has been correctly taken into account.
	 */
	virtual bool set_mixer_buffer_parameters(int count, int buffer_sample_size) = 0;
	// virtual bool set_mixer_buffer_default_parameters();




	/* ---------------- START/PAUSE/STOP the Mixer -------------------*/

	/**
	 * @brief Starts or stops the mixer
	 * 
	 * @note These actions can be "heavy" and consume time and resources.
	 * 
	 * @param start true : starts the mixer, false : stops the mixer
	 * @return true The command was performed successfully. 
	 * @return false A problem occurred and the operation could not be completed.
	 */
	virtual bool start_stop_mixer(bool start) = 0;

	/**
	 * @brief 
	 * 
	 * @param pause 
	 * @return true 
	 * @return false 
	 */
	virtual bool pause_resume_mixer(bool pause) = 0;

	/**
	 * @brief Get the mixer status
	 * 
	 * @return int 
	 * @return MixerError An error occurs, status is undefined
	 * @return MixerStopped
	 * @return MixerPaused
	 * @return MixerRunning
	 * 
	 */
	virtual int get_mixer_status() = 0;

	/**
	 * @fn bool start()=0
	 * @brief Starts Majimix mixer (audio thread).
	 * @deprecated replaced by start_stop_mixer
	 * @return true if the mixer has started.
	 */
	[[deprecated("see start_stop_mixer")]]
	virtual bool start() = 0;

	/**
	 * @fn void stop()=0
	 * @brief Stops the mixer (audio thread).
	 * @deprecated replaced by start_stop_mixer
	 */
	[[deprecated("see start_stop_mixer")]]
	virtual void stop() = 0;

	/**
	 * @brief Pauses or resumes the mixer (audio thread).
	 * 
	 * @deprecated replaced by pause_resume_mixer
	 *
	 * @param paused Pauses or resumes the mixer.
	 */
	[[deprecated("see pause_resume_mixer")]]
	virtual void pause(bool paused) = 0;



	/* ---------------- ADDING / REMOVING SOURCES -------------------*/

	/**
	 *
	 * @brief Add a source to the mixer.
	 * 
	 * Add a source to the mixer. This source can be a wave file or a Vorbis file.
	 * The mixer doesn't have to be stopped or paused.
	 * 
	 * @attention Not compatible with kss file.
	 * 
	 *
	 * @param name The filename of the source.
	 * @return In case of success, an integer strictly greater than 0, identifying the source (id or handle of the source), in case of failure 0.
	 */
	virtual int add_source(const std::string& name) = 0;

	/**
	 * @brief KSS support for majimix
	 * Add a KSS type source to the mixer
	 *
	 * @param [in] name kss file name
	 * @param [in] lines Number of lines (voices) managed by this source
	 * @param [in] silent_limit_ms for autostop option
	 * @return A kss source handle
	 */
	virtual int add_source_kss(const std::string& name, int lines, int silent_limit_ms = 500) = 0;

	/**
	 *
	 * @brief Stops all samples created by this source and removes the source from the mixer.
	 * 
	 * Should be invoked to suppress a source of any kind (wav ogg kss)
	 * 
	 * @param source_handle The handle identifying the source.
	 * @return
	 */
	virtual bool drop_source(int source_handle) = 0;


	
	/* ---------------- PLAY / PAUSE / STOP SOURCES -------------------*/

	/**
	 * @brief Plays the sound of a source on the mixer.
	 *
	 * Associates a new free channel from the mixer to the source and plays the source
	 * on that track. A source may of course be played on several channels at the same time.
	 * If there are no channels available, nothing happens and the method returns 0.
	 * 
	 * @attention Incompatible with kss
	 *
	 * @param source_handle The handle identifying the source.
	 * @param loop If true plays the sound continuously. If set to false, plays the sound only once and releases the mixer channel used.
	 * @param paused If this is the case, the mixer channel is paused (no sound) and waits for you to resume. If false, the sound is played immediately.
	 * @return A sample (a sound associated to a mixer channel) handle or 0 if there are no channels available on the mixer.
	 */
	virtual int play_source(int source_handle, bool loop = false, bool paused = false) = 0;

	/**
	 * @brief Play a kss track
	 *
	 * @param [in] kss_source_handle The kss handle returned by add_source_kss
	 * @param [in] track The kss track number to play [1, 255]
	 * @param [in] autostop Automatic detection of the end of the track (the dedicated line will be automatically deactivated at the end of the track). When false, prevents forced deactivation.
	 * @param [in] force When true and if no line is available, deactivate the oldest line (autostop true and pause false) and take it for playing the track.
	 * @return A kss track handle or 0 if fails (invalid \c kss_source_handle, no line available)
	 */
	virtual int play_kss_track(int kss_source_handle, int track, bool autostop = true, bool forcable = true, bool force = true) = 0;

	/**
	 * @brief Update a spécific kss line
	 * Can be use to change the current track of the line.
	 *
	 * @param [in] kss_track_handle The kss handle returned by play_kss_track
	 * @param [in] new_track The new track to play
	 * @param [in] autostop Automatic detection of the end of the track (the dedicated line will be automatically deactivated at the end of the track). When false, prevents forced deactivation.
	 * @param [in] fade_out_ms Fading time in milliseconds between the current playing track and \c new_track
	 * @return True if successful / False for an invalid \c kss_track_handle.
	 */
	virtual bool update_kss_track(int kss_track_handle, int new_track, bool autostop = true, bool forcable = true, int fade_out_ms = 0) = 0;


	/**
	 * @brief Stops a currently playing sound on a specific channel.
	 * 
	 * Stops a sound and releases the associated channel.
	 * If play_handle is a source_handle, stops every samples associated with this source.
	 * If play handle is a specific sample handle (play handle /sample handle), stops the sample associated with this handle.
	 * If play_handle value is 0 : stops all avtive samples
	 *
	 * \remarks This also works for a kss handle (which is not the case for all methods).
	 *          When the handle represents a kss track, majimix will automatically call the dedicated method \c stop_kss
	 *
	 *
	 * @param play_handle The sound handle (representing a source and a channel). This handle is the value returned by play_source.
	 *        If this handle does not match the source played on the track, nothing happens.
	 */
	virtual void stop_playback(int play_handle) = 0;

	[[deprecated("replaced by stop_playback")]]
	virtual bool stop_kss(int kss_handle) = 0;
	
	/**
	 *
	 * @brief Stops all samples created by this source.
	 *
	 * @param source_handle The source handle (value returned by add_source).
	 * 
	 * @deprecated use stop_playback with the source handle instead.
	 */
	[[deprecated("replaced by stop_playback")]]
	virtual void stop_source(int source_handle) = 0;

	/**
	 * @fn void stop_all_playback()=0
	 * @brief Stops all sounds (all channels, but leave the mixer active).
	 *
	 */
	[[deprecated("replaced by stop_playback")]]
	virtual void stop_all_playback() = 0;



	/**
	 * @brief Pause or resume samples
	 *  
	 * @param play_handle 0 pause / resume all samples
	 * @param play_handle source_handle pause / resume all samples of the source
	 * @param play_handle sample_handle pause / resume one sample
	 * 
	 * @param pause 
	 */
	virtual void pause_resume_playback(int play_handle, bool pause) = 0;


	[[deprecated("replaced by pause_resume_playback")]]
	virtual void set_pause(int play_handle, bool pause) = 0;
	[[deprecated("replaced by pause_resume_playback")]]
	virtual bool set_pause_kss(int kss_handle, bool pause) = 0;


	/**
	 * @brief Set the master volume of the mixer/
	 *
	 * @param v The volume value : from 0 (mute) to 255 (max)
	 */
	virtual void set_master_volume(int v) = 0;

	/**
	 * @fn void set_loop(int, bool)=0
	 * @brief Changes the sound loop mode.
	 *
	 * @param play_handle The sound handle.
	 * @param loop The loop mode : if true the sound will be played continuously, if false the sound the sound will automatically stop at the end and the channel will be released.
	 */
	virtual void set_loop(int play_handle, bool loop) = 0;


	/**
	 * @brief Update volume
	 *
	 * Update volume for a spécific line of a kss source of for all lines of a kss source.
	 *
	 * @param [in] kss_handle A kss source handle or a kss track handle.
	 * @param [in] volume Volume value between 0 and 100
	 * @return True if successful / False for an invalid \c kss_track_handle.
	 */
	virtual bool update_kss_volume(int kss_handle, int volume) = 0;
	virtual bool update_kss_frequency(int kss_handle, int frequency) = 0;
	
	// virtual void set_pause_active(bool pause);
	
	// virtual void stop_active();
	virtual int get_kss_active_lines_count(int kss_source_handle) = 0;


	virtual int get_kss_playtime_millis(int kss_play_handle) = 0;


};

}



#ifdef __cplusplus
extern "C"
{
#endif

// Exported variables. 

namespace majimix {
namespace pa {

/**
 * @fn void initialize()
 * @brief Initialize PortAudio. Makes a call to Pa_Initialize PortAudio function.
 *
 * Must be called before any access to the mixer or PortAudio functions.
 *
 */
MAJIMIXAPI void APIENTRY initialize();


/**
 * @fn void terminate()
 * @brief Makes a call to Pa_Terminate PortAudio function.
 *
 */
MAJIMIXAPI void APIENTRY terminate();


/**
 * @fn std::unique_ptr<Majimix> create_instance()
 * @brief Creates an instance of the Majimix mixer based on the portaudio library.
 *
 * @return
 */
MAJIMIXAPI std::unique_ptr<Majimix> APIENTRY create_instance();

}
MAJIMIXAPI bool APIENTRY is_valid_kss_file(const std::string &filename);
}

#ifdef __cplusplus
} // __cplusplus defined.
#endif


#endif /* MAJIMIX_HPP_ */

