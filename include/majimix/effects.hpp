#ifndef MAJIMIX_EFFECTS_HPP_
#define MAJIMIX_EFFECTS_HPP_

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace majimix {

class AudioEffect {
public:
	virtual ~AudioEffect() = default;

	/**
	 * @brief Prepare the effect for a mixer format.
	 *
	 * Called with the current mixer output format before processing begins.
	 * The effect processes interleaved floating-point samples.
	 *
	 * @param [in] sample_rate mixer sample rate
	 * @param [in] channels mixer channel count
	 */
	virtual void prepare([[maybe_unused]] int sample_rate, [[maybe_unused]] int channels)
	{
	}

	/**
	 * @brief Reset the internal effect state.
	 */
	virtual void reset() {}

	/**
	 * @brief Process an interleaved floating-point audio buffer in place.
	 *
	 * The buffer contains frame_count * channels samples.
	 *
	 * @param [in,out] samples interleaved floating-point samples
	 * @param [in] frame_count number of audio frames in the buffer
	 */
	virtual void process(float *samples, int frame_count) = 0;
};

class GainEffect final : public AudioEffect {
public:
	explicit GainEffect(float gain = 1.0f)
		: gain_ {sanitize_gain(gain)}
	{
	}

	void prepare([[maybe_unused]] int sample_rate, int channels) override
	{
		channel_count_.store(channels > 0 ? channels : 1, std::memory_order_relaxed);
	}

	void set_gain(float gain)
	{
		gain_.store(sanitize_gain(gain), std::memory_order_relaxed);
	}

	float get_gain() const
	{
		return gain_.load(std::memory_order_relaxed);
	}

	void process(float *samples, int frame_count) override
	{
		if(!samples || frame_count <= 0)
			return;

		const int sample_count = frame_count * channel_count_.load(std::memory_order_relaxed);
		const float gain = gain_.load(std::memory_order_relaxed);
		for(int i = 0; i < sample_count; ++i)
			samples[i] *= gain;
	}

private:
	static float sanitize_gain(float gain)
	{
		return gain < 0.0f ? 0.0f : gain;
	}

	std::atomic<float> gain_;
	std::atomic<int> channel_count_ {1};
};

class FadeEffect final : public AudioEffect {
public:
	explicit FadeEffect(float initial_gain = 1.0f)
		: reset_gain_ {sanitize_gain(initial_gain)},
		  current_gain_value_ {sanitize_gain(initial_gain)},
		  command_target_gain_ {sanitize_gain(initial_gain)},
		  current_gain_ {sanitize_gain(initial_gain)},
		  target_gain_ {sanitize_gain(initial_gain)}
	{
	}

	void prepare(int sample_rate, int channels) override
	{
		sample_rate_.store(sample_rate > 0 ? sample_rate : 44100, std::memory_order_relaxed);
		channel_count_.store(channels > 0 ? channels : 1, std::memory_order_relaxed);
		reset();
	}

	void reset() override
	{
		const float gain = reset_gain_.load(std::memory_order_relaxed);
		current_gain_ = gain;
		target_gain_ = gain;
		gain_step_ = 0.0f;
		remaining_frames_ = 0;
		current_gain_value_.store(gain, std::memory_order_relaxed);
		applied_command_version_ = 0;
	}

	void set_gain(float gain)
	{
		const float sanitized_gain = sanitize_gain(gain);
		reset_gain_.store(sanitized_gain, std::memory_order_relaxed);
		publish_command(sanitized_gain, 0, true);
	}

	float get_current_gain() const
	{
		return current_gain_value_.load(std::memory_order_relaxed);
	}

	float get_target_gain() const
	{
		return command_target_gain_.load(std::memory_order_relaxed);
	}

	void fade_to(float target_gain, int duration_ms)
	{
		publish_command(sanitize_gain(target_gain), duration_ms < 0 ? 0 : duration_ms, false);
	}

	void fade_in(int duration_ms)
	{
		fade_to(1.0f, duration_ms);
	}

	void fade_out(int duration_ms)
	{
		fade_to(0.0f, duration_ms);
	}

	void process(float *samples, int frame_count) override
	{
		if(!samples || frame_count <= 0)
			return;

		apply_pending_command();

		const int channel_count = channel_count_.load(std::memory_order_relaxed);
		int sample_index = 0;
		if(remaining_frames_ <= 0)
		{
			for(int frame = 0; frame < frame_count; ++frame)
			{
				const float gain = current_gain_;
				for(int channel = 0; channel < channel_count; ++channel)
					samples[sample_index++] *= gain;
			}
		}
		else
		{
			for(int frame = 0; frame < frame_count; ++frame)
			{
				const float gain = current_gain_;
				for(int channel = 0; channel < channel_count; ++channel)
					samples[sample_index++] *= gain;

				if(remaining_frames_ > 0)
				{
					current_gain_ += gain_step_;
					--remaining_frames_;
					if(remaining_frames_ == 0)
					{
						current_gain_ = target_gain_;
						gain_step_ = 0.0f;
					}
				}
			}
		}

		current_gain_value_.store(current_gain_, std::memory_order_relaxed);
	}

private:
	static float sanitize_gain(float gain)
	{
		return gain < 0.0f ? 0.0f : gain;
	}

	void publish_command(float target_gain, int duration_ms, bool immediate)
	{
		command_version_.fetch_add(1, std::memory_order_acq_rel);
		command_target_gain_.store(target_gain, std::memory_order_release);
		command_duration_ms_.store(duration_ms, std::memory_order_release);
		command_immediate_.store(immediate, std::memory_order_release);
		command_version_.fetch_add(1, std::memory_order_acq_rel);
	}

	void read_pending_command(float &target_gain, int &duration_ms, bool &immediate, unsigned int &version) const
	{
		for(;;)
		{
			const unsigned int version_before = command_version_.load(std::memory_order_acquire);
			if(version_before & 1U)
				continue;

			target_gain = command_target_gain_.load(std::memory_order_acquire);
			duration_ms = command_duration_ms_.load(std::memory_order_acquire);
			immediate = command_immediate_.load(std::memory_order_acquire);

			const unsigned int version_after = command_version_.load(std::memory_order_acquire);
			if(version_before == version_after)
			{
				version = version_after;
				return;
			}
		}
	}

	void apply_pending_command()
	{
		float requested_target_gain = 0.0f;
		int requested_duration_ms = 0;
		bool immediate = true;
		unsigned int version = 0;
		read_pending_command(requested_target_gain, requested_duration_ms, immediate, version);
		if(version == applied_command_version_)
			return;

		applied_command_version_ = version;
		if(immediate || requested_duration_ms <= 0)
		{
			current_gain_ = requested_target_gain;
			target_gain_ = requested_target_gain;
			gain_step_ = 0.0f;
			remaining_frames_ = 0;
			current_gain_value_.store(current_gain_, std::memory_order_relaxed);
			return;
		}

		const int sample_rate = sample_rate_.load(std::memory_order_relaxed);
		remaining_frames_ = (sample_rate * requested_duration_ms) / 1000;
		if(remaining_frames_ <= 0)
			remaining_frames_ = 1;

		target_gain_ = requested_target_gain;
		gain_step_ = (target_gain_ - current_gain_) / static_cast<float>(remaining_frames_);
	}

	std::atomic<int> sample_rate_ {44100};
	std::atomic<int> channel_count_ {1};
	std::atomic<float> reset_gain_;
	std::atomic<float> current_gain_value_;
	std::atomic<unsigned int> command_version_ {0};
	std::atomic<float> command_target_gain_;
	std::atomic<int> command_duration_ms_ {0};
	std::atomic<bool> command_immediate_ {true};

	unsigned int applied_command_version_ {0};
	float current_gain_ {1.0f};
	float target_gain_ {1.0f};
	float gain_step_ {0.0f};
	int remaining_frames_ {0};
};

namespace detail {

inline float sanitize_denormal_sample(float sample)
{
	return std::fabs(sample) < 1.0e-20f ? 0.0f : sample;
}

class ReverbCombFilter {
public:
	void prepare(int sample_count)
	{
		buffer_.assign(static_cast<size_t>(sample_count > 0 ? sample_count : 1), 0.0f);
		reset();
	}

	void reset()
	{
		std::fill(buffer_.begin(), buffer_.end(), 0.0f);
		index_ = 0;
		filter_store_ = 0.0f;
	}

	float process(float input, float feedback, float damping)
	{
		if(buffer_.empty())
			return input;

		const float output = buffer_[static_cast<size_t>(index_)];
		filter_store_ = sanitize_denormal_sample(output * (1.0f - damping) + filter_store_ * damping);
		buffer_[static_cast<size_t>(index_)] = sanitize_denormal_sample(input + filter_store_ * feedback);
		index_ = (index_ + 1) % static_cast<int>(buffer_.size());
		return output;
	}

private:
	std::vector<float> buffer_;
	int index_ {0};
	float filter_store_ {0.0f};
};

class ReverbAllPassFilter {
public:
	void prepare(int sample_count)
	{
		buffer_.assign(static_cast<size_t>(sample_count > 0 ? sample_count : 1), 0.0f);
		reset();
	}

	void reset()
	{
		std::fill(buffer_.begin(), buffer_.end(), 0.0f);
		index_ = 0;
	}

	float process(float input)
	{
		if(buffer_.empty())
			return input;

		const float buffered = buffer_[static_cast<size_t>(index_)];
		const float output = sanitize_denormal_sample(buffered - input);
		buffer_[static_cast<size_t>(index_)] = sanitize_denormal_sample(input + buffered * feedback());
		index_ = (index_ + 1) % static_cast<int>(buffer_.size());
		return output;
	}

private:
	static constexpr float feedback() { return 0.5f; }

	std::vector<float> buffer_;
	int index_ {0};
};

class ModulatedDelayEffectBase : public AudioEffect {
public:
	ModulatedDelayEffectBase(float delay_ms, float depth_ms, float rate_hz, float stereo_phase_deg)
		: delay_ms_ {sanitize_delay_ms(delay_ms)},
		  depth_ms_ {sanitize_depth_ms(depth_ms)},
		  rate_hz_ {sanitize_rate_hz(rate_hz)},
		  stereo_phase_deg_ {sanitize_stereo_phase_deg(stereo_phase_deg)}
	{
	}

	void prepare(int sample_rate, int channels) final
	{
		sample_rate_.store(sample_rate > 0 ? sample_rate : 44100, std::memory_order_relaxed);
		channel_count_.store(channels > 0 ? channels : 1, std::memory_order_relaxed);

		const int prepared_sample_rate = sample_rate_.load(std::memory_order_relaxed);
		const int prepared_channels = channel_count_.load(std::memory_order_relaxed);
		delay_buffer_frame_count_ = std::max(static_cast<int>(std::ceil(static_cast<float>(prepared_sample_rate) * max_supported_delay_ms() / 1000.0f)) + 2, 2);
		delay_buffer_.assign(static_cast<size_t>(delay_buffer_frame_count_ * prepared_channels), 0.0f);
		wet_frame_buffer_.assign(static_cast<size_t>(prepared_channels), 0.0f);
		reset();
	}

	void reset() final
	{
		std::fill(delay_buffer_.begin(), delay_buffer_.end(), 0.0f);
		std::fill(wet_frame_buffer_.begin(), wet_frame_buffer_.end(), 0.0f);
		write_frame_index_ = 0;
		lfo_phase_ = 0.0f;
	}

	void set_delay_ms(float delay_ms)
	{
		delay_ms_.store(sanitize_delay_ms(delay_ms), std::memory_order_relaxed);
	}

	float get_delay_ms() const
	{
		return delay_ms_.load(std::memory_order_relaxed);
	}

	void set_depth_ms(float depth_ms)
	{
		depth_ms_.store(sanitize_depth_ms(depth_ms), std::memory_order_relaxed);
	}

	float get_depth_ms() const
	{
		return depth_ms_.load(std::memory_order_relaxed);
	}

	void set_rate_hz(float rate_hz)
	{
		rate_hz_.store(sanitize_rate_hz(rate_hz), std::memory_order_relaxed);
	}

	float get_rate_hz() const
	{
		return rate_hz_.load(std::memory_order_relaxed);
	}

	void set_stereo_phase_deg(float stereo_phase_deg)
	{
		stereo_phase_deg_.store(sanitize_stereo_phase_deg(stereo_phase_deg), std::memory_order_relaxed);
	}

	float get_stereo_phase_deg() const
	{
		return stereo_phase_deg_.load(std::memory_order_relaxed);
	}

	void process(float *samples, int frame_count) final
	{
		if(!samples || frame_count <= 0 || delay_buffer_.empty())
			return;

		const int sample_rate = sample_rate_.load(std::memory_order_relaxed);
		const int channel_count = channel_count_.load(std::memory_order_relaxed);
		if(sample_rate <= 0 || channel_count <= 0)
			return;

		const float delay_ms = delay_ms_.load(std::memory_order_relaxed);
		const float depth_ms = depth_ms_.load(std::memory_order_relaxed);
		const float rate_hz = rate_hz_.load(std::memory_order_relaxed);
		const float stereo_phase_rad = degrees_to_radians(stereo_phase_deg_.load(std::memory_order_relaxed));
		const float phase_step = rate_hz > 0.0f ? rate_hz * two_pi() / static_cast<float>(sample_rate) : 0.0f;

		for(int frame = 0; frame < frame_count; ++frame)
		{
			const int frame_offset = frame * channel_count;
			const int write_frame_index = write_frame_index_;
			const size_t write_offset = static_cast<size_t>(write_frame_index * channel_count);

			for(int channel = 0; channel < channel_count; ++channel)
				wet_frame_buffer_[static_cast<size_t>(channel)] = read_modulated_sample(samples[frame_offset + channel], channel,
					write_frame_index, sample_rate, channel_count, delay_ms, depth_ms, stereo_phase_rad);

			for(int channel = 0; channel < channel_count; ++channel)
			{
				const float dry = samples[frame_offset + channel];
				const float wet = wet_frame_buffer_[static_cast<size_t>(channel)];
				samples[frame_offset + channel] = output_sample(dry, wet);
				delay_buffer_[write_offset + static_cast<size_t>(channel)] = delay_input_sample(dry, wet);
			}

			write_frame_index_ = (write_frame_index_ + 1) % delay_buffer_frame_count_;
			lfo_phase_ = normalize_phase(lfo_phase_ + phase_step);
		}
	}

	static constexpr float max_delay_ms_limit() { return 100.0f; }
	static constexpr float max_depth_ms_limit() { return 20.0f; }
	static constexpr float max_stereo_phase_deg_limit() { return 180.0f; }

protected:
	virtual float output_sample(float dry, float wet) const = 0;
	virtual float delay_input_sample(float dry, float wet) const = 0;

	static float sanitize_delay_ms(float delay_ms)
	{
		return std::clamp(delay_ms, 0.0f, max_delay_ms_limit());
	}

	static float sanitize_depth_ms(float depth_ms)
	{
		return std::clamp(depth_ms, 0.0f, max_depth_ms_limit());
	}

	static float sanitize_rate_hz(float rate_hz)
	{
		return rate_hz < 0.0f ? 0.0f : rate_hz;
	}

	static float sanitize_stereo_phase_deg(float stereo_phase_deg)
	{
		return std::clamp(stereo_phase_deg, 0.0f, max_stereo_phase_deg_limit());
	}

private:
	static constexpr float max_supported_delay_ms() { return max_delay_ms_limit() + max_depth_ms_limit(); }
	static constexpr float two_pi() { return 6.28318530717958647692f; }

	static float degrees_to_radians(float degrees)
	{
		return degrees * (two_pi() / 360.0f);
	}

	static float normalize_phase(float phase)
	{
		while(phase >= two_pi())
			phase -= two_pi();
		while(phase < 0.0f)
			phase += two_pi();
		return phase;
	}

	float read_modulated_sample(float dry, int channel, int write_frame_index, int sample_rate, int channel_count,
		float delay_ms, float depth_ms, float stereo_phase_rad) const
	{
		const float channel_phase = normalize_phase(lfo_phase_ + static_cast<float>(channel % 2) * stereo_phase_rad);
		float modulated_delay_frames = (delay_ms + depth_ms * std::sin(channel_phase)) * static_cast<float>(sample_rate) / 1000.0f;
		modulated_delay_frames = std::clamp(modulated_delay_frames, 0.0f, static_cast<float>(delay_buffer_frame_count_ - 1));
		if(modulated_delay_frames <= 0.0f)
			return dry;

		float read_position = static_cast<float>(write_frame_index) - modulated_delay_frames;
		while(read_position < 0.0f)
			read_position += static_cast<float>(delay_buffer_frame_count_);

		const int read_frame_index_0 = static_cast<int>(read_position);
		const int read_frame_index_1 = (read_frame_index_0 + 1) % delay_buffer_frame_count_;
		const float fraction = read_position - static_cast<float>(read_frame_index_0);
		const size_t read_offset_0 = static_cast<size_t>(read_frame_index_0 * channel_count + channel);
		const size_t read_offset_1 = static_cast<size_t>(read_frame_index_1 * channel_count + channel);

		const float wet_0 = delay_buffer_[read_offset_0];
		const float wet_1 = delay_buffer_[read_offset_1];
		return wet_0 + (wet_1 - wet_0) * fraction;
	}

	std::atomic<float> delay_ms_;
	std::atomic<float> depth_ms_;
	std::atomic<float> rate_hz_;
	std::atomic<float> stereo_phase_deg_;
	std::atomic<int> sample_rate_ {44100};
	std::atomic<int> channel_count_ {1};

	int delay_buffer_frame_count_ {0};
	int write_frame_index_ {0};
	float lfo_phase_ {0.0f};
	std::vector<float> delay_buffer_;
	std::vector<float> wet_frame_buffer_;
};

class MixedFeedbackModulatedDelayEffectBase : public ModulatedDelayEffectBase {
public:
	MixedFeedbackModulatedDelayEffectBase(float delay_ms, float depth_ms, float rate_hz, float mix, float feedback,
		float stereo_phase_deg, float min_feedback_limit, float max_feedback_limit)
		: ModulatedDelayEffectBase(delay_ms, depth_ms, rate_hz, stereo_phase_deg),
		  mix_ {sanitize_mix(mix)},
		  feedback_ {sanitize_feedback(feedback, min_feedback_limit, max_feedback_limit)},
		  min_feedback_limit_ {min_feedback_limit},
		  max_feedback_limit_ {max_feedback_limit < 0.0f ? 0.0f : max_feedback_limit}
	{
	}

	void set_mix(float mix)
	{
		mix_.store(sanitize_mix(mix), std::memory_order_relaxed);
	}

	float get_mix() const
	{
		return mix_.load(std::memory_order_relaxed);
	}

	void set_feedback(float feedback)
	{
		feedback_.store(sanitize_feedback(feedback, min_feedback_limit_, max_feedback_limit_), std::memory_order_relaxed);
	}

	float get_feedback() const
	{
		return feedback_.load(std::memory_order_relaxed);
	}

private:
	static float sanitize_mix(float mix)
	{
		return std::clamp(mix, 0.0f, 1.0f);
	}

	static float sanitize_feedback(float feedback, float min_feedback_limit, float max_feedback_limit)
	{
		return std::clamp(feedback, min_feedback_limit, max_feedback_limit);
	}

	float output_sample(float dry, float wet) const final
	{
		const float mix = mix_.load(std::memory_order_relaxed);
		return dry * (1.0f - mix) + wet * mix;
	}

	float delay_input_sample(float dry, float wet) const final
	{
		const float feedback = feedback_.load(std::memory_order_relaxed);
		return dry + wet * feedback;
	}

	std::atomic<float> mix_;
	std::atomic<float> feedback_;
	const float min_feedback_limit_;
	const float max_feedback_limit_;
};

} // namespace detail

class ReverbEffect final : public AudioEffect {
public:
	ReverbEffect(float room_size = 0.65f, float damping = 0.25f, float wet = 0.18f, float dry = 1.0f, float width = 1.0f)
		: room_size_ {sanitize_room_size(room_size)},
		  damping_ {sanitize_damping(damping)},
		  wet_ {sanitize_mix(wet)},
		  dry_ {sanitize_mix(dry)},
		  width_ {sanitize_width(width)}
	{
	}

	void prepare(int sample_rate, int channels) override
	{
		sample_rate_.store(sample_rate > 0 ? sample_rate : 44100, std::memory_order_relaxed);
		channel_count_.store(channels > 0 ? channels : 1, std::memory_order_relaxed);

		const int prepared_sample_rate = sample_rate_.load(std::memory_order_relaxed);
		const int prepared_channels = channel_count_.load(std::memory_order_relaxed);
		comb_filters_.clear();
		comb_filters_.resize(static_cast<size_t>(prepared_channels));
		allpass_filters_.clear();
		allpass_filters_.resize(static_cast<size_t>(prepared_channels));
		wet_frame_buffer_.assign(static_cast<size_t>(prepared_channels), 0.0f);
		dry_frame_buffer_.assign(static_cast<size_t>(prepared_channels), 0.0f);

		for(int channel = 0; channel < prepared_channels; ++channel)
		{
			auto &comb_bank = comb_filters_[static_cast<size_t>(channel)];
			for(size_t i = 0; i < comb_filter_count; ++i)
				comb_bank[i].prepare(scale_delay_frames(comb_tunings()[i], prepared_sample_rate, channel));

			auto &allpass_bank = allpass_filters_[static_cast<size_t>(channel)];
			for(size_t i = 0; i < allpass_filter_count; ++i)
				allpass_bank[i].prepare(scale_delay_frames(allpass_tunings()[i], prepared_sample_rate, channel));
		}

		reset();
	}

	void reset() override
	{
		for(auto &comb_bank : comb_filters_)
			for(auto &comb : comb_bank)
				comb.reset();

		for(auto &allpass_bank : allpass_filters_)
			for(auto &allpass : allpass_bank)
				allpass.reset();

		std::fill(wet_frame_buffer_.begin(), wet_frame_buffer_.end(), 0.0f);
		std::fill(dry_frame_buffer_.begin(), dry_frame_buffer_.end(), 0.0f);
	}

	void set_room_size(float room_size)
	{
		room_size_.store(sanitize_room_size(room_size), std::memory_order_relaxed);
	}

	float get_room_size() const
	{
		return room_size_.load(std::memory_order_relaxed);
	}

	void set_damping(float damping)
	{
		damping_.store(sanitize_damping(damping), std::memory_order_relaxed);
	}

	float get_damping() const
	{
		return damping_.load(std::memory_order_relaxed);
	}

	void set_wet(float wet)
	{
		wet_.store(sanitize_mix(wet), std::memory_order_relaxed);
	}

	float get_wet() const
	{
		return wet_.load(std::memory_order_relaxed);
	}

	void set_dry(float dry)
	{
		dry_.store(sanitize_mix(dry), std::memory_order_relaxed);
	}

	float get_dry() const
	{
		return dry_.load(std::memory_order_relaxed);
	}

	void set_width(float width)
	{
		width_.store(sanitize_width(width), std::memory_order_relaxed);
	}

	float get_width() const
	{
		return width_.load(std::memory_order_relaxed);
	}

	void process(float *samples, int frame_count) override
	{
		if(!samples || frame_count <= 0 || comb_filters_.empty() || allpass_filters_.empty())
			return;

		const int channel_count = channel_count_.load(std::memory_order_relaxed);
		if(channel_count <= 0)
			return;

		const float room_size = room_size_.load(std::memory_order_relaxed);
		const float damping = damping_.load(std::memory_order_relaxed);
		const float wet = wet_.load(std::memory_order_relaxed);
		const float dry = dry_.load(std::memory_order_relaxed);
		const float width = width_.load(std::memory_order_relaxed);
		const float comb_feedback = room_size_to_comb_feedback(room_size);
		const float comb_damping = damping_to_comb_damping(damping);
		const float wet_1 = channel_count == 2 ? wet * (width * 0.5f + 0.5f) : wet;
		const float wet_2 = channel_count == 2 ? wet * ((1.0f - width) * 0.5f) : 0.0f;

		for(int frame = 0; frame < frame_count; ++frame)
		{
			const int frame_offset = frame * channel_count;

			for(int channel = 0; channel < channel_count; ++channel)
				dry_frame_buffer_[static_cast<size_t>(channel)] = samples[frame_offset + channel];

			for(int channel = 0; channel < channel_count; ++channel)
			{
				float wet_sample = 0.0f;
				const float input = detail::sanitize_denormal_sample(dry_frame_buffer_[static_cast<size_t>(channel)] * input_gain());
				auto &comb_bank = comb_filters_[static_cast<size_t>(channel)];
				for(auto &comb : comb_bank)
					wet_sample += comb.process(input, comb_feedback, comb_damping);

				auto &allpass_bank = allpass_filters_[static_cast<size_t>(channel)];
				for(auto &allpass : allpass_bank)
					wet_sample = allpass.process(wet_sample);

				wet_frame_buffer_[static_cast<size_t>(channel)] = detail::sanitize_denormal_sample(wet_sample);
			}

			if(channel_count == 2)
			{
				const float wet_left = wet_frame_buffer_[0];
				const float wet_right = wet_frame_buffer_[1];
				samples[frame_offset] = detail::sanitize_denormal_sample(dry_frame_buffer_[0] * dry + wet_left * wet_1 + wet_right * wet_2);
				samples[frame_offset + 1] = detail::sanitize_denormal_sample(dry_frame_buffer_[1] * dry + wet_right * wet_1 + wet_left * wet_2);
			}
			else
			{
				for(int channel = 0; channel < channel_count; ++channel)
					samples[frame_offset + channel] = detail::sanitize_denormal_sample(
						dry_frame_buffer_[static_cast<size_t>(channel)] * dry + wet_frame_buffer_[static_cast<size_t>(channel)] * wet);
			}
		}
	}

private:
	static constexpr size_t comb_filter_count = 8;
	static constexpr size_t allpass_filter_count = 4;

	static constexpr float input_gain() { return 0.015f; }
	static constexpr int stereo_spread_samples() { return 23; }

	static constexpr std::array<int, comb_filter_count> comb_tunings()
	{
		return {1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
	}

	static constexpr std::array<int, allpass_filter_count> allpass_tunings()
	{
		return {556, 441, 341, 225};
	}

	static int scale_delay_frames(int tuning, int sample_rate, int channel)
	{
		const int stereo_spread = channel % 2 ? stereo_spread_samples() : 0;
		const float scale = static_cast<float>(sample_rate > 0 ? sample_rate : 44100) / 44100.0f;
		return std::max(static_cast<int>(std::lround(static_cast<float>(tuning + stereo_spread) * scale)), 1);
	}

	static float room_size_to_comb_feedback(float room_size)
	{
		return 0.7f + sanitize_room_size(room_size) * 0.28f;
	}

	static float damping_to_comb_damping(float damping)
	{
		return sanitize_damping(damping) * 0.4f;
	}

	static float sanitize_room_size(float room_size)
	{
		return std::clamp(room_size, 0.0f, 1.0f);
	}

	static float sanitize_damping(float damping)
	{
		return std::clamp(damping, 0.0f, 1.0f);
	}

	static float sanitize_mix(float mix)
	{
		return std::clamp(mix, 0.0f, 1.0f);
	}

	static float sanitize_width(float width)
	{
		return std::clamp(width, 0.0f, 1.0f);
	}

	std::atomic<int> sample_rate_ {44100};
	std::atomic<int> channel_count_ {1};
	std::atomic<float> room_size_;
	std::atomic<float> damping_;
	std::atomic<float> wet_;
	std::atomic<float> dry_;
	std::atomic<float> width_;

	std::vector<std::array<detail::ReverbCombFilter, comb_filter_count>> comb_filters_;
	std::vector<std::array<detail::ReverbAllPassFilter, allpass_filter_count>> allpass_filters_;
	std::vector<float> wet_frame_buffer_;
	std::vector<float> dry_frame_buffer_;
};

class VibratoEffect final : public detail::ModulatedDelayEffectBase {
public:
	VibratoEffect(float delay_ms = 6.0f, float depth_ms = 2.0f, float rate_hz = 4.5f, float stereo_phase_deg = 90.0f)
		: ModulatedDelayEffectBase(delay_ms, depth_ms, rate_hz, stereo_phase_deg)
	{
	}

private:
	float output_sample(float, float wet) const override
	{
		return wet;
	}

	float delay_input_sample(float dry, float) const override
	{
		return dry;
	}
};

class ChorusEffect final : public detail::MixedFeedbackModulatedDelayEffectBase {
public:
	ChorusEffect(float delay_ms = 18.0f, float depth_ms = 2.0f, float rate_hz = 0.25f, float mix = 0.35f,
				 float feedback = 0.0f, float stereo_phase_deg = 90.0f)
		: MixedFeedbackModulatedDelayEffectBase(delay_ms, depth_ms, rate_hz, mix, feedback, stereo_phase_deg,
			0.0f,
			max_feedback_limit())
	{
	}

private:
	static constexpr float max_feedback_limit() { return 0.95f; }
};

class FlangerEffect final : public detail::MixedFeedbackModulatedDelayEffectBase {
public:
	FlangerEffect(float delay_ms = 2.0f, float depth_ms = 1.5f, float rate_hz = 0.2f, float mix = 0.6f,
				  float feedback = 0.72f, float stereo_phase_deg = 90.0f)
		: MixedFeedbackModulatedDelayEffectBase(delay_ms, depth_ms, rate_hz, mix, feedback, stereo_phase_deg,
			min_feedback_limit(),
			max_feedback_limit())
	{
	}

private:
	static constexpr float min_feedback_limit() { return -0.98f; }
	static constexpr float max_feedback_limit() { return 0.98f; }
};

} // namespace majimix

#endif