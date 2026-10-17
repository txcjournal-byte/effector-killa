/*
    TrapWin BEND DEMON
    HostKeys.cpp
*/
#if defined (_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #include <windows.h>
#endif

#include "HostKeys.h"

namespace bd::hostkeys
{
#if defined (_WIN32)
static HWND hostWindowFor (juce::Component& c)
{
    if (auto* peer = c.getPeer())
        if (auto h = (HWND) peer->getNativeHandle())
            if (auto root = GetAncestor (h, GA_ROOT); root != nullptr && root != h)
                return root;
    return nullptr;
}
#endif

void returnFocusToHost (juce::Component& c)
{
    juce::Component::unfocusAllComponents();
   #if defined (_WIN32)
    if (auto host = hostWindowFor (c))
        SetFocus (host);
   #else
    juce::ignoreUnused (c);
   #endif
}

bool forwardToHost (juce::Component& c, const juce::KeyPress& key)
{
   #if defined (_WIN32)
    auto host = hostWindowFor (c);
    if (host == nullptr)
        return false;

    juce::Component::unfocusAllComponents();
    SetFocus (host);

    WORD vk = 0;
    const int code = key.getKeyCode();
    if (code == juce::KeyPress::spaceKey)             vk = VK_SPACE;
    else if (code == juce::KeyPress::returnKey)       vk = VK_RETURN;
    else if (code >= 'A' && code <= 'Z')              vk = (WORD) code;
    else if (code >= 'a' && code <= 'z')              vk = (WORD) (code - 'a' + 'A');
    else if (code >= '0' && code <= '9')              vk = (WORD) code;
    if (vk == 0)
        return true;   // focus is back with the host; the next press goes straight there

    INPUT in[2] = {};
    in[0].type = INPUT_KEYBOARD;  in[0].ki.wVk = vk;
    in[1].type = INPUT_KEYBOARD;  in[1].ki.wVk = vk;  in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput (2, in, sizeof (INPUT));
    return true;
   #else
    juce::ignoreUnused (c, key);
    return false;   // macOS / Linux: an unused key travels up to the host's view
   #endif
}
}
