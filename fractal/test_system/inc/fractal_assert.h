#ifndef _MYASSERT_DEF
#define _MYASSERT_DEF
#include <string>

class AssertManager {
public:
    struct AssertInfo {
        std::string category;
        std::string message;
        bool passed;
    };

    std::vector<AssertInfo> assert_log;

    // Log an assertion
    void log_assertion(const std::string& category, const std::string& message, bool passed) {
        assert_log.push_back({category, message, passed});
    }

    // Generate a summary
    void generate_summary() const {
        int total = assert_log.size();
        int passed = 0;
        for (const auto& assert : assert_log) {
            if (assert.passed) passed++;
        }
        std::cout << "\n--- Test Summary ---\n";
        std::cout << "Total: " << total << ", Passed: " << passed << ", Failed: " << (total - passed) << "\n";
    }

    // Generate a detailed report
    void generate_report() const {
        std::cout << "\n--- Detailed Test Report ---\n";
        for (const auto& assert : assert_log) {
            std::cout << "[" << (assert.passed ? "PASS" : "FAIL") << "] "
                      << "Category: " << assert.category
                      << " | Message: " << assert.message << "\n";
        }
    }
};

// Global instance for assertion management
AssertManager assert_manager;

#define myassert(condition, category, crash)                               \
    do {                                                                   \
        bool _result = (condition);                                         \
        assert_manager.log_assertion(category, #condition, _result);        \
        if (!_result) {                                                     \
            std::cerr << "[ASSERT FAIL] Category: " << category            \
                      << " | Condition: " << #condition << std::endl;      \
            if (crash) {                                                   \
                std::cerr << "Exiting due to failed assertion.\n";         \
                std::terminate();                                          \
            }                                                              \
        }                                                                  \
    } while (0)

#endif