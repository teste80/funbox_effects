#include "daisy_petal.h"
#include "daisysp.h"
#include "funbox.h"

using namespace daisy;
using namespace daisysp;
using namespace funbox;

namespace
{
DaisyPetal hw;

Parameter drive_knob, tone_knob, level_knob, blend_knob, tight_knob, bias_knob;

Led led1, led2;

bool bypass = true;

bool pswitch1[2], pswitch2[2], pswitch3[2], pdip[4];
int  switch1[2], switch2[2], switch3[2], dip[4];

enum class ClipMode
{
    Soft,
    Hard,
    Asym,
};

enum class VoiceMode
{
    Full,
    Tight,
    Thick,
};

enum class OutputMode
{
    Mono,
    StereoSpread,
    DualMono,
};

ClipMode   clip_mode   = ClipMode::Soft;
VoiceMode  voice_mode  = VoiceMode::Full;
OutputMode output_mode = OutputMode::Mono;

Overdrive overdrive_l;
Overdrive overdrive_r;

float sample_rate = 48000.0f;

float smoothed_drive = 0.4f;
float smoothed_tone = 0.5f;
float smoothed_level = 0.7f;
float smoothed_blend = 1.0f;
float smoothed_tight = 0.3f;
float smoothed_bias = 0.5f;

float hp_state_l = 0.0f;
float hp_state_r = 0.0f;
float hp_x_l     = 0.0f;
float hp_x_r     = 0.0f;

float lp_state_l = 0.0f;
float lp_state_r = 0.0f;

float width_lfo_phase = 0.0f;

float clampf(float x, float lo, float hi)
{
    return fclamp(x, lo, hi);
}

float cubic_soft_clip(float x)
{
    const float clipped = clampf(x, -1.5f, 1.5f);
    return clipped - (clipped * clipped * clipped) / 3.0f;
}

float hard_clip(float x)
{
    return clampf(x, -0.85f, 0.85f);
}

float asym_clip(float x, float bias)
{
    const float positive_drive = 1.0f + bias * 2.5f;
    const float negative_drive = 1.0f + (1.0f - bias) * 2.0f;

    if(x >= 0.0f)
        return tanhf(x * positive_drive) * 0.8f;

    return tanhf(x * negative_drive) * 0.8f;
}

float one_pole_highpass(float x, float cutoff, float &state, float &x_prev)
{
    const float coeff = expf(-2.0f * PI_F * cutoff / sample_rate);
    const float y     = coeff * (state + x - x_prev);
    state             = y;
    x_prev            = x;
    return y;
}

float one_pole_lowpass(float x, float cutoff, float &state)
{
    const float coeff = 1.0f - expf(-2.0f * PI_F * cutoff / sample_rate);
    state += coeff * (x - state);
    return state;
}

void updateSwitch1()
{
    if(pswitch1[0])
        clip_mode = ClipMode::Soft;
    else if(pswitch1[1])
        clip_mode = ClipMode::Asym;
    else
        clip_mode = ClipMode::Hard;

    led2.Set(clip_mode == ClipMode::Soft ? 0.15f
             : clip_mode == ClipMode::Hard ? 0.55f
                                           : 1.0f);
}

void updateSwitch2()
{
    if(pswitch2[0])
        voice_mode = VoiceMode::Full;
    else if(pswitch2[1])
        voice_mode = VoiceMode::Thick;
    else
        voice_mode = VoiceMode::Tight;
}

void updateSwitch3()
{
    if(pswitch3[0])
        output_mode = OutputMode::Mono;
    else if(pswitch3[1])
        output_mode = OutputMode::DualMono;
    else
        output_mode = OutputMode::StereoSpread;
}

void UpdateButtons()
{
    if(hw.switches[Funbox::FOOTSWITCH_1].FallingEdge())
    {
        bypass = !bypass;
        led1.Set(bypass ? 0.0f : 1.0f);
    }

    led1.Update();
    led2.Update();
}

void UpdateSwitches()
{
    bool changed1 = false;
    for(int i = 0; i < 2; i++)
    {
        if(hw.switches[switch1[i]].Pressed() != pswitch1[i])
        {
            pswitch1[i] = hw.switches[switch1[i]].Pressed();
            changed1    = true;
        }
    }
    if(changed1)
        updateSwitch1();

    bool changed2 = false;
    for(int i = 0; i < 2; i++)
    {
        if(hw.switches[switch2[i]].Pressed() != pswitch2[i])
        {
            pswitch2[i] = hw.switches[switch2[i]].Pressed();
            changed2    = true;
        }
    }
    if(changed2)
        updateSwitch2();

    bool changed3 = false;
    for(int i = 0; i < 2; i++)
    {
        if(hw.switches[switch3[i]].Pressed() != pswitch3[i])
        {
            pswitch3[i] = hw.switches[switch3[i]].Pressed();
            changed3    = true;
        }
    }
    if(changed3)
        updateSwitch3();

    for(int i = 0; i < 4; i++)
    {
        if(hw.switches[dip[i]].Pressed() != pdip[i])
            pdip[i] = hw.switches[dip[i]].Pressed();
    }
}

float shape_sample(float x, float drive, float bias, Overdrive &od)
{
    switch(clip_mode)
    {
        case ClipMode::Soft:
            od.SetDrive(drive);
            return od.Process(x);
        case ClipMode::Hard: return hard_clip(x * (1.0f + drive * 10.0f));
        case ClipMode::Asym: return asym_clip(x * (1.0f + drive * 8.0f), bias);
    }
    return x;
}

static void AudioCallback(AudioHandle::InputBuffer in,
                          AudioHandle::OutputBuffer out,
                          size_t size)
{
    hw.ProcessAnalogControls();
    hw.ProcessDigitalControls();

    UpdateButtons();
    UpdateSwitches();

    fonepole(smoothed_drive, drive_knob.Process(), 0.002f);
    fonepole(smoothed_tone, tone_knob.Process(), 0.002f);
    fonepole(smoothed_level, level_knob.Process(), 0.002f);
    fonepole(smoothed_blend, blend_knob.Process(), 0.002f);
    fonepole(smoothed_tight, tight_knob.Process(), 0.002f);
    fonepole(smoothed_bias, bias_knob.Process(), 0.002f);

    const float tight_base = 60.0f + smoothed_tight * smoothed_tight * 900.0f;
    float hp_cutoff        = tight_base;
    if(voice_mode == VoiceMode::Tight)
        hp_cutoff += 220.0f;
    else if(voice_mode == VoiceMode::Thick)
        hp_cutoff *= 0.45f;

    float lp_cutoff = 1200.0f + smoothed_tone * smoothed_tone * 9000.0f;
    if(voice_mode == VoiceMode::Thick)
        lp_cutoff *= 0.78f;

    const float dry_amt  = 1.0f - smoothed_blend;
    const float wet_amt  = smoothed_blend;
    const float out_gain = 0.35f + smoothed_level * 1.2f;

    for(size_t i = 0; i < size; i++)
    {
        const float in_l = in[0][i];
        const float in_r = in[1][i];

        if(bypass)
        {
            out[0][i] = in_l;
            out[1][i] = in_r;
            continue;
        }

        float proc_l = one_pole_highpass(in_l, hp_cutoff, hp_state_l, hp_x_l);
        float proc_r = pdip[0] ? one_pole_highpass(in_r, hp_cutoff, hp_state_r, hp_x_r)
                               : proc_l;

        if(voice_mode == VoiceMode::Thick)
        {
            proc_l += in_l * 0.35f;
            proc_r += (pdip[0] ? in_r : in_l) * 0.35f;
        }

        const float pregain = 1.0f + smoothed_drive * smoothed_drive * 18.0f;
        proc_l *= pregain;
        proc_r *= pregain;

        const float bias = smoothed_bias;
        float wet_l      = shape_sample(proc_l, smoothed_drive, bias, overdrive_l);
        float wet_r      = shape_sample(proc_r, smoothed_drive, bias, overdrive_r);

        wet_l = cubic_soft_clip(wet_l);
        wet_r = cubic_soft_clip(wet_r);

        wet_l = one_pole_lowpass(wet_l, lp_cutoff, lp_state_l);
        wet_r = one_pole_lowpass(wet_r, lp_cutoff, lp_state_r);

        float left_mix  = in_l * dry_amt + wet_l * wet_amt;
        float right_mix = (pdip[0] ? in_r : in_l) * dry_amt + wet_r * wet_amt;

        if(output_mode == OutputMode::Mono)
        {
            const float mono = 0.5f * (left_mix + right_mix) * out_gain;
            out[0][i]        = mono;
            out[1][i]        = mono;
        }
        else if(output_mode == OutputMode::DualMono)
        {
            out[0][i] = left_mix * out_gain;
            out[1][i] = right_mix * out_gain;
        }
        else
        {
            width_lfo_phase += 0.35f / sample_rate;
            if(width_lfo_phase >= 1.0f)
                width_lfo_phase -= 1.0f;

            const float width = 0.12f + 0.08f * sinf(TWOPI_F * width_lfo_phase);
            out[0][i]         = (left_mix + wet_l * width) * out_gain;
            out[1][i]         = (right_mix - wet_r * width) * out_gain;
        }
    }
}
} // namespace

