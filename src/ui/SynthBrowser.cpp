#include "SynthBrowser.h"
#include "Palette.h"
#include "Fonts.h"

namespace bd
{

SynthBrowser::SynthBrowser (HostedPluginManager& h) : host (h)
{
    setWantsKeyboardFocus (true);

    search.setTextToShowWhenEmpty ("Search instruments...", pal::textFaint);
    search.setFont (fonts::mono (14.0f));
    search.onTextChange = [this] { refilter(); };
    search.onReturnKey  = [this] { loadRow (list.getSelectedRow() >= 0 ? list.getSelectedRow() : 0); };
    addAndMakeVisible (search);

    list.setRowHeight (34);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (list);

    scanButton.setTooltip ("Scan for new VST3 / AU instruments");
    scanButton.onClick = [this] { host.startScan (false); };
    rescanButton.setTooltip ("Forget the cached list and scan everything again");
    rescanButton.onClick = [this] { host.startScan (true); };
    loadButton.onClick = [this] { loadRow (list.getSelectedRow()); };
    closeButton.onClick = [this] { if (onClose) onClose(); };

    for (auto* b : { &scanButton, &rescanButton, &loadButton, &closeButton })
        addAndMakeVisible (b);

    host.addChangeListener (this);
    refilter();
}

SynthBrowser::~SynthBrowser()
{
    host.removeChangeListener (this);
}

void SynthBrowser::visibilityChanged()
{
    if (isVisible())
    {
        refilter();
        search.grabKeyboardFocus();

        // First time: nothing cached yet -> start scanning automatically.
        if (all.isEmpty() && ! host.isScanning())
            host.startScan (false);
    }
}

bool SynthBrowser::isKnownMpeSynth (const juce::PluginDescription& d)
{
    return HostedPluginManager::isKnownMpeSynth (d);
}

void SynthBrowser::refilter()
{
    all = host.getInstruments();
    filtered.clearQuick();

    const auto q = search.getText().trim().toLowerCase();
    for (auto& d : all)
    {
        const auto hay = (d.name + " " + d.manufacturerName + " " + d.pluginFormatName).toLowerCase();
        if (q.isEmpty() || hay.contains (q))
            filtered.add (d);
    }

    // MPE-capable synths first (they can slide every note on its own), then by name
    std::stable_sort (filtered.begin(), filtered.end(), [] (const juce::PluginDescription& a, const juce::PluginDescription& b)
    {
        const bool ma = isKnownMpeSynth (a), mb = isKnownMpeSynth (b);
        if (ma != mb) return ma;
        return a.name.compareIgnoreCase (b.name) < 0;
    });

    list.updateContent();
    if (filtered.size() > 0 && list.getSelectedRow() < 0)
        list.selectRow (0);
    repaint();
}

void SynthBrowser::changeListenerCallback (juce::ChangeBroadcaster*)
{
    if (host.isScanning())
        status = "SCANNING  " + juce::String (juce::roundToInt (host.getScanProgress() * 100.0f)) + "%   "
                 + juce::File (host.getScanStatus()).getFileName();
    else
        status = host.getLastError().isNotEmpty() ? ("ERROR: " + host.getLastError()) : host.getScanStatus();

    if (! host.isScanning())
        refilter();

    repaint();
}

void SynthBrowser::loadRow (int row)
{
    if (! juce::isPositiveAndBelow (row, filtered.size()))
        return;

    const auto desc = filtered[row];
    status = "LOADING " + desc.name + "...";
    repaint();

    juce::Component::SafePointer<SynthBrowser> safe (this);
    host.loadPlugin (desc, {}, [safe] (juce::String err)
    {
        if (safe == nullptr) return;
        if (err.isEmpty())
        {
            if (safe->onClose) safe->onClose();
        }
        else
        {
            safe->status = "ERROR: " + err;
            safe->repaint();
        }
    });
}

//==============================================================================
int SynthBrowser::getNumRows() { return filtered.size(); }

void SynthBrowser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (! juce::isPositiveAndBelow (row, filtered.size()))
        return;

    const auto& d = filtered.getReference (row);
    auto r = juce::Rectangle<int> (0, 0, w, h).reduced (4, 2).toFloat();

    if (selected)
    {
        g.setColour (pal::raised);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (pal::cyan.withAlpha (0.6f));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }

    const bool mpe = isKnownMpeSynth (d);
    auto rr = r.reduced (10.0f, 0.0f);
    if (mpe)
    {
        auto badge = rr.removeFromLeft (40.0f).withSizeKeepingCentre (40.0f, 18.0f);
        g.setColour (pal::cyan.withAlpha (0.16f));
        g.fillRoundedRectangle (badge, 3.0f);
        g.setColour (pal::cyan);
        g.drawRoundedRectangle (badge, 3.0f, 1.0f);
        g.setFont (fonts::ui (11.0f, true));
        g.drawText ("MPE", badge.toNearestInt(), juce::Justification::centred);
    }
    else
        rr.removeFromLeft (40.0f);
    rr.removeFromLeft (10.0f);

    g.setColour (pal::text);
    g.setFont (fonts::ui (14.0f, true));
    g.drawText (d.name, rr.withTrimmedRight (220).toNearestInt(), juce::Justification::centredLeft);

    g.setColour (pal::textDim);
    g.setFont (fonts::ui (12.0f));
    const juce::String dot (juce::CharPointer_UTF8 ("  \xc2\xb7  "));
    g.drawText (d.manufacturerName + dot + d.pluginFormatName + (mpe ? juce::String() : dot + "mono slides"),
                rr.toNearestInt(), juce::Justification::centredRight);
}

void SynthBrowser::listBoxItemDoubleClicked (int row, const juce::MouseEvent&) { loadRow (row); }
void SynthBrowser::returnKeyPressed (int row)                                  { loadRow (row); }

bool SynthBrowser::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { if (onClose) onClose(); return true; }
    if (k == juce::KeyPress::downKey)   { list.selectRow (juce::jmin (filtered.size() - 1, list.getSelectedRow() + 1)); return true; }
    if (k == juce::KeyPress::upKey)     { list.selectRow (juce::jmax (0, list.getSelectedRow() - 1)); return true; }
    return false;
}

//==============================================================================
juce::Rectangle<int> SynthBrowser::panelBounds() const
{
    return getLocalBounds().withSizeKeepingCentre (juce::jmin (640, getWidth() - 40), juce::jmin (560, getHeight() - 40));
}

void SynthBrowser::mouseDown (const juce::MouseEvent& e)
{
    if (! panelBounds().contains (e.getPosition()) && onClose)
        onClose();
}

void SynthBrowser::resized()
{
    auto r = panelBounds().reduced (20);
    auto header = r.removeFromTop (40);
    closeButton.setBounds (header.removeFromRight (32).withSizeKeepingCentre (32, 28));
    r.removeFromTop (8);

    auto row = r.removeFromTop (34);
    rescanButton.setBounds (row.removeFromRight (120));
    row.removeFromRight (6);
    scanButton.setBounds (row.removeFromRight (80));
    row.removeFromRight (6);
    search.setBounds (row);

    r.removeFromTop (10);
    auto bottom = r.removeFromBottom (40);
    loadButton.setBounds (bottom.removeFromRight (120).withSizeKeepingCentre (120, 32));
    r.removeFromBottom (6);
    list.setBounds (r);
}

void SynthBrowser::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.6f));

    auto panel = panelBounds().toFloat();
    g.setColour (pal::ground1);
    g.fillRoundedRectangle (panel, 8.0f);
    g.setColour (pal::hairline);
    g.drawRoundedRectangle (panel, 8.0f, 1.0f);

    auto r = panelBounds().reduced (20);
    auto header = r.removeFromTop (40);
    g.setColour (pal::text);
    g.setFont (fonts::ui (18.0f, true));
    g.drawText ("Load your synth", header, juce::Justification::centredLeft);

    g.setFont (fonts::ui (12.0f));
    g.setColour (pal::textDim);
    g.drawText (juce::String (filtered.size()) + " / " + juce::String (all.size()) + " instruments  -  MPE = every note slides on its own",
                header.withTrimmedLeft (170).withTrimmedRight (40), juce::Justification::centredRight);

    auto bottom = panelBounds().reduced (20).removeFromBottom (40).withTrimmedRight (130);
    g.setColour (status.startsWith ("ERROR") ? pal::magenta : pal::textDim);
    g.setFont (fonts::ui (12.0f));
    g.drawFittedText (status.isNotEmpty() ? status
                                          : juce::String ("Double-click or press Enter to load. Scan results are cached."),
                      bottom, juce::Justification::centredLeft, 2);

    if (host.isScanning())
    {
        auto bar = bottom.toFloat().removeFromBottom (3.0f);
        g.setColour (pal::hairline);
        g.fillRoundedRectangle (bar, 1.5f);
        g.setGradientFill (pal::ribbonGradient (bar.getTopLeft(), bar.getTopRight()));
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * host.getScanProgress()), 1.5f);
    }

    if (all.isEmpty() && ! host.isScanning())
    {
        g.setColour (pal::textDim);
        g.setFont (fonts::ui (13.0f));
        g.drawFittedText ("No instruments found yet. Press SCAN.", list.getBounds(), juce::Justification::centred, 2);
    }
}

} // namespace bd
