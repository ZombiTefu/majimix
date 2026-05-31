#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include <majimix/effects.hpp>
#include <majimix/majimix.hpp>

int main(int argc, char **argv)
{
	if(argc < 2)
	{
		std::cerr << "usage: " << argv[0] << " <audio-file>\n";
		return 1;
	}

	const char *source_path = argv[1];

	majimix::pa::initialize();

	auto majimix_ptr = majimix::pa::create_instance();
	if(!majimix_ptr || !majimix_ptr->set_format(44100, true, 16, 8))
	{
		std::cerr << "unable to initialize the mixer\n";
		majimix::pa::terminate();
		return 1;
	}

	const int source_handle = majimix_ptr->add_source(source_path);
	if(!source_handle)
	{
		std::cerr << "unable to load " << source_path << "\n";
		majimix::pa::terminate();
		return 1;
	}

	if(!majimix_ptr->start_mixer())
	{
		std::cerr << "unable to start the mixer\n";
		majimix::pa::terminate();
		return 1;
	}

	auto vibrato = std::make_shared<majimix::VibratoEffect>(2.0f, 1.f, 4.0f, 90.0f);
	const int play_handle = majimix_ptr->play_source(source_handle, true, false);
	if(!play_handle)
	{
		std::cerr << "unable to start playback\n";
		majimix_ptr->stop_mixer();
		majimix::pa::terminate();
		return 1;
	}

	std::cout << "playback dry for 3 seconds...\n";
	std::this_thread::sleep_for(std::chrono::seconds(3));

	if(majimix_ptr->add_playback_effect(play_handle, vibrato) == majimix::InvalidEffectHandle)
	{
		std::cerr << "unable to attach vibrato effect\n";
		majimix_ptr->stop_playback(play_handle);
		majimix_ptr->stop_mixer();
		majimix::pa::terminate();
		return 1;
	}

	std::cout << "subtle vibrato running...\n";
	std::this_thread::sleep_for(std::chrono::seconds(4));

	std::cout << "deeper stereo vibrato...\n";
	vibrato->set_depth_ms(3.0f);
	vibrato->set_stereo_phase_deg(180.0f);
	std::this_thread::sleep_for(std::chrono::seconds(4));

	std::cout << "faster vibrato...\n";
	vibrato->set_rate_hz(6.0f);
	vibrato->set_delay_ms(6.5f);
	std::this_thread::sleep_for(std::chrono::seconds(5));

	majimix_ptr->stop_playback(play_handle);
	majimix_ptr->stop_mixer();
	majimix::pa::terminate();
	return 0;
}