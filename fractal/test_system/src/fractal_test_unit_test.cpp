void test_str_to_offset() {
    bool crash = false;
    // Test valid decimal input
    myassert(str_to_offset("123") == 123,"str_to_offset_integer", crash);

    // Test valid hexadecimal input
    myassert(str_to_offset("0x1A3") == 419,"str_to_offset_hex_1", crash);

    // Test another valid hexadecimal input
    myassert(str_to_offset("0X1a3") == 419,"str_to_offset_hex_2", crash);

    // Test valid hexadecimal with capital letters
    myassert(str_to_offset("0XABC") == 2748,"str_to_offset_hex_3", crash);

    // Test valid negative decimal
    myassert(str_to_offset("-10") == -10,"str_to_offset_neg", crash);

    // Test valid negative hexadecimal
    myassert(str_to_offset("-0x1A") == -26,"str_to_offset_neg_hex", crash);

    // Test edge case of minimal non-zero integers
    myassert(str_to_offset("1") == 1,"str_to_offset_one", crash);
    myassert(str_to_offset("0x1") == 1,"str_to_offset_hex_one", crash);

    // Test zero
    myassert(str_to_offset("0") == 0,"str_to_offset_zero", crash);
    myassert(str_to_offset("0x0") == 0,"str_to_offset_hex_zero", crash);

    // // Test invalid inputs
    // try {
    //     str_to_offset("hello");
    //     myassert(false && "str_to_offset should have thrown an exception for 'hello'");
    // } catch (const std::exception&) {
    //     // Expected path
    // }

    // try {
    //     str_to_offset("0x1G");
    //     myassert(false && "str_to_offset should have thrown an exception for '0x1G'");
    // } catch (const std::exception&) {
    //     // Expected path
    // }
}

void test_decode_name() {
    std::string sm_name, item;
    int index;
    bool crash = false;

    // Test valid input with index
    decode_name(sm_name, item, index, "rtos:min_soc[2]");
    myassert(sm_name == "rtos" && item == "min_soc" && index == 2, "decode_name_with_index", crash);

    // Test valid input without index
    decode_name(sm_name, item, index, "rtos:min_soc");
    myassert(sm_name == "rtos" && item == "min_soc" && index == -1, "decode_name_without_index", crash);

    // Test valid input with improper format (missing index closing bracket)
    decode_name(sm_name, item, index, "rtos:min_soc[2");
    myassert(sm_name.empty() && item.empty() && index == -1, "decode_name_improper_format", crash);

    // Test input with no colon
    decode_name(sm_name, item, index, "rtosmin_soc");
    myassert(sm_name.empty() && item.empty() && index == -1, "decode_name_no_colon", crash);

    // Additional tests can be added as needed
}
 
void test_match_system() {
    bool crash = false; // Flag to check for test failures

    // Test case 1: Exact match without weights or tolerances
    std::shared_ptr<MatchObject> match1 = std::make_shared<MatchObject>();
    match1->vector = {100, 200, 300};
    std::vector<uint16_t> input1 = {100, 200, 300};

    double result1 = calculate_match_score(input1, match1);
    myassert(result1 == 1.0, "test_match_system|Exact match without weights or tolerances", crash);

    // Test case 2: Match with tolerances
    std::shared_ptr<MatchObject> match2 = std::make_shared<MatchObject>();
    match2->vector = {100, 205, 300};
    match2->tolerance = {0, 10, 0}; // Tolerances where only the second element has a non-zero tolerance
    std::vector<uint16_t> input2 = {100, 200, 300};

    double result2 = calculate_match_score(input2, match2);
    myassert(result2 == 1.0, "test_match_system|Match with tolerances allowing variation in the second element", crash);

    // Test case 3: Match with weights
    std::shared_ptr<MatchObject> match3 = std::make_shared<MatchObject>();
    match3->vector = {100, 200, 300};
    match3->weight = {100, 50, 25};  // Different weights for each element
    std::vector<uint16_t> input3 = {100, 200, 300};
    std::vector<uint16_t> input31 = {100, 100, 300};
    std::vector<uint16_t> input32 = {100, 200, 100};
    std::vector<uint16_t> input33 = {50, 200, 300};

    double result3 = calculate_match_score(input3, match3);
    std::cout << " result with input3 " << result3 << std::endl;
    double result31 = calculate_match_score(input31, match3);
    std::cout << " result with input31 " << result31 << std::endl;
    double result32 = calculate_match_score(input32, match3);
    std::cout << " result with input32 " << result32 << std::endl;
    double result33 = calculate_match_score(input33, match3);
    std::cout << " result with input33 " << result33 << std::endl;

    myassert(result3 > 0.58, "test_match_system|Match with varying weights", crash);

    // Test case 4: Match with mask
    std::shared_ptr<MatchObject> match4 = std::make_shared<MatchObject>();
    match4->vector = {0x10, 0x10, 0x10};
    match4->mask = {0xff, 0xff, 0xff};  // Masks for each element
    std::vector<uint16_t> input4 = {0x10, 0x110, 0x1110};

    double result4 = calculate_match_score(input4, match4);
    myassert(result4 == 1.0, "test_match_system|Match with masks", crash);

    // Test case 5: No match due to out of tolerance
    std::shared_ptr<MatchObject> match5 = std::make_shared<MatchObject>();
    match5->vector = {100, 250, 300};
    match5->tolerance = {5, 5, 5}; // Tolerance too small for second element
    std::vector<uint16_t> input5 = {100, 200, 300};

    double result5 = calculate_match_score(input5, match5);
    myassert(result5 == 0.0, "test_match_system|No match due to tolerance being too small", crash);


}

