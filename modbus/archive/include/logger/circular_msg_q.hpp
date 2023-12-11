#ifndef CIRCULAR_MSG_Q_HPP
#define CIRCULAR_MSG_Q_HPP

#include <vector>
#include "spdlog/spdlog.h"
#include "spdlog/details/file_helper.h"

// nickname: Ouroboros (the snake that eats its own tail)
// however: the tail cannot eat the head.
// If you set backtrace size = 0, you have undefined behaviour.
// neat trick: reSize to 0 then reSize to a larger number to flush out the previous messages and start a new.
class circular_msg_q
{
    std::size_t mHead; // newest
    std::size_t mTail; // oldest
    std::size_t mSize; // # of actual messages
    std::vector<spdlog::memory_buf_t> mBuf; // stores messages

    bool empty() const
    {
        return mSize == 0;
    }

    spdlog::memory_buf_t& oldest()
    {
        auto& msg = mBuf[mTail];
        mTail = (mTail + 1) % mBuf.capacity();
        mSize -= ((mSize - 1) < mBuf.capacity()); // branchless resize
        mHead = (mHead + (mTail == mHead)) % mBuf.capacity(); // branchless head shift
        return msg;
    }

public:
    circular_msg_q() = delete; // no default construct
    circular_msg_q(std::size_t size)
        : mHead(0), mTail(0), mSize(0), mBuf(size)
    {}
    ~circular_msg_q() = default;

    // no copies
    circular_msg_q(const circular_msg_q&) = delete;
    circular_msg_q& operator=(const circular_msg_q&) = delete;

    // no moves
    circular_msg_q(circular_msg_q&&) = delete;
    circular_msg_q& operator=(circular_msg_q&&) = delete;

    // NOTE: you must write to this message or the old one will be kept there.
    // if you want to make your own sink, look at how rotating_event_sink does it.
    spdlog::memory_buf_t& newest()
    {
        auto& msg = mBuf[mHead];
        mHead = (mHead + 1) % mBuf.capacity();
        mSize += ((mSize + 1) <= mBuf.capacity()); // branchless resize
        mTail = (mTail + (mHead != mTail && mSize == mBuf.capacity())) % mBuf.capacity(); // branchless tail shift
        msg.clear(); // our newest message is the one we're going to write to, make sure we flush the old data.
        return msg;
    }

    void reSize(std::size_t size)
    {
        std::size_t tempHead = 0;
        std::vector<spdlog::memory_buf_t> tempBuf{size};
        if(size == 0){
            mHead = 0;
            mTail = 0;
            mBuf = std::move(tempBuf);
            mSize = 0;
            return;
        }

        if (mSize <= size) // moving to >= capacity() than what I have -> mSize <= new capacity();
        {
            std::size_t oldSize = mSize; // todo: see if this can get better?
            while (!empty())
            {
                tempBuf[tempHead] = std::move(oldest());
                ++tempHead;
            }
            mSize = oldSize;
        }
        else // moving to < capacity() than what I have -> mSize > new capacity();
        {
            // todo: check this resize, might need tempTail to be just size - head -> correct, that works
            // that is correct
            std::size_t tempTail = (mBuf.capacity() - (size - mHead)) % mBuf.capacity();
            while (tempHead != size)
            {
                tempBuf[tempHead] = std::move(mBuf[tempTail]);
                tempTail = (tempTail + 1) % mBuf.capacity();
                ++tempHead;
            }
            mSize = size;
        }
        mHead = tempHead % size;
        mTail = 0; // oldest message should always be in the first slot, even if we loop around once
        mBuf = std::move(tempBuf);
    }

    void dumpToFile(spdlog::details::file_helper& helper)
    {
        std::size_t tempTail = mTail;
        for (std::size_t i = 0; i < mSize; ++i)
        {
            helper.write(mBuf[tempTail]);
            tempTail = (tempTail + 1) % mBuf.capacity();
        }
    }
};

#endif