int main(void)
{
    hw.Init();
    hw.SetAudioBlockSize(48);
    sample_rate = hw.AudioSampleRate();

    overdrive_l.Init();
    overdrive_r.Init();
    overdrive_l.SetDrive(smoothed_drive);
    overdrive_r.SetDrive(smoothed_drive);

    switch1[0] = Funbox::SWITCH_1_LEFT;
    switch1[1] = Funbox::SWITCH_1_RIGHT;
    switch2[0] = Funbox::SWITCH_2_LEFT;
    switch2[1] = Funbox::SWITCH_2_RIGHT;
    switch3[0] = Funbox::SWITCH_3_LEFT;
    switch3[1] = Funbox::SWITCH_3_RIGHT;
    dip[0]     = Funbox::SWITCH_DIP_1;
    dip[1]     = Funbox::SWITCH_DIP_2;
    dip[2]     = Funbox::SWITCH_DIP_3;
    dip[3]     = Funbox::SWITCH_DIP_4;

    for(int i = 0; i < 2; i++)
    {
        pswitch1[i] = false;
        pswitch2[i] = false;
        pswitch3[i] = false;
    }
    for(int i = 0; i < 4; i++)
        pdip[i] = false;

    drive_knob.Init(hw.knob[Funbox::KNOB_1], 0.0f, 1.0f, Parameter::LINEAR);
    tone_knob.Init(hw.knob[Funbox::KNOB_2], 0.0f, 1.0f, Parameter::LINEAR);
    level_knob.Init(hw.knob[Funbox::KNOB_3], 0.0f, 1.0f, Parameter::LINEAR);
    blend_knob.Init(hw.knob[Funbox::KNOB_4], 0.0f, 1.0f, Parameter::LINEAR);
    tight_knob.Init(hw.knob[Funbox::KNOB_5], 0.0f, 1.0f, Parameter::LINEAR);
    bias_knob.Init(hw.knob[Funbox::KNOB_6], 0.0f, 1.0f, Parameter::LINEAR);

    led1.Init(hw.seed.GetPin(Funbox::LED_1), false);
    led2.Init(hw.seed.GetPin(Funbox::LED_2), false);
    led1.Set(0.0f);
    led2.Set(0.15f);
    led1.Update();
    led2.Update();

    updateSwitch1();
    updateSwitch2();
    updateSwitch3();

    hw.StartAdc();
    hw.StartAudio(AudioCallback);

    while(1)
    {
        System::Delay(10);
    }
}
