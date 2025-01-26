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
