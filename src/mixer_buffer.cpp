/**
 * @file mixer_buffer.cpp 
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
#include "mixer_buffer.hpp"

namespace majimix 
{

BufferedMixer::BufferedMixer(int32_t buffer_count, int32_t buffer_sample_size, int32_t sample_size)
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

int32_t BufferedMixer::get_buffer_count() const
{
	return buffer_packet_size ? buffer_total_size / buffer_packet_size : 0;
}

int32_t BufferedMixer::get_buffer_packet_size() const
{
	return buffer_packet_size;
}

int32_t BufferedMixer::get_buffer_packet_sample_size() const
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

}