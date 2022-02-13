/*
 * kss.hpp
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


#ifndef KSS_HPP_
#define KSS_HPP_

#include  "kssplay.h"
#include <cstring>
#include <vector>
#include <string>
#include <atomic>
#include <memory>
#include <functional>

/**
 * \namespace majimix::kss
 * \brief Support KSS pour le Majimix mixer
 * \details Les fichiers KSS sont des dump mémoire des données audio de jeux des ordinateurs MSX.
 *         Les classes et structures présentes ici permettent de lire ce type de fichiers à l'aide de la bibliothèque libkss (https://github.com/digital-sound-antiques/libkss).
 *         À la différence des sources classiques WAVE ou Vorbis, les sources KSS sont exploitées à l'aide d'une classe dédié CartridgeKSS.
 *
 *
 * \warning Attention ici :
 *          CartridgeKSS gère les KSSLine (voix - couple {KSS, KSSPLAY} ) pour obtenir voire mixer les données audio
 *          Il n'a aucune connaissance des éventuels thread l'appelant et N'EST PAS THREAD-SAFE !
 *          Toutes les méthode de CartridgeKSS son susceptibles de lire et modifier les KSS et KSSPLAY des KSSLine et ces structures
 *          ne doivent pas (jamais!) être accedées simultanément depuis plusieurs thread.
 *          C'est à l'appelant de synchroniser les appels - cela est prévu dans Majimix
 *          Majimix exploite un thread (T1) PortAudio qui récupère les données sonores au bon format (ex 16 bist stereo 44,1KHz) et ne doit jamais être
 *          bloqué (lock interdit)
 *
 * \warning	Pour pemettre une diffusion du son en continue, Majimix exploite un deuxième thread (T2) via la classe
 *          BufferedMixer qui supporte les locks.
 *          C'est dans ce thread que le mixage est effectué et bufferisé.
 *          Cela permet de lire les fichiers ogg et mixer les données audio.
 *
 *  \warning Le thread portaudio (T1) récupère les données du MixedBuffer (T2).
 *           Si MixedBuffer (T2) n'a pas eu suffisamment de temps pour préparer les données de portaudio,
 *           il fournit un buffer vide au thread portaudio (T1) qui peut continuer sans blocage.
 *           (cela veut dire aussi que la configuration du MixedBuffer est à revoir ;))
 *
 *           Ceci étant, il y a une exception :
 *           les appels une KSSLine inactive (active = false) peuvent être effectués depuis un autre thread sans risque
 *
 */
namespace majimix::kss {

KSS *load_kss(const std::string & filename);
void kss_deleter(KSS *kss);
void kssplay_deleter(KSSPLAY *kssplay);

/**
 * \struct KSSLine kss.hpp "kss.hpp"
 * @brief KSSLine représente une voix (ligne) dans laquelle est associé un track (musique ou son).
 * @details KSSLine - triplet {KSS, KSSPLAY, numéro de track} permettant d'extraire les données audio PCM 16 bits d'une piste du fichier KSS
 *          Les données audio PCM du track peuvent être récupérées par la métode \a read de la classe \a CartridgeKSS.
 */
struct KSSLine
{
    /**
     * @brief Ancienneté de la \a line.
     *
     * Permet de déterminer l'ancienneté de la \a line. Cette valeur est lise à jour lors de l'activation de la \a line.
     */
    int id;

    /**
     * @brief KSS pointer (c.f. libkss) permettant l'extraction des données PCM
     */
    std::unique_ptr<KSS, decltype(&kss_deleter)> kss_ptr;
    /**
     * @brief KSSPLAY pointer (c.f. libkss) permettant l'extraction des données PCM
     */
    std::unique_ptr<KSSPLAY, decltype(&kssplay_deleter)> kssplay_ptr;

