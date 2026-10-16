/*
    TrapWin BEND DEMON - engine unit test runner (juce::UnitTest).
    Exit code = number of failed tests (0 = all passed).
*/
#include <juce_core/juce_core.h>

int main (int argc, char* argv[])
{
    juce::ignoreUnused (argc, argv);

    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false);
    runner.runTestsInCategory ("BendDemon");

    int failures = 0, passes = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        failures += runner.getResult (i)->failures;
        passes   += runner.getResult (i)->passes;
    }

    std::printf ("\nBEND DEMON engine tests: %d checks passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
