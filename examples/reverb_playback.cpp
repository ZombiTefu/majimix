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

	auto reverb = std::make_shared<majimix::ReverbEffect>(0.55f, 0.2f, 0.0f, 1.0f, 1.0f);
	if(majimix_ptr->add_master_effect(reverb) == majimix::InvalidEffectHandle)
	{
		std::cerr << "unable to attach reverb effect\n";
		majimix_ptr->stop_mixer();
		majimix::pa::terminate();
		return 1;
	}

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

	std::cout << "small room reverb...\n";
	reverb->set_wet(0.18f);
	reverb->set_room_size(0.55f);
	std::this_thread::sleep_for(std::chrono::seconds(4));

	std::cout << "larger brighter room...\n";
	reverb->set_room_size(0.78f); 
	reverb->set_width(1.0f); 
	reverb->set_wet(0.42f);
	std::this_thread::sleep_for(std::chrono::seconds(5));

	std::cout << "darker longer tail...\n";
    reverb->set_room_size(1.f);
	reverb->set_damping(0.45f);
	reverb->set_wet(0.53f);
	reverb->set_dry(0.95f);
	std::this_thread::sleep_for(std::chrono::seconds(5));

	majimix_ptr->stop_playback(play_handle);
	majimix_ptr->stop_mixer();
	majimix::pa::terminate();
	return 0;
}