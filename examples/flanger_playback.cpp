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

	auto flanger = std::make_shared<majimix::FlangerEffect>(2.0f, 1.5f, 0.18f, 0.55f, 0.72f, 90.0f);
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

	if(majimix_ptr->add_playback_effect(play_handle, flanger) == majimix::InvalidEffectHandle)
	{
		std::cerr << "unable to attach flanger effect\n";
		majimix_ptr->stop_playback(play_handle);
		majimix_ptr->stop_mixer();
		majimix::pa::terminate();
		return 1;
	}

	std::cout << "classic flanger running...\n";
	std::this_thread::sleep_for(std::chrono::seconds(4));

	std::cout << "stronger jet sweep...\n";
	flanger->set_depth_ms(2.4f);
	flanger->set_mix(0.7f);
	flanger->set_feedback(0.82f);
	std::this_thread::sleep_for(std::chrono::seconds(4));

	std::cout << "negative feedback character...\n";
	flanger->set_feedback(-0.78f);
	flanger->set_rate_hz(0.3f);
	flanger->set_stereo_phase_deg(180.0f);
	std::this_thread::sleep_for(std::chrono::seconds(5));

	majimix_ptr->stop_playback(play_handle);
	majimix_ptr->stop_mixer();
	majimix::pa::terminate();
	return 0;
}