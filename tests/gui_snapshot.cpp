// Rende l'interfaccia in PNG senza schermo (xvfb-run) e prova il processore con un accordo.
#include "PluginProcessor.h"
#include "PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String out = argc > 1 ? argv[1] : "snapshot.png";
    const float scale = argc > 2 ? juce::String (argv[2]).getFloatValue() : 1.0f;

    VksProcessor proc;
    proc.setPlayConfigDetails (0, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    // qualche nota per verificare il percorso audio del processore
    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
    midi.addEvent (juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100), 100);
    float peak = 0.0f;
    for (int b = 0; b < 200; ++b)
    {
        buf.clear();
        proc.processBlock (buf, midi);
        midi.clear();
        peak = juce::jmax (peak, buf.getMagnitude (0, 0, 512));
    }
    std::printf ("picco uscita processore: %.3f, LED: %.2f\n", peak, proc.lfoLed());

    // apprendimento MIDI: CC 20 sulla risonanza
    proc.startLearn (proc.paramIndexOf ("f_res"));
    midi.addEvent (juce::MidiMessage::controllerEvent (1, 20, 100), 0);
    proc.processBlock (buf, midi); midi.clear();
    std::printf ("CC appreso per f_res: %d, valore: %.3f\n", proc.ccForParam (proc.paramIndexOf ("f_res")),
                 proc.apvts.getRawParameterValue ("f_res")->load());

    // stato salvato e ricaricato
    juce::MemoryBlock mb; proc.getStateInformation (mb);
    VksProcessor p2; p2.setStateInformation (mb.getData(), (int) mb.getSize());
    std::printf ("dopo ricarica: CC per f_res = %d\n", p2.ccForParam (p2.paramIndexOf ("f_res")));

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize ((int) (900 * scale), (int) (530 * scale));
    proc.keyboardState.noteOn (1, 60, 1.0f);
    proc.keyboardState.noteOn (1, 64, 1.0f);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    {
        // costo del disegno: pannello intero e solo la zona del LED (ridisegnata fino a 30 volte al secondo)
        auto t0 = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < 20; ++i) ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto t1 = juce::Time::getMillisecondCounterHiRes();
        const auto ledArea = juce::Rectangle<int> ((int) (110 * scale), (int) (30 * scale), (int) (16 * scale), (int) (16 * scale));
        for (int i = 0; i < 200; ++i) ed->createComponentSnapshot (ledArea, true, 1.0f);
        auto t2 = juce::Time::getMillisecondCounterHiRes();
        std::printf ("disegno: pannello intero %.2f ms, zona LED %.3f ms\n", (t1 - t0) / 20.0, (t2 - t1) / 200.0);
    }
    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (out));
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("salvato %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
