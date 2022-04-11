/**
 * @file mixer_buffer.hpp
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

#ifndef MIXER_BUFFER_HPP_
#define MIXER_BUFFER_HPP_

#include <atomic>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>

namespace majimix 
{

/*  ---------- BufferedMixer ----------
 * 
 * Allows to use a thread (independent from PA) dedicated 
 * to mixing: retrieving data from samples and merging it into an internal buffer
 * The mixing method has to be implemented elsewhere, BufferedMixer just
 * to provide a write buffer to store these data (it will claim n samples)
 *
 * PA can access the mixed audio data without blocking through the read method
 */

class BufferedMixer {
	/** Buffer "packet" size (size in byte) */
	const int32_t buffer_packet_size;
	/** Buffer "packet" sample size (size in sample) */
	const int32_t buffer_packet_sample_size;  
	/** Size of one sample in byte */
	const int32_t sample_size; 
	/** Total buffer size in byte */
	const int32_t buffer_total_size;

	std::vector<char> buffer; 

	std::atomic<int32_t> read_position;  
	int32_t read_inrange_index;           // read index within the buffer [0, buffer_packet_size]
	std::atomic<int32_t> write_position;
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
	BufferedMixer(int32_t buffer_count, int32_t buffer_sample_size, int32_t sample_size);
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


	int32_t get_buffer_count() const;
	int32_t get_buffer_packet_size() const;
	int32_t get_buffer_packet_sample_size() const;

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
}

#endif