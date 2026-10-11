#include <juce_events/juce_events.h>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);

    // runTestsInCategory() clears the previous results, so count the failures after every run
    int failures = 0;
    auto run = [&] (const juce::String& category)
    {
        runner.runTestsInCategory (category);
        for (int i = 0; i < runner.getNumResults(); ++i)
            if (auto* r = runner.getResult (i)) failures += r->failures;
    };

    if (argc > 1) run (argv[1]);
    else
    {
        // everything except the (slow, machine dependent) benchmark
        for (auto& cat : juce::UnitTest::getAllCategories())
            if (cat != "benchmark") run (cat);
    }

    std::printf ("\n==== %d failure(s) ====\n", failures);
    return failures == 0 ? 0 : 1;
}
