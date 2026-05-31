#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include <majimix/majimix.hpp>
#include <majimix/effects.hpp>

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

	auto chorus = std::make_shared<majimix::ChorusEffect>(18.0f, 3.5f, 0.28f, 0.4f, 0.12f, 120.0f);
	const int play_handle = majimix_ptr->play_source(source_handle, true, false);
	if(!play_handle)
	{
		std::cerr << "unable to start playback\n";
		majimix_ptr->stop_mixer();
		majimix::pa::terminate();
		return 1;
	}

	std::cout << "playback dry for 4 seconds...\n";
	std::this_thread::sleep_for(std::chrono::seconds(4));

	if(majimix_ptr->add_playback_effect(play_handle, chorus) == majimix::InvalidEffectHandle)
	{
		std::cerr << "unable to attach chorus effect\n";
		majimix_ptr->stop_playback(play_handle);
		majimix_ptr->stop_mixer();
		majimix::pa::terminate();
		return 1;
	}

	std::cout << "chorus running...\n";
	std::this_thread::sleep_for(std::chrono::seconds(4));

	std::cout << "wider stereo modulation...\n";
	chorus->set_stereo_phase_deg(180.0f);
	chorus->set_depth_ms(4.5f);
	std::this_thread::sleep_for(std::chrono::seconds(4));

	std::cout << "slightly more feedback and mix...\n";
	chorus->set_feedback(0.18f);
	chorus->set_mix(0.5f);
	chorus->set_rate_hz(0.35f);
	std::this_thread::sleep_for(std::chrono::seconds(6));

	majimix_ptr->stop_playback(play_handle);
	majimix_ptr->stop_mixer();
	majimix::pa::terminate();
	return 0;
}