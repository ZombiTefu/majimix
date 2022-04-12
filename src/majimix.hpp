/**
 * @file majimix.hpp
 *
 * @section majimix_desc_hpp DESCRIPTION
 *
 * Majimix is a high-level API designed to play simultaneously audio sources
 * (WAVE files, Xiph Ogg audio files, KSS files) as sounds or background musics.
 * 
 * This version of Majimix uses the following libraries:
 *  - PortAudio   (http://www.portaudio.com/)
 *  - Xiph libogg (https://xiph.org/ogg) 
 *  - libKss      (https://github.com/digital-sound-antiques/libkss) 
 *
 *
 * @author François Jacobs 
 * @date 2022-04-12
 * @version 0.5
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

/**
 * @mainpage 
 * 
 * @section intro_sec Introduction
 *
 * Majimix is a high-level API designed to play simultaneously audio sources
 * (WAVE files, Xiph Ogg audio files, KSS files) as sounds or background musics.
 * 
 * @subsection format_sub Currently supported File Formats
 * 
 * Majimix supports the following audio formats (mono or stereo only)
 *   - WAVE
 *   - Ogg
 *   - KSS
 * 
 * 
 * \section example_sec Code example
 * 
 * \code{.cpp}
 * #include <iostream>
 * #include <majimix/majimix.hpp>
 * 
 * int main()
 * {
 *     // initialize majimix
 *     majimix::pa::initialize();
 * 
 *     // create a majimix instance
 *     auto majimix_ptr = majimix::pa::create_instance();
 * 
 *     // set the majimix output format : rate 44,1 KHz stereo 16 bits
 *     // and use 10 channels : you can play 10 sounds simultaneously
 *     if (majimix_ptr->set_format(44100, true, 16, 10))
 *     {
 *         majimix_ptr->set_master_volume(50);
 *         // add source and get the source handle (it can be done later)
 *         int source_handle_1 = majimix_ptr->add_source("bgm.ogg");
 *         // ... add other sources ...
 * 
 *         // start majimix instance
 *         if (majimix_ptr->start_mixer())
 *         {
 *             // play bgm.ogg (loop)
 *             int play_handle_1 = majimix_ptr->play_source(source_handle_1, true, false);
 *             std::cout << play_handle_1 << "\n";
 * 
 *             // add a new source
 *             int source_handle_2 = majimix_ptr->add_source("sound.wav");
 *             std::cout << source_handle_2 << "\n";
 * 
 *             // press any key 'q' to quit
 *             char u = 0;
 *             while(u != 'q') 
 *             {
 *                 // play sound.wav (once) while bgm.ogg continu
 *                 int play_handle_2 = majimix_ptr->play_source(source_handle_2);
 *                 std::cout << "Press 'q' to quit\n";
 *                 std::cin >> u; 
 *             }
 * 
 *             // stop majimix
 *             majimix_ptr->stop_mixer();
 *         }
 *     }
 * 
 *     // dispose
 *     majimix::pa::terminate();
 *     return 0;
 * }
 * \endcode
 * 
 * 
 * 
 * \section install_sec Installation
 *
 * \subsection step1 Step 1: Opening the box
 *
 * 
 */




#ifndef MAJIMIX_PA_HPP_
#define MAJIMIX_PA_HPP_

#include <string>
#include <memory>


#ifdef _WIN32

  #ifdef MAJIMIX_EXPORTS
		#define MAJIMIXAPI __declspec(dllexport)
  #elif defined(MAJIMIX_IMPORTS)
		#define MAJIMIXAPI __declspec(dllimport)
  #else
		#define MAJIMIXAPI
  #endif

  #define APIENTRY __cdecl

#else

  #define MAJIMIXAPI
  #define APIENTRY

#endif


/*!
 * \namespace majimix
 */
namespace majimix {

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
	 * If this latency is suitable, you can keep this setting or even increase the size of the buffers, which should make processing lighter.
	 * If you need lower latency, you can increase the number of buffers and decrease their size.
	 * Example: With a sample rate of 44100 Hz, for a latency of 2 hundredths of a second and using 6 buffers the size will be : <tt>44100 * 2 / 100 / 6 = 147</tt>
	 * @code
	 * 			const int sample_rate  = 44100;
	 * 			const int latency_ms   = 20;
	 * 			const int buffer_count = 6
	 * 			const int buffer_size = sample_rate * latency_ms / 1000 / buffer_count;  // => 147
	 * @endcode
	 * 
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
	bool start_mixer();
	bool stop_mixer();
	

	/**
	 * @brief 
	 * 
	 * @param pause 
	 * @return true 
	 * @return false 
	 */
	virtual bool pause_resume_mixer(bool pause) = 0;
	bool pause_mixer();
	bool resume_mixer();

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
	 * @param [in] name The filename of the source.
	 * @return In case of success, an integer greater than 0, identifying the source (id or handle of the source), in case of failure 0.
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
	 * @param [in] source_handle The handle identifying the source.
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
	 * @param [in] source_handle The handle identifying the source.
	 * @param [in] loop If true plays the sound continuously. If set to false, plays the sound only once and releases the mixer channel used.
	 * @param [in] paused If this is the case, the mixer channel is paused (no sound) and waits for you to resume. If false, the sound is played immediately.
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
	 * @brief Update a specific kss line
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
	 * If play handle is a specific sample handle, stops the sample associated with this handle.
	 * If play_handle value is 0 : stops all active samples
	 *
	 * \remarks This also works for a kss handle (which is not the case for all methods).
	 *          When the handle represents a kss track, majimix will automatically call the dedicated method \c stop_kss
	 *
	 * @param [in] play_handle The sound handle (representing a source and a channel). This handle is the value returned by play_source.
	 *        If this handle does not match the source played on the track, nothing happens.
	 */
	virtual void stop_playback(int play_handle) = 0;


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
	void pause_playback(int play_handle);
	void resume_playback(int play_handle);


	/**
	 * @brief Set the master volume of the mixer/
	 *
	 * @param [in] v The volume value : from 0 (mute) to 255 (max)
	 */
	virtual void set_master_volume(int v) = 0;


	/**
	 * @fn void set_loop(int, bool)=0
	 * @brief Changes the sound loop mode.
	 *
	 * @param [in] play_handle sound handle.
	 * @param [in] loop loop mode : if true the sound will be played continuously, if false the sound the sound will automatically stop at the end and the channel will be released.
	 */
	virtual void set_loop(int play_handle, bool loop) = 0;


	/**
	 * @brief Update volume
	 *
	 * Update volume for a specific line of a kss source of for all lines of a kss source.
	 *
	 * @param [in] kss_handle A kss source handle or a kss track handle.
	 * @param [in] volume Volume value between 0 and 100
	 * @return True if successful / False for an invalid \c kss_track_handle.
	 */
	virtual bool update_kss_volume(int kss_handle, int volume) = 0;

	virtual bool update_kss_frequency(int kss_handle, int frequency) = 0;
	
	
	virtual int get_kss_active_lines_count(int kss_source_handle) = 0;


	virtual int get_kss_playtime_millis(int kss_play_handle) = 0;


    // TODO: update_volume - (not only kss version)
	// TODO:  bool is_active(int handle) - active / paused (source / channel/track) kss compatible
	// TODO  bool is_paused(int play_handle);
	// TODO:  activity_callback()
	// TODO:  effects - fade transition ...

};
}



#ifdef __cplusplus
extern "C"
{
#endif

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


}

#ifdef __cplusplus
} // __cplusplus defined.
#endif


#endif /* MAJIMIX_HPP_ */