    // std::atomic is neither copyable nor movable => attention au vector !
    /**
     * @brief Activité de la KSSLine.
     *
     * Determine l'activité de la \a line. Une \a line active est potentiellement en cours de traitement dans le thread effectuant la récupération et le mixage des données autio.
     */
    std::atomic_bool active;
    std::atomic_bool pause;
    /**
     * Pour la detection automatique de fin de track (son ou musique).
     * Indique si la \a line est en mode autostop. Ce mode permet la désactivation automatique de la \a line
     * sans demande explicite utilisateur pour mettre fin au traitement de cette \a line lorsque le track est \em terminé.
     */
    std::atomic_bool autostop;
    /*  indique s'il est possible de forcer la ligne cad : play force true peut prendre une ligne forcable */
    bool forcable;

    //	std::atomic_bool stop_request;

    uint8_t current_track;
    int32_t transition_fadeout;
    uint8_t next_track;

    KSSLine()
        : id{0},
          kss_ptr{nullptr, &kss_deleter},
          kssplay_ptr{nullptr, &kssplay_deleter},
          active{false},
          pause{false},
          forcable{true},
          autostop{false},
          //	 stop_request{false},
          current_track{0},
          transition_fadeout{0},
          next_track{0}

    {
    }

    /**
     * @brief Affectation / remplacement du pointeur KSS à la KSSLine
     * @warning KSSLine récupère le \a ownership du pointeur kss.
     *          L'appelant <b>ne doit pas</b> effectuer un delete sur ce pointeur.
     *          Le delete sera effectué par la \c line.
     *
     * @param kss
     */
    void set_kss(KSS *kss);

    /**
     * @brief Affectation / remplacement du pointeur KSSPLAY à la KSSLine
     * @warning Cette KSSLine récupère le \a ownership du pointeur kssplay.
     *          L'appelant <b>ne doit pas</b> effectuer un delete sur ce pointeur.
     *          Le delete sera effectué par la \c line.
     *
     * @param kssplay
     */
    void set_kssplay(KSSPLAY *kssplay);
};

/**
 * @class CartridgeKSS kss.hpp "kss.hpp"
 * @brief
 *
 */
class CartridgeKSS
{
    // format KSSPLAY 16 bits
    constexpr static uint8_t m_kss_bits = 16;

    // nombre de voix (lines)
    uint8_t m_lines_count;

    // sample rate - rate de KSSPLAY et de sortie
    uint32_t m_rate;

    // 1 mono 2 stereo - idem KSSPLAY et sortie
    uint8_t m_channels;

    // output format 16 ou 24 - KSS produit un format 16 bits => conversion necessaire si sortie 24 bits
    uint8_t m_bits;

    // detection des silences pour arrêt d'un son
    unsigned int m_silent_limit_ms;

    // usage interne pour determiner quelle line est la plus ancienne
    // utilisé lorsque l'on est obligé de forcer une line - c-a-d demande de changement de track sur une line active
    int m_next_mixer;
    int m_master_volume;

    // liste des voix - lines
    std::vector<std::unique_ptr<KSSLine>> m_lines;

    // buffer de récupération des track depuis kss_play
    std::vector<int16_t> m_lines_buffer;

    // read all
    // buffer converti d'une ligne à destination de m_mixed_lines_buffer
    //		std::vector<int> m_converted_line_buffer;
    // read all
    //		// buffer de mixage des lignes actives
    //	std::vector<int> m_mixed_lines_buffer;

    template <int N, bool ADD>
    int read_line_convert(std::vector<int>::iterator it_out, KSSLine &line, int requested_sample_count);
    template <int N, bool ADD>
    int read_lines_convert(std::vector<int>::iterator it_out, int requested_sample_count);

    //		using fn_read_line = std::function<int(std::vector<int>::iterator it_out,KSSLine &line, int requested_sample_count)>;
    using fn_read_line = std::function<int(CartridgeKSS *c, std::vector<int>::iterator it_out, KSSLine &line, int requested_sample_count)>;
    fn_read_line read_line;
    using fn_read_lines = std::function<int(CartridgeKSS *c, std::vector<int>::iterator it_out, int requested_sample_count)>;
    fn_read_lines read_lines;

    KSS *create_copy();
    void init_line(KSS *kss_ref, KSSLine &line);

