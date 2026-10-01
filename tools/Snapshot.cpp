// MFR-Mayo - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Renders the plugin UI to a PNG without opening any audio device.
// (The standalone app would start whistling through the speakers immediately.)
// Usage: MayoSnapshot out.png [scale]
#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::puts ("usage: MayoSnapshot out.png [scale]");
        return 1;
    }

    juce::ScopedJuceInitialiser_GUI gui;
    MayoProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditorIfNeeded());

    const float scale = argc > 2 ? (float) std::atof (argv[2]) : 1.0f;
    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);

    juce::File file (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    file.deleteFile();
    juce::FileOutputStream out (file);
    const bool ok = out.openedOk() && juce::PNGImageFormat().writeImageToStream (image, out);
    std::printf ("%s %s (%dx%d)\n", ok ? "wrote" : "FAILED", file.getFullPathName().toRawUTF8(), image.getWidth(), image.getHeight());

    editor.reset();
    return ok ? 0 : 1;
}
