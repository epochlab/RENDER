#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <catch2/catch_test_case_info.hpp>
#include <iostream>

struct ColourListener : Catch::EventListenerBase {
    using Catch::EventListenerBase::EventListenerBase;

    void testCaseEnded(Catch::TestCaseStats const& s) override {
        if (s.totals.assertions.allPassed())
            std::cout << "\033[32m  ✓ PASSED\033[0m  " << s.testInfo->name << "\n";
        else
            std::cout << "\033[31m  ✗ FAILED\033[0m  " << s.testInfo->name << "\n";
    }
};

CATCH_REGISTER_LISTENER(ColourListener)
