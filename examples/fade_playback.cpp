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

	auto master_gain = std::make_shared<majimix::GainEffect>(0.85f);
	auto fade = std::make_shared<majimix::FadeEffect>(0.0f);

	majimix_ptr->add_master_effect(master_gain);

	const int play_handle = majimix_ptr->play_source(source_handle, false, true);
	if(!play_handle)
	{
		std::cerr << "unable to start playback\n";
		majimix_ptr->stop_mixer();
		majimix::pa::terminate();
		return 1;
	}

	if(majimix_ptr->add_playback_effect(play_handle, fade) == majimix::InvalidEffectHandle)
	{
		std::cerr << "unable to attach fade effect\n";
		majimix_ptr->stop_playback(play_handle);
		majimix_ptr->stop_mixer();
		majimix::pa::terminate();
		return 1;
	}

	std::cout << "fade in...\n";
	fade->fade_in(800);
	majimix_ptr->resume_playback(play_handle);
	std::this_thread::sleep_for(std::chrono::seconds(4));

	std::cout << "fade out...\n";
	fade->fade_out(1200);
	std::this_thread::sleep_for(std::chrono::milliseconds(1200));

	majimix_ptr->stop_playback(play_handle);
	majimix_ptr->stop_mixer();
	majimix::pa::terminate();
	return 0;
}