    void activate(KSSLine &line, uint8_t track, bool autostop, bool forcable = true, int fadeout_ms = 0);
    void set_kss_line_frequency(KSSLine *l, int frequency);

public:
    // CartridgeKSS(const std::string &filename, int nb_lines = 1, int rate = 44100, int channels = 2, int bits = 16, int silent_limit_ms = 200);
    CartridgeKSS(KSS *kss, int nb_lines = 1, int rate = 44100, int channels = 2, int bits = 16, int silent_limit_ms = 500);
    //~CartridgeKSS();
    bool set_output_format(int samples_per_sec, int channels, int bits /*, int silent_limit_ms*/);
    bool set_lines_count(int nb_lines);
    int get_line_count() const;
    //	/*
    //	 * ex play_sound 1 => int channel_id = activate_track(1) => trouve une piste inactive (sinon remplace la plus ancienne non bgm - cad en autostop -) change le track à 1 et active : retourne le channel_id utilisé
    //	 * ex play_bgm 1   => int channel_id = activate_track(1, false) => trouve une piste inactive (sinon remplace la plus ancienne non bgm - cad en autostop -) change le track à 1 et active : retourne le channel_id utilisé
    //	 *
    //	 */
    //	//  A VIRER
    //	int activate_track(int track, bool autostop = true, bool force = true);
    //	// A VIRER
    //	bool update_track(int channel_id, int new_track, bool autostop = true, int fade_out_ms = 0);

    /**
     * Conversion d'un Channel (track) au format du mixer - pas de fusion ici
     * @param it_out
     * @param line
     * @param requested_sample_count
     * @return le nombre de sample traité - égal au demandé requested_sample_count sauf
     */
    int read(std::vector<int>::iterator it_out, KSSLine &line, int requested_sample_count);
    // version toutes ligne
    int read(std::vector<int>::iterator it_out, int requested_sample_count);

    std::vector<std::unique_ptr<KSSLine>>::iterator begin();
    std::vector<std::unique_ptr<KSSLine>>::iterator end();

    /**
     * Activation d'une line inactive.
     * Recherche une line inactive et en mode autostop :
     *   Si une line est trouvée :
     *   	met à jour le track
     *   	active la ligne - elle peut commencer à être utilisée par le mixer
     *   	retourne le numéro de la ligne (1 based index)
     *   Si aucune ligne libre :
     *   	retourn 0
     *
     *  Cette méthode est sans risque - pas besoin d'arrêter le mixer
     *
     * @param track
     * @param autostop
     * @return
     */
    int active_line(int track, bool autostop = true, bool forcable = true);

    /*
     * Force la mise à jour de la ligne la plus ancienne et qui est en autostop - qui est potentiellement active
     *
     * Il est necessaire de synchroniser le mixer avec cet appel - pause du mixer puis reprise après appel par exemple
     *
     * @param track
     * @param autostop
     * @return
     */

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
    int force_line(int track, bool autostop = true, bool forcable = true);

    /**
     * Mise à jour d'une ligne identifiée par line_id
     *
     * Il est necessaire de synchroniser le mixer avec cet appel - pause du mixer puis reprise après appel par exemple
     *
     * @param line_id 1 based line index
     * @param new_track
     * @param autostop
     * @param fade_out_ms
     * @return
     */
    bool update_line(int line_id, int new_track, bool autostop = true, bool forcable = true, int fade_out_ms = 0);

    /**
     * @brief Pause / Resume a specific line
     *
     * @param line_id
     * @param pause
     * @return
     */
    void set_pause(int line_id, bool pause);
    void set_pause_active(bool pause);
    void stop(int line_id);
    void stop_active();

    // master volume
    void set_master_volume(int volume);
    // line volume
    void set_line_volume(int line_id, int volume);

    // frequence
    void set_kss_frequency(int frequency);
    void set_kss_line_frequency(int line_id, int frequency);

    int get_playtime_millis(int line_id);
};
}




#endif /* KSS_HPP_ */
