# Majimix

## Overview
Majimix is a high-level API designed to play simultaneously audio sources (WAVE files, Xiph Ogg audio files, KSS files) as sounds or background musics.


## Example
```cpp
#include <iostream>
#include <majimix/majimix.hpp>

int main()
{
    // initialize majimix
    majimix::pa::initialize();

    // create a majimix instance
    auto majimix_ptr = majimix::pa::create_instance();

    // set the majimix output format : rate 44,1 KHz stereo 16 bits
    // and use 10 channels : you can play 10 sounds simultaneously
    if (majimix_ptr->set_format(44100, true, 16, 10))
    {
        majimix_ptr->set_master_volume(50);
        // add source and get the source handle (it can be done later)
        int source_handle_1 = majimix_ptr->add_source("bgm.ogg");
        // ... add other sources ...

        // start majimix instance
        if (majimix_ptr->start_mixer())
        {
            // play bgm.ogg (loop)
            int play_handle_1 = majimix_ptr->play_source(source_handle_1, true, false);
            std::cout << play_handle_1 << "\n";

            // add a new source
            int source_handle_2 = majimix_ptr->add_source("sound.wav");
            std::cout << source_handle_2 << "\n";

            // press any key 'q' to quit
            char u = 0;
            while(u != 'q') 
            {
                // play sound.wav (once) while bgm.ogg continu
                int play_handle_2 = majimix_ptr->play_source(source_handle_2);
                std::cout << "Press 'q' to quit\n";
                std::cin >> u; 
            }

            // stop majimix
            majimix_ptr->stop_mixer();
        }
    }

    // dispose
    majimix::pa::terminate();
    return 0;
}
 ```

 ## Dependencies

 Majimix uses the following libraries:<br>
   - PortAudio   - http://www.portaudio.com
   - Xiph libogg - https://xiph.org/ogg
   - libKss      - https://github.com/digital-sound-antiques/libkss

 ## Build

 ### On Linux

 ### On Windows