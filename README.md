AlloAudio
=========

This is a simple jack application to handle the final output stage for the AlloSphere's audio system. It can do:

* Level and mute management
* Room compensation (from precomputed FIR Impulse responses)
* Bass management (with optional cross-over filters)
* Send output levels to external metering interface

OSC Namespace
=============

Send
----

### /Alloaudio/meter i f ###

Meter value for chanels (in dB FS), e.g.

    /Alloaudio/meter 4 -3.455

corresponds to a level of -3.455 dB FS in channel 5 (index 4).

Receive
-------

### /Alloaudio/global_gain f

Set global gain to f.

### /Alloaudio/gain i f

Sets individual channel gains by channel index (channel 1 = index 0)

### /Alloaudio/mute_all i

If set to 1, mute all audio, set to 0 to unmute.

### /Alloaudio/clipper_on i

Clip any audio that goes above the global gain value if set to 1. If set to 0 no clipping occurs.

### /Alloaudio/room_compensation_on i

Turn room compensation filters on if set to 1. Disabled if set to 0.

### /Alloaudio/bass_management_freq f

Choose frequency in Hz for bass_management cross-over filters. See bass management mode below.

### /Alloaudio/bass_management_mode i

Choose bass management mode:

* 0 : No bass management. All channels are routed directly (BASSMODE_NONE)
* 1 : Signals from all channels are routed to subwoofers, but no cross-over filters are applied (BASSMODE_MIX)
* 2 : Signals from all channels are routed to subwoofers, and low pass filters are applied to the SW channels (BASSMODE_LOWPASS)
* 3 : Signals from all channels are routed to subwoofers, and high-pass filters are applied to all channels that are not SW (BASSMODE_HIGHPASS)
* 4 : Signals from all channels are routed to subwoofers, and both cross-over filters are applied (BASSMODE_FULL)

The cross-over filters are fourth-order Linkwitz-Riley cross-over filters. You can set the cross-over frequency with the message /Alloaudio/bass_management_freq.

### /Alloaudio/sw_indeces iiii

Set the indeces for the SW channels. Currently four subwoofers are supported. For example if only channel 6 (index 5) is a subwoofer, you would send:

    /Alloaudio/sw_indeces 5 -1 -1 -1

If channels 31 and 32 are subwoofers, send:

    /Alloaudio/sw_indeces 30 31 -1 -1

### /Alloaudio/meter_on i

Turn on or off sending of meter values.
