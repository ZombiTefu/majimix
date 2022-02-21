/*
 * kss.cpp
 *
 * @author  François Jacobs
 * @date 13/02/2022
 * @version
 *
 * @section majimix_lic_cpp LICENSE
 *
 * The MIT License (MIT)
 *
 * Copyright © 2022  - François Jacobs
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
 */


#include "kss.hpp"

#include <iostream>
#include <fstream>

namespace majimix::kss {


KSS *load_kss(const std::string & filename)
{
	// FIXME: add zip support - 
	return KSS_load_file((char *) filename.c_str());
}

/* TEST DEV : --- à remplacer directement par les KSS_delete et KSSPLAY_delete --- */
/*static*/ void kss_deleter(KSS* kss)
{
#ifdef DEBUG
std::cout <<"kss_deleter - ";
#endif
	if(kss)
	{
		KSS_delete(kss);
#ifdef DEBUG
		std::cout << "deleted\n";
#endif
	}
#ifdef DEBUG
	else
		std::cout << "nop\n";
#endif
}

/*static*/ void kssplay_deleter(KSSPLAY* kssplay)
{
#ifdef DEBUG
	std::cout <<"kssplay_deleter - ";
#endif
	if(kssplay)
	{
		KSSPLAY_delete(kssplay);
#ifdef DEBUG
		std::cout << "deleted\n";
#endif
	}
#ifdef DEBUG
	else
		std::cout << "nop\n";
#endif
}


static KSS* KSS_copy(KSS *kss)
{
	KSS *kss_copy {nullptr};
	if(kss)
	{
		kss_copy = KSS_new(kss->data, kss->size); // creation copie data + reste à 0
		                                          // NE FONCTIONNE PAS => tester un partage du data plutôt qu'une copie MAIS ATTENTION AU DELETE !!!
		                                          // Je pense à cause du realloc de KSS_set_data
		if(kss_copy)
		{
			kss_copy->type = kss->type;
			kss_copy->stop_detectable = kss->stop_detectable;
			kss_copy->loop_detectable = kss->loop_detectable;
//			kss_copy->title[KSS_TITLE_MAX]; /* the title */
//			kss_copy->idstr[8];             /* ID */
//			  uint8_t *data;                /* the KSS binary */
//			  uint32_t size;                /* the size of KSS binary */
//			  uint8_t *extra;               /* Infomation text for KSS info dialog */

			kss_copy->kssx = kss->kssx; /* 0:KSCC, 1:KSSX */
			kss_copy->mode = kss->mode; /* 0:MSX 1:SMS 2:GG */

			kss_copy->fmpac = kss->fmpac;
			kss_copy->fmunit = kss->fmunit;
			kss_copy->sn76489 = kss->sn76489;
			kss_copy->ram_mode = kss->ram_mode;
			kss_copy->msx_audio = kss->msx_audio;
			kss_copy->stereo = kss->stereo;
			kss_copy->pal_mode = kss->pal_mode;

			kss_copy->DA8_enable = kss->DA8_enable;

			kss_copy->load_adr = kss->load_adr;
			kss_copy->load_len = kss->load_len;
			kss_copy->init_adr = kss->init_adr;
			kss_copy->play_adr = kss->play_adr;

			kss_copy->bank_offset = kss->bank_offset;
			kss_copy->bank_num = kss->bank_num;
			kss_copy->bank_mode = kss->bank_mode;
			kss_copy->extra_size = kss->extra_size;
			kss_copy->device_flag = kss->device_flag;
			kss_copy->trk_min = kss->trk_min;
			kss_copy->trk_max = kss->trk_max;

			//  uint8_t vol[EDSC_MAX];
			std::memcpy(kss_copy->vol, kss->vol, sizeof(kss->vol));

			// info_num + KSSINFO pourraient être utilisé pour automatiser les transitions !?
			// kss_copy->info_num = kss->info_num;
			//KSSINFO *info;
		}
	}
	return kss_copy;
}

/**
 * @brief Affectation / remplacement du pointeur KSS à la KSSLine
 * @warning KSSLine récupère le \a ownership du pointeur kss.
 *          L'appelant <b>ne doit pas</b> effectuer un delete sur ce pointeur.
 *          Le delete sera effectué par la \c line.
 *
 * @param kss
 */
void KSSLine::set_kss(KSS *kss)
{
	kss_ptr = {kss, &kss_deleter};
}
/**
 * @brief Affectation d'un pointeur KSSPLAY à la KSSLine
 * @warning Cette KSSLine récupère le \a ownership du pointeur kssplay.
 *          L'appelant <b>ne doit pas</b> effectuer un delete sur ce pointeur.
 *          Le delete sera effectué par la \c line.
 *
 * @param kssplay
 */
void KSSLine::set_kssplay(KSSPLAY *kssplay)
{
	kssplay_ptr = {kssplay, &kssplay_deleter};
}




//void CartridgeKSS::init_kssplay(KSSLine &line)
//{
//	line.set_kssplay(KSSPLAY_new(m_rate, m_channels, m_kss_bits));
//	uint32_t quality = 1; // 0-1 (plus clair)
//	KSSPLAY_set_device_quality(line.kssplay_ptr.get(),EDSC_PSG,quality);
//	KSSPLAY_set_device_quality(line.kssplay_ptr.get(),EDSC_SCC,quality);
//	KSSPLAY_set_device_quality(line.kssplay_ptr.get(),EDSC_OPL,quality);
//	KSSPLAY_set_device_quality(line.kssplay_ptr.get(),EDSC_OPLL,quality);
//
//	KSSPLAY_set_data(line.kssplay_ptr.get(), line.kss_ptr.get());
//
//	KSSPLAY_set_device_pan(line.kssplay_ptr.get(), EDSC_PSG,2);
//	KSSPLAY_set_device_pan(line.kssplay_ptr.get(), EDSC_SCC,3);
//	KSSPLAY_set_device_pan(line.kssplay_ptr.get(), EDSC_OPL,5);
//	KSSPLAY_set_device_pan(line.kssplay_ptr.get(), EDSC_OPLL,9);
//
//	// 1 = droite ; 2 = gauche ; 3 = centre
//	int opllPan = 3;
//	for(int ch=0; ch<14; ++ch)
//		KSSPLAY_set_channel_pan(line.kssplay_ptr.get(), EDSC_OPLL, ch,opllPan);
//
//	KSSPLAY_set_silent_limit(line.kssplay_ptr.get(), m_silent_limit_ms);
//
//	//	 KSSPLAY_set_device_volume(kss_play, EDSC_SCC, 40); //mus effets
//	//	 KSSPLAY_set_device_volume(kss_play, EDSC_PSG, 40);  // percu + basse
//	//	 KSSPLAY_set_device_volume(kss_play, EDSC_OPL, 40);
//	// KSSPLAY_set_device_volume(kss_play, EDSC_OPLL, 40);
//	// KSSPLAY_set_device_volume(kss_play, EDSC_MAX, 40);
//
//	// XXX à prendre en compte dans les kss_line
//	// line.kssplay_ptr->vsync_freq = 50; // m_vsync_freq;
//}

// CartridgeKSS::CartridgeKSS(const std::string &filename, int nb_lines, int rate, int channels, int bits, int silent_limit_ms)
// : m_lines_count {static_cast<uint8_t>(std::max(nb_lines, 1))},
//   m_rate {static_cast<uint32_t>(rate)},
//   m_channels {static_cast<uint8_t>(channels)},
//   m_bits {static_cast<uint8_t>(bits)},
//   m_silent_limit_ms {static_cast<unsigned int>(silent_limit_ms)},
//   m_next_mixer {0},
//   m_master_volume{60}
// {
// 	KSS *kss = KSS_load_file((char *) filename.c_str());
// 	if(kss)
// 	{
// 		// creation des voies
// 		//m_lines.assign(m_lines_count, std::make_unique<KSSLines>());

// 		for(int i = 0; i < m_lines_count; ++i)
// 			m_lines.push_back(std::make_unique<KSSLine>());

// 		bool first = true;
// 		for(auto &l : m_lines)
// 		{
// 			if(first)
// 			{
// 				l->set_kss(kss);
// 				first = false;
// 			}
// 			init_line(kss, *l);
// 		}
// 	}

// 	if(bits == 16)
// 	{
// 		read_line = &CartridgeKSS::read_line_convert<2, true>;  // mode additif
// 		read_lines = &CartridgeKSS::read_lines_convert<2, true>;
// 	}
// 	else
// 	{
// 		read_line = &CartridgeKSS::read_line_convert<3, true>;  // mode additif
// 		read_lines = &CartridgeKSS::read_lines_convert<3, true>;
// 	}
// }


CartridgeKSS::CartridgeKSS(KSS* kss, int nb_lines, int rate, int channels, int bits, int silent_limit_ms)
: m_lines_count {static_cast<uint8_t>(std::max(nb_lines, 1))},
  m_rate {static_cast<uint32_t>(rate)},
  m_channels {static_cast<uint8_t>(channels)},
  m_bits {static_cast<uint8_t>(bits)},
  m_silent_limit_ms {static_cast<unsigned int>(silent_limit_ms)},
  m_next_mixer {0},
  m_master_volume{60}
{
	if (kss && nb_lines > 0)
	{
		for (int i = 0; i < m_lines_count; ++i)
			m_lines.push_back(std::make_unique<KSSLine>());

		m_lines[0]->set_kss(kss);
		for (auto &l : m_lines)
		{
			init_line(kss, *l);
		}
	}

	if(bits == 16)
	{
		read_line = &CartridgeKSS::read_line_convert<2, true>;  // mode additif
		read_lines = &CartridgeKSS::read_lines_convert<2, true>;
	}
	else
	{
		read_line = &CartridgeKSS::read_line_convert<3, true>;  // mode additif
		read_lines = &CartridgeKSS::read_lines_convert<3, true>;
	}
}

void CartridgeKSS::init_line(KSS *kss_ref, KSSLine &line)
{
	line.active             = false;
	line.pause              = false;
	line.autostop           = false;
	line.forcable           = true;
//	line.stop_request       = false;
	//line.last_value         = 0;
	//line.inactive_count     = 0;
	//line.id                 = 0;
	line.current_track      = 0;
	line.next_track         = 0;
	line.transition_fadeout = 0;



	// init du KSS si necessaire
	if(!line.kss_ptr)
		line.set_kss(KSS_copy(kss_ref));

	// reinit du KSSPLAY
	int32_t current_volume = line.kssplay_ptr ? line.kssplay_ptr->master_volume : m_master_volume;
	int32_t sync_freq = line.kssplay_ptr ? line.kssplay_ptr->vsync_freq : 0;



	line.set_kssplay(KSSPLAY_new(m_rate, m_channels, m_kss_bits));
	uint32_t quality = 1; // 0-1 (plus clair)
	KSSPLAY_set_device_quality(line.kssplay_ptr.get(),EDSC_PSG,quality);
	KSSPLAY_set_device_quality(line.kssplay_ptr.get(),EDSC_SCC,quality);
	KSSPLAY_set_device_quality(line.kssplay_ptr.get(),EDSC_OPL,quality);
	KSSPLAY_set_device_quality(line.kssplay_ptr.get(),EDSC_OPLL,quality);

	KSSPLAY_set_data(line.kssplay_ptr.get(), line.kss_ptr.get());

	if(m_channels > 1)
	{
		// stereo : see kss2wav.c (l 219)

		// MSX : PSG + SCC 
		// Device pan : +128 left => -128 right (0 center)
		// KSSPLAY_set_channel_pan : OPLL only
	 	KSSPLAY_set_device_pan(line.kssplay_ptr.get(), EDSC_PSG, -32); // a little further to the right
	 	KSSPLAY_set_device_pan(line.kssplay_ptr.get(), EDSC_SCC, 32);  // a little further to the left

		line.kssplay_ptr->opll_stereo = 1;
		KSSPLAY_set_channel_pan(line.kssplay_ptr.get(), EDSC_OPLL, 0, 1);
		KSSPLAY_set_channel_pan(line.kssplay_ptr.get(), EDSC_OPLL, 1, 2);
		KSSPLAY_set_channel_pan(line.kssplay_ptr.get(), EDSC_OPLL, 2, 1);
		KSSPLAY_set_channel_pan(line.kssplay_ptr.get(), EDSC_OPLL, 3, 2);
		KSSPLAY_set_channel_pan(line.kssplay_ptr.get(), EDSC_OPLL, 4, 1);
		KSSPLAY_set_channel_pan(line.kssplay_ptr.get(), EDSC_OPLL, 5, 2);
	}


	KSSPLAY_set_silent_limit(line.kssplay_ptr.get(), m_silent_limit_ms);

	//	 KSSPLAY_set_device_volume(kss_play, EDSC_SCC, 40); //mus effets
	//	 KSSPLAY_set_device_volume(kss_play, EDSC_PSG, 40);  // percu + basse
	//	 KSSPLAY_set_device_volume(kss_play, EDSC_OPL, 40);
	// KSSPLAY_set_device_volume(kss_play, EDSC_OPLL, 40);
	// KSSPLAY_set_device_volume(kss_play, EDSC_MAX, 40);

	KSSPLAY_set_master_volume(line.kssplay_ptr.get(), current_volume);

	line.kssplay_ptr->vsync_freq = sync_freq;
}

void CartridgeKSS::activate(KSSLine &line, uint8_t track, bool autostop, bool forcable, int fadeout_ms)
{
		line.autostop = autostop;
		// non line->current_track = track;
		line.next_track = track;
		line.pause = false;
		line.forcable = forcable;
//		line.stop_request = false;
		line.id = m_next_mixer++;

		if(fadeout_ms)
		{
			line.transition_fadeout = fadeout_ms * m_rate / 1000;
			KSSPLAY_fade_start(line.kssplay_ptr.get(), fadeout_ms);
		}
		else
			line.transition_fadeout = 0;

		// en dernier
		line.active = true;
}

//CartridgeKSS::~CartridgeKSS()
bool CartridgeKSS::set_output_format(int samples_per_sec, int channels, int bits/*, int silent_limit_ms*/)
{
	if(samples_per_sec >= 8000 && samples_per_sec <= 96000 && (channels == 1 || channels ==2) && (bits == 16 || bits == 24) /*&& silent_limit_ms >= 0*/)
	{
		m_rate = samples_per_sec;
		m_channels = channels;
		m_bits = bits;
		// m_silent_limit_ms = silent_limit_ms;

		if(bits == 16)
		{
#ifdef DEBUG
			std::cerr << "read_line_convert<2, true>\n";
#endif
			read_line = &CartridgeKSS::read_line_convert<2, true>;  // mode additif
			read_lines = &CartridgeKSS::read_lines_convert<2, true>;
		}
		else
		{
#ifdef DEBUG
			std::cerr << "read_line_convert<3, true>\n";
#endif
			read_line = &CartridgeKSS::read_line_convert<3, true>;  // mode additif
			read_lines = &CartridgeKSS::read_lines_convert<3, true>;
		}


		// réinit lines
		KSS *kss_ref = m_lines[0]->kss_ptr.get();
		for(auto &l : m_lines)
			init_line(kss_ref, *l);
		return true;
	}
	return false;
}
bool CartridgeKSS::set_lines_count(int nb_lines)
{
	if(nb_lines > 0)
	{
		if(nb_lines != m_lines_count)
		{
			if(nb_lines < m_lines_count)
			{
				m_lines.resize(nb_lines);
			}
			else
			{
				int add = nb_lines - m_lines_count;
				for(int i = 0; i < add; ++i)
				{
					m_lines.push_back(std::make_unique<KSSLine>());
					init_line(m_lines[0]->kss_ptr.get(), *m_lines.back());
				}
			}
			m_lines_count = m_lines.size();
		}
	}
	return false;
}

int CartridgeKSS::get_line_count() const
{
	return m_lines_count;
}

// une ligne
template<int N, bool ADD>
int CartridgeKSS::read_line_convert(std::vector<int>::iterator it_out, KSSLine &line, int requested_sample_count)
{
	const int data_count = requested_sample_count * m_channels;
	if(m_lines_buffer.size() < static_cast<unsigned int>(data_count))
		m_lines_buffer.resize(data_count);

	int sample_count = 0;
	bool deactivate = false; // line.stop_request;
	if(line.active)
	{
		if(!deactivate && !line.pause)
		{
			// nouvelle activation
			if(line.next_track && !line.transition_fadeout)
			{
				line.current_track = line.next_track;
				line.next_track = 0;
				KSSPLAY_reset(line.kssplay_ptr.get(), line.current_track, 0);
			}

			// recupération des données
			KSSPLAY_calc(line.kssplay_ptr.get(), m_lines_buffer.data(), requested_sample_count);
			// check autostop
			deactivate = line.autostop && (KSSPLAY_get_stop_flag(line.kssplay_ptr.get()) == 1);
			auto it_end = m_lines_buffer.cbegin() + data_count;
			if constexpr (N == 2)
			{
				if constexpr (ADD)
				{
//					std::cout << "ADD16 !\n";
					// version additive !
					std::transform(m_lines_buffer.cbegin(), it_end, it_out, it_out, [](const int16_t &v, int &v_buffer) -> int { return static_cast<int>(v) + v_buffer; });
				}
				else
					// 16 bits - simple copie
					std::copy(m_lines_buffer.cbegin(), it_end, it_out);
			}
			else // N = 3 : 24 bits
			{
//				for(auto it = m_lines_buffer.cbegin(); it != it_end; ++it)
//					*it_out++ = (static_cast<int>(*it) << 8);
				if constexpr (ADD)
				{
//					std::cout << "ADD24 !\n";
					// version additive !
					std::transform(m_lines_buffer.cbegin(), it_end, it_out, it_out, [](const int16_t &v, int &v_buffer) -> int { return (static_cast<int>(v) << 8) + v_buffer; });
				}
				else
					// 24 bits - conversion et copie
					std::transform(m_lines_buffer.cbegin(), it_end, it_out, [](const int16_t &v) -> int { return (static_cast<int>(v) << 8); });
			}
			sample_count = requested_sample_count;

			if(line.transition_fadeout)
			{
					if(line.transition_fadeout < requested_sample_count)
					{
						line.transition_fadeout = 0;
						deactivate = !line.next_track;
					}
					else
						line.transition_fadeout -= requested_sample_count;
			}
			/*
			if(line.transition_fadeout || line.next_track)
			{
					if(line.transition_fadeout < requested_sample_count)
					{
						line.transition_fadeout = 0;
						line.current_track = line.next_track;
						line.next_track = 0;
						if(line.current_track)
							KSSPLAY_reset(line.kssplay_ptr.get(), line.current_track, 0);
						else
							deactivate = true;
					}
					else
						line.transition_fadeout -= requested_sample_count;
			}

			 */
		}
		if(deactivate)
		{
//			line.stop_request = false;
			line.active = false;
		}
	}
	return sample_count;
}

// toutes lignes
template<int N, bool ADD>
int CartridgeKSS::read_lines_convert(std::vector<int>::iterator it_out, int requested_sample_count)
{
	if constexpr (!ADD)
	{
		const int data_count = requested_sample_count * m_channels;
		// mode standard : reset du buffer
		std::fill(it_out, it_out + data_count, 0);
	}

	//int sample_count;
	for(auto &l : m_lines)
		// template avec true ici car on va sommer les lines dans le buffer qui a été remis à 0 si !ADD
		read_line_convert<N, true>(it_out, *l, requested_sample_count);
	return requested_sample_count;
}


int CartridgeKSS::read(std::vector<int>::iterator it_out, KSSLine &line, int requested_sample_count)
{
	return read_line(this, it_out, line, requested_sample_count);
}

int CartridgeKSS::read(std::vector<int>::iterator it_out, int requested_sample_count)
{
	return read_lines(this, it_out, requested_sample_count);
}

std::vector<std::unique_ptr<KSSLine>>::iterator CartridgeKSS::begin()
{
	return m_lines.begin();
}
std::vector<std::unique_ptr<KSSLine>>::iterator CartridgeKSS::end()
{
	return m_lines.end();
}





// ici locker le buffer n'est pas necessaire car on ne modifie pas une ligne active !
// on tente d'activer une ligne inactive => aucun pb pour le mixer
/**
 * @brief try to find and activate a free \e line.
 *
 * @param track The soundtrack to be associated with the \e line.
 * @param autostop Automatic disabling of the line when the track (sound playback) is finished.
 * @return the index (1 based) of the activated \c line or 0 if no \c line could be activated.
 */
int CartridgeKSS::active_line(int track, bool autostop, bool forcable)
{
	int id = 0;
	for(auto &l : m_lines)
	{
		++id;
		if(!l->active)
		{
			activate(*l, track, autostop, forcable);
			return id; // 1 based index
		}
	}
	return 0;
}


// ici on doit "pauser" le mixer car on intervient sur une ligne déjà active
/**
 * @brief Force the activation of a \c line.
 *
 * XXX a commenter : seules les ligne en autostop et pas en pause sont recherchées
 *
 * @warning It is necessary to synchronize the mixer with this call - pausing the mixer then resuming after the call for example
 *
 * @param track The soundtrack to be associated with the \e line.
 * @param autostop Automatic disabling of the line when the track (sound playback) is finished.
 * @return the index (1 based) of the activated \c line or 0 if no \c line could be activated.
 */
int CartridgeKSS::force_line(int track, bool autostop, bool forcable)
{
	int id = 0;
	int min = m_next_mixer;
	int idmin = 0;
	for(auto &l : m_lines)
	{
		++id;
		// reprise 05/02/2022 if(l->autostop && !l->pause) // on évite les bgm et les pause
		if(l->forcable)
		{
			if(l->id < min)
			{
				min = l->id;
				idmin = id;
			}
		}
	}

	if(idmin)
	{
		activate(*m_lines[idmin-1], track, autostop, forcable);
//		auto &l = m_lines[idmin-1];
//		// on prend
//		l->autostop = autostop;
//		// non line->current_track = track;
//		l->next_track = track;
//		l->pause = false;
//		l->stop_request = false;
//		l->transition_fadeout = 0;
//		l->id = m_next_mixer++;
//
//		// en dernier
//		l->active = true;
	}
	return idmin; // 1 based index
}
bool CartridgeKSS::update_line(int line_id, int new_track, bool autostop, bool forcable, int fade_out_ms)
{
	activate(*m_lines[line_id-1], new_track, autostop, forcable, fade_out_ms);
//	auto &line = m_lines[line_id];
//	// on prend
//	line->autostop = autostop;
//	// non line->current_track = track;
//	line->next_track = new_track;
//	line->pause = false;
//	line->stop_request = false;
//	line->transition_fadeout = fade_out_ms * m_rate / 1000;
//	line->id = m_next_mixer++;
//	KSSPLAY_fade_start(line->kssplay_ptr.get(), fade_out_ms);
//
//	// en dernier
//	line->active = true;

	return true;
}


void CartridgeKSS::set_pause(int line_id, bool pause)
{
	m_lines[line_id-1]->pause = pause;
}

void CartridgeKSS::set_pause_active(bool pause)
{
	for(auto &l : m_lines)
		if(l->active)
			l->pause = pause;

}

void CartridgeKSS::stop(int line_id)
{
	m_lines[line_id-1]->active = false;
}

void CartridgeKSS::stop_active()
{
	for(auto &l : m_lines)
		if(l->active)
			l->active = false;
}


// master volume
void CartridgeKSS::set_master_volume(int volume)
{
	m_master_volume = volume;
	for(auto &l : m_lines)
	{
		KSSPLAY_set_master_volume(l->kssplay_ptr.get(), m_master_volume);
	}
}

void CartridgeKSS::set_line_volume(int line_id, int volume)
{
	KSSPLAY_set_master_volume(m_lines[line_id-1]->kssplay_ptr.get(), volume);
}

// frequence

void CartridgeKSS::set_kss_line_frequency(KSSLine *l, int frequency)
{
	if(!l->active)
	{
		l->kssplay_ptr->vsync_freq = frequency;
	}
	else
	{
		uint32_t position =  l->kssplay_ptr->decoded_length;
		// ajustement de la position avec le changement de frequence
		position = position * l->kssplay_ptr->vsync_freq / frequency;
		l->kssplay_ptr->vsync_freq = frequency;
		// cpu_speed = 0:auto 1:3.58MHz 2:7.16MHz ...
		KSSPLAY_reset(l->kssplay_ptr.get(), l->current_track, 0);
		KSSPLAY_calc_silent(l->kssplay_ptr.get(), position);
	}
}

void CartridgeKSS::set_kss_frequency(int frequency)
{
	for(auto &l : m_lines)
		set_kss_line_frequency(l.get(), frequency);
}

void CartridgeKSS::set_kss_line_frequency(int line_id, int frequency)
{
	set_kss_line_frequency(m_lines[line_id-1].get(), frequency);
}

int CartridgeKSS::get_playtime_millis(int line_id) 
{
	if (m_rate == 0)
		return 0;
	int64_t decode_length = static_cast<int64_t>(m_lines[line_id-1]->kssplay_ptr->decoded_length) * 1000;
	return static_cast<int>(decode_length / m_rate);
}


}



