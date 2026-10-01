MFR-Mayo - free fuzz + microphonic squeal guitar effect by Music For Robots
https://musicforrobots.com
Source code: https://github.com/music-for-robots/MFR-Mayo

INSTALLING (zip version)
  Copy the MFR-Mayo.vst3 folder into:
      C:\Program Files\Common Files\VST3\
  Then rescan plugins in your DAW (in FL Studio: Options > Manage plugins
  > Find plugins). MFR-Mayo.exe is a standalone version: pick your audio
  interface under Options, and plug your guitar in.

WHAT IT DOES
  A vintage op-amp style fuzz into a cranked British amp and a 4x12 cab,
  with a cheap, microphonic pickup in the loop: when you stop playing,
  the rig starts to whistle. Play through it, stop, and let it sing.

  GATE    First in the chain. Keeps the rig quiet when you're not playing,
          and decides how long the whistle rings after you stop (Fade).
          If the plugin stays silent, lower the Threshold; if it whines
          when you're not playing, raise it.
  FUZZ    Sustain, Tone and Volume.
  AMP     Drive and EQ. Switch AMP + CAB off to use your own amp sim
          (the squeal still works).
  SQUEAL  Squeal: how microphonic the pickup is (0 = off).
          Pitch: where the whistle sits.
          Distance: guitar-to-amp distance; changes which notes it jumps to.
          Drift: how much the whistle wanders and sags.
          Bloom: how pure and slow-building the whistle is.

  Knobs: double-click or Alt-click resets to the default.
         Ctrl- or Shift-drag gives fine control.
         Click the value under a knob to type in a number.
  INIT resets every control.

CAREFUL
  With the gate off, the plugin whistles continuously whenever nothing
  is playing. Mind your speakers and headphones.

LICENSE
  MFR-Mayo is free software under the GNU AGPL v3 or later (see
  LICENSE.txt). Free to use in your music, including commercial
  releases. No warranty.

  VST is a trademark of Steinberg Media Technologies GmbH. Big Muff is a
  trademark of Electro-Harmonix; Marshall and JCM800 are trademarks of
  Marshall Amplification. MFR-Mayo is an independent project, not
  affiliated with or endorsed by any of them.
