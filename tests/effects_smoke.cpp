#include <cmath>
#include <iostream>

#include <majimix/effects.hpp>

namespace {

bool nearly_equal(float lhs, float rhs, float epsilon = 1e-4f)
{
	return std::fabs(lhs - rhs) <= epsilon;
}

bool expect(bool condition, const char *message)
{
	if(condition)
		return true;

	std::cerr << "effects_smoke failure: " << message << "\n";
	return false;
}

bool test_gain_effect()
{
	majimix::GainEffect effect(0.5f);
	effect.prepare(44100, 2);

	float samples[] = {1.0f, -1.0f, 0.5f, -0.5f};
	effect.process(samples, 2);

	return expect(nearly_equal(samples[0], 0.5f), "gain left sample mismatch") &&
		expect(nearly_equal(samples[1], -0.5f), "gain right sample mismatch") &&
		expect(nearly_equal(samples[2], 0.25f), "gain third sample mismatch") &&
		expect(nearly_equal(samples[3], -0.25f), "gain fourth sample mismatch");
}

bool test_fade_in_helper()
{
	majimix::FadeEffect effect(0.0f);
	effect.prepare(1000, 2);
	effect.fade_in(4);

	float samples[] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
	effect.process(samples, 4);

	return expect(nearly_equal(samples[0], 0.0f), "fade_in frame 0 mismatch") &&
		expect(nearly_equal(samples[2], 0.25f), "fade_in frame 1 mismatch") &&
		expect(nearly_equal(samples[4], 0.5f), "fade_in frame 2 mismatch") &&
		expect(nearly_equal(samples[6], 0.75f), "fade_in frame 3 mismatch") &&
		expect(nearly_equal(effect.get_current_gain(), 1.0f), "fade_in final gain mismatch") &&
		expect(nearly_equal(effect.get_target_gain(), 1.0f), "fade_in target gain mismatch");
}

bool test_fade_out_helper()
{
	majimix::FadeEffect effect(1.0f);
	effect.prepare(1000, 2);
	effect.fade_out(4);

	float samples[] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
	effect.process(samples, 4);

	return expect(nearly_equal(samples[0], 1.0f), "fade_out frame 0 mismatch") &&
		expect(nearly_equal(samples[2], 0.75f), "fade_out frame 1 mismatch") &&
		expect(nearly_equal(samples[4], 0.5f), "fade_out frame 2 mismatch") &&
		expect(nearly_equal(samples[6], 0.25f), "fade_out frame 3 mismatch") &&
		expect(nearly_equal(effect.get_current_gain(), 0.0f), "fade_out final gain mismatch") &&
		expect(nearly_equal(effect.get_target_gain(), 0.0f), "fade_out target gain mismatch");
}

bool test_immediate_gain_update()
{
	majimix::FadeEffect effect(0.0f);
	effect.prepare(48000, 2);
	effect.set_gain(0.25f);

	float samples[] = {1.0f, 1.0f};
	effect.process(samples, 1);

	return expect(nearly_equal(samples[0], 0.25f), "immediate gain left mismatch") &&
		expect(nearly_equal(samples[1], 0.25f), "immediate gain right mismatch") &&
		expect(nearly_equal(effect.get_current_gain(), 0.25f), "immediate gain state mismatch");
}

bool test_reverb_dry_passthrough()
{
	majimix::ReverbEffect effect(0.65f, 0.25f, 0.0f, 1.0f, 1.0f);
	effect.prepare(1000, 1);

	float samples[] = {1.0f, -0.5f, 0.25f};
	effect.process(samples, 3);

	return expect(nearly_equal(samples[0], 1.0f), "reverb passthrough sample 0 mismatch") &&
		expect(nearly_equal(samples[1], -0.5f), "reverb passthrough sample 1 mismatch") &&
		expect(nearly_equal(samples[2], 0.25f), "reverb passthrough sample 2 mismatch");
}

bool test_reverb_impulse_tail()
{
	majimix::ReverbEffect effect(0.7f, 0.2f, 1.0f, 0.0f, 1.0f);
	effect.prepare(1000, 1);

	float samples[64] = {};
	samples[0] = 1.0f;
	effect.process(samples, 64);

	float max_tail = 0.0f;
	for(int i = 1; i < 64; ++i)
	{
		const float magnitude = std::fabs(samples[i]);
		if(magnitude > max_tail)
			max_tail = magnitude;
	}

	return expect(nearly_equal(samples[0], 0.0f), "reverb first sample mismatch") &&
		expect(max_tail > 1e-4f, "reverb tail missing");
}

bool test_reverb_reset_clears_tail()
{
	majimix::ReverbEffect effect(0.7f, 0.2f, 1.0f, 0.0f, 1.0f);
	effect.prepare(1000, 1);

	float impulse[64] = {};
	impulse[0] = 1.0f;
	effect.process(impulse, 64);

	effect.reset();

	float silence[64] = {};
	effect.process(silence, 64);

	float max_output = 0.0f;
	for(float sample : silence)
	{
		const float magnitude = std::fabs(sample);
		if(magnitude > max_output)
			max_output = magnitude;
	}

	return expect(max_output <= 1e-6f, "reverb reset tail mismatch");
}

bool test_reverb_parameter_clamp()
{
	majimix::ReverbEffect effect;
	effect.set_room_size(-5.0f);
	effect.set_damping(2.0f);
	effect.set_wet(-1.0f);
	effect.set_dry(2.0f);
	effect.set_width(-2.0f);
	const bool lower_width_clamp_ok = nearly_equal(effect.get_width(), 0.0f);
	effect.set_width(2.0f);

	return expect(nearly_equal(effect.get_room_size(), 0.0f), "reverb room size clamp mismatch") &&
		expect(nearly_equal(effect.get_damping(), 1.0f), "reverb damping clamp mismatch") &&
		expect(nearly_equal(effect.get_wet(), 0.0f), "reverb wet clamp mismatch") &&
		expect(nearly_equal(effect.get_dry(), 1.0f), "reverb dry clamp mismatch") &&
		expect(lower_width_clamp_ok, "reverb lower width clamp mismatch") &&
		expect(nearly_equal(effect.get_width(), 1.0f), "reverb width clamp mismatch");
}

bool test_vibrato_zero_delay_passthrough()
{
	majimix::VibratoEffect effect(0.0f, 0.0f, 0.0f);
	effect.prepare(1000, 1);

	float samples[] = {1.0f, -0.5f, 0.25f};
	effect.process(samples, 3);

	return expect(nearly_equal(samples[0], 1.0f), "vibrato passthrough sample 0 mismatch") &&
		expect(nearly_equal(samples[1], -0.5f), "vibrato passthrough sample 1 mismatch") &&
		expect(nearly_equal(samples[2], 0.25f), "vibrato passthrough sample 2 mismatch");
}

bool test_vibrato_fixed_delay()
{
	majimix::VibratoEffect effect(1.0f, 0.0f, 0.0f);
	effect.prepare(1000, 1);

	float samples[] = {1.0f, 0.0f, 0.0f, 0.0f};
	effect.process(samples, 4);

	return expect(nearly_equal(samples[0], 0.0f), "vibrato delay sample 0 mismatch") &&
		expect(nearly_equal(samples[1], 1.0f), "vibrato delay sample 1 mismatch") &&
		expect(nearly_equal(samples[2], 0.0f), "vibrato delay sample 2 mismatch") &&
		expect(nearly_equal(samples[3], 0.0f), "vibrato delay sample 3 mismatch");
}

bool test_vibrato_parameter_clamp()
{
	majimix::VibratoEffect effect;
	effect.set_delay_ms(-5.0f);
	effect.set_depth_ms(999.0f);
	effect.set_rate_hz(-2.0f);
	effect.set_stereo_phase_deg(999.0f);

	return expect(nearly_equal(effect.get_delay_ms(), 0.0f), "vibrato delay clamp mismatch") &&
		expect(nearly_equal(effect.get_depth_ms(), 20.0f), "vibrato depth clamp mismatch") &&
		expect(nearly_equal(effect.get_rate_hz(), 0.0f), "vibrato rate clamp mismatch") &&
		expect(nearly_equal(effect.get_stereo_phase_deg(), 180.0f), "vibrato stereo phase clamp mismatch");
}

bool test_chorus_passthrough_mix_zero()
{
	majimix::ChorusEffect effect(1.0f, 0.0f, 0.0f, 0.0f);
	effect.prepare(1000, 1);

	float samples[] = {1.0f, -0.5f, 0.25f};
	effect.process(samples, 3);

	return expect(nearly_equal(samples[0], 1.0f), "chorus passthrough sample 0 mismatch") &&
		expect(nearly_equal(samples[1], -0.5f), "chorus passthrough sample 1 mismatch") &&
		expect(nearly_equal(samples[2], 0.25f), "chorus passthrough sample 2 mismatch");
}

bool test_chorus_fixed_delay()
{
	majimix::ChorusEffect effect(1.0f, 0.0f, 0.0f, 1.0f);
	effect.prepare(1000, 1);

	float samples[] = {1.0f, 0.0f, 0.0f, 0.0f};
	effect.process(samples, 4);

	return expect(nearly_equal(samples[0], 0.0f), "chorus delay sample 0 mismatch") &&
		expect(nearly_equal(samples[1], 1.0f), "chorus delay sample 1 mismatch") &&
		expect(nearly_equal(samples[2], 0.0f), "chorus delay sample 2 mismatch") &&
		expect(nearly_equal(samples[3], 0.0f), "chorus delay sample 3 mismatch");
}

bool test_chorus_feedback_tail()
{
	majimix::ChorusEffect effect(1.0f, 0.0f, 0.0f, 1.0f, 0.5f);
	effect.prepare(1000, 1);

	float samples[] = {1.0f, 0.0f, 0.0f, 0.0f};
	effect.process(samples, 4);

	return expect(nearly_equal(samples[0], 0.0f), "chorus feedback sample 0 mismatch") &&
		expect(nearly_equal(samples[1], 1.0f), "chorus feedback sample 1 mismatch") &&
		expect(nearly_equal(samples[2], 0.5f), "chorus feedback sample 2 mismatch") &&
		expect(nearly_equal(samples[3], 0.25f), "chorus feedback sample 3 mismatch");
}

bool test_chorus_parameter_clamp()
{
	majimix::ChorusEffect effect;
	effect.set_delay_ms(-5.0f);
	effect.set_depth_ms(999.0f);
	effect.set_rate_hz(-2.0f);
	effect.set_mix(2.0f);
	effect.set_feedback(2.0f);
	effect.set_stereo_phase_deg(999.0f);

	return expect(nearly_equal(effect.get_delay_ms(), 0.0f), "chorus delay clamp mismatch") &&
		expect(nearly_equal(effect.get_depth_ms(), 20.0f), "chorus depth clamp mismatch") &&
		expect(nearly_equal(effect.get_rate_hz(), 0.0f), "chorus rate clamp mismatch") &&
		expect(nearly_equal(effect.get_mix(), 1.0f), "chorus mix clamp mismatch") &&
		expect(nearly_equal(effect.get_feedback(), 0.95f), "chorus feedback clamp mismatch") &&
		expect(nearly_equal(effect.get_stereo_phase_deg(), 180.0f), "chorus stereo phase clamp mismatch");
}

bool test_flanger_fixed_delay_feedback_tail()
{
	majimix::FlangerEffect effect(1.0f, 0.0f, 0.0f, 1.0f, 0.75f);
	effect.prepare(1000, 1);

	float samples[] = {1.0f, 0.0f, 0.0f, 0.0f};
	effect.process(samples, 4);

	return expect(nearly_equal(samples[0], 0.0f), "flanger feedback sample 0 mismatch") &&
		expect(nearly_equal(samples[1], 1.0f), "flanger feedback sample 1 mismatch") &&
		expect(nearly_equal(samples[2], 0.75f), "flanger feedback sample 2 mismatch") &&
		expect(nearly_equal(samples[3], 0.5625f), "flanger feedback sample 3 mismatch");
}

bool test_flanger_negative_feedback_tail()
{
	majimix::FlangerEffect effect(1.0f, 0.0f, 0.0f, 1.0f, -0.75f);
	effect.prepare(1000, 1);

	float samples[] = {1.0f, 0.0f, 0.0f, 0.0f};
	effect.process(samples, 4);

	return expect(nearly_equal(samples[0], 0.0f), "flanger negative feedback sample 0 mismatch") &&
		expect(nearly_equal(samples[1], 1.0f), "flanger negative feedback sample 1 mismatch") &&
		expect(nearly_equal(samples[2], -0.75f), "flanger negative feedback sample 2 mismatch") &&
		expect(nearly_equal(samples[3], 0.5625f), "flanger negative feedback sample 3 mismatch");
}

bool test_flanger_parameter_clamp()
{
	majimix::FlangerEffect effect;
	effect.set_delay_ms(-5.0f);
	effect.set_depth_ms(999.0f);
	effect.set_rate_hz(-2.0f);
	effect.set_mix(2.0f);
	effect.set_feedback(-2.0f);
	const bool lower_clamp_ok = nearly_equal(effect.get_feedback(), -0.98f);
	effect.set_feedback(2.0f);
	effect.set_stereo_phase_deg(999.0f);

	return expect(nearly_equal(effect.get_delay_ms(), 0.0f), "flanger delay clamp mismatch") &&
		expect(nearly_equal(effect.get_depth_ms(), 20.0f), "flanger depth clamp mismatch") &&
		expect(nearly_equal(effect.get_rate_hz(), 0.0f), "flanger rate clamp mismatch") &&
		expect(nearly_equal(effect.get_mix(), 1.0f), "flanger mix clamp mismatch") &&
		expect(lower_clamp_ok, "flanger negative feedback clamp mismatch") &&
		expect(nearly_equal(effect.get_feedback(), 0.98f), "flanger feedback clamp mismatch") &&
		expect(nearly_equal(effect.get_stereo_phase_deg(), 180.0f), "flanger stereo phase clamp mismatch");
}

} // namespace

int main()
{
	if(!test_gain_effect())
		return 1;
	if(!test_fade_in_helper())
		return 1;
	if(!test_fade_out_helper())
		return 1;
	if(!test_immediate_gain_update())
		return 1;
	if(!test_reverb_dry_passthrough())
		return 1;
	if(!test_reverb_impulse_tail())
		return 1;
	if(!test_reverb_reset_clears_tail())
		return 1;
	if(!test_reverb_parameter_clamp())
		return 1;
	if(!test_vibrato_zero_delay_passthrough())
		return 1;
	if(!test_vibrato_fixed_delay())
		return 1;
	if(!test_vibrato_parameter_clamp())
		return 1;
	if(!test_chorus_passthrough_mix_zero())
		return 1;
	if(!test_chorus_fixed_delay())
		return 1;
	if(!test_chorus_feedback_tail())
		return 1;
	if(!test_chorus_parameter_clamp())
		return 1;
	if(!test_flanger_fixed_delay_feedback_tail())
		return 1;
	if(!test_flanger_negative_feedback_tail())
		return 1;
	if(!test_flanger_parameter_clamp())
		return 1;

	std::cout << "effects_smoke ok\n";
	return 0;
}