#include <juce_events/juce_events.h>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    if (argc > 1) runner.runTestsInCategory (argv[1]);
    else runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        if (auto* r = runner.getResult (i)) failures += r->failures;
    std::printf ("\n==== %d failure(s) ====\n", failures);
    return failures == 0 ? 0 : 1;
}
