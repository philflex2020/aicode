// functions for decoding and encoding (only for Holding and Input registers):

u64 decode_raw(const u16* raw_registers, const Decode_Info& current_decode, std::any &decode_output)
{
    u64 raw_data = 0UL;

    u64 current_unsigned_val = 0UL;
    s64 current_signed_val = 0;
    f64 current_float_val = 0.0;

    if (current_decode.flags.is_size_one())
    {
        raw_data = raw_registers[0];

        current_unsigned_val = raw_data;
        if (current_decode.flags.uses_masks())
        {
            // invert mask stuff:
            current_unsigned_val ^= current_decode.invert_mask;
            // care mask stuff:
            current_unsigned_val &= current_decode.care_mask;
        }
        // do signed stuff (size 1 cannot be float):
        if (current_decode.flags.is_signed())
        {
            s16 to_reinterpret = 0;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_signed_val = to_reinterpret;
        }
    }
    else if (current_decode.flags.is_size_two())
    {
        raw_data =  (static_cast<u64>(raw_registers[0]) << 16) +
                    (static_cast<u64>(raw_registers[1]) <<  0);

        if (!current_decode.flags.is_word_swap()) // normal:
        {
            current_unsigned_val = raw_data;
        }
        else // swap:
        {
            current_unsigned_val =  (static_cast<u64>(raw_registers[0]) <<  0) + 
                                    (static_cast<u64>(raw_registers[1]) << 16);
        }
        if (current_decode.flags.uses_masks())
        {
            // invert mask stuff:
            current_unsigned_val ^= current_decode.invert_mask;
            // care mask stuff:
            current_unsigned_val &= current_decode.care_mask;
        }
        // do signed/float stuff:
        if (current_decode.flags.is_signed())
        {
            s32 to_reinterpret = 0;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_signed_val = to_reinterpret;
        }
        else if (current_decode.flags.is_float())
        {
            f32 to_reinterpret = 0.0f;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_float_val = to_reinterpret;
        }
    }
    else // size of 4:
    {
        raw_data =  (static_cast<u64>(raw_registers[0]) << 48) +
                    (static_cast<u64>(raw_registers[1]) << 32) +
                    (static_cast<u64>(raw_registers[2]) << 16) +
                    (static_cast<u64>(raw_registers[3]) <<  0);

        if (!current_decode.flags.is_word_swap()) // normal:
        {
            current_unsigned_val = raw_data;
        }
        else // swap:
        {
            current_unsigned_val =  (static_cast<u64>(raw_registers[0]) <<  0) +
                                    (static_cast<u64>(raw_registers[1]) << 16) +
                                    (static_cast<u64>(raw_registers[2]) << 32) +
                                    (static_cast<u64>(raw_registers[3]) << 48);
        }
        if (current_decode.flags.uses_masks())
        {
            // invert mask stuff:
            current_unsigned_val ^= current_decode.invert_mask;
            // care mask stuff:
            current_unsigned_val &= current_decode.care_mask;
        }
        // do signed/float stuff:
        if (current_decode.flags.is_signed()) // this is only for whole numbers really (signed but not really float):
        {
            s64 to_reinterpret = 0;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_signed_val = to_reinterpret;
        }
        else if (current_decode.flags.is_float())
        {
            f64 to_reinterpret = 0.0;
            memcpy(&to_reinterpret, &current_unsigned_val, sizeof(to_reinterpret));
            current_float_val = to_reinterpret;
        }
    }

    // do scaling/shift stuff at the end:
    if (current_decode.flags.is_float())
    {
        if (current_decode.scale) // scale != 0
        {
            current_float_val /= current_decode.scale;
        }
        current_float_val += static_cast<f64>(current_decode.shift); // add shift
        current_jval = current_float_val;
    }
    else if (!current_decode.flags.is_signed()) // unsigned whole numbers:
    {
        if (!current_decode.scale) // scale == 0
        {
            current_unsigned_val += current_decode.shift; // add shift
            current_unsigned_val >>= current_decode.starting_bit_pos;
            current_jval = current_unsigned_val;
        }
        else
        {
            auto scaled = static_cast<f64>(current_unsigned_val) / current_decode.scale;
            scaled += static_cast<f64>(current_decode.shift); // add shift
            current_jval = scaled;
        }
    }
    else // signed whole numbers:
    {
        if (!current_decode.scale) // scale == 0
        {
            current_signed_val += current_decode.shift; // add shift
            current_signed_val >>= current_decode.starting_bit_pos;
            current_jval = current_signed_val;
        }
        else
        {
            auto scaled = static_cast<f64>(current_signed_val) / current_decode.scale;
            scaled += static_cast<f64>(current_decode.shift); // add shift
            current_jval = scaled;
        }
    }
    return raw_data;
}
