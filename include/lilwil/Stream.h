#pragma once
#include <mutex>
#include <vector>
#include <ostream>

namespace lilwil {

/******************************************************************************/

/// std::ostream synchronizer for redirection from multiple threads
struct StreamSync {
    std::ostream &stream;
    std::streambuf *original; // never changed (unless by user)
    std::mutex mutex;
    std::vector<std::streambuf *> queue;
};

extern StreamSync cout_sync;
extern StreamSync cerr_sync;

/// RAII acquisition of cout or cerr
struct RedirectStream {
    StreamSync &sync;
    std::streambuf * const buf;

    RedirectStream(StreamSync &s, std::streambuf *b) : sync(s), buf(b) {
        if (!buf) return;
        std::lock_guard<std::mutex> lk(sync.mutex);
        if (sync.queue.empty()) sync.stream.rdbuf(buf); // take over the stream
        else sync.queue.push_back(buf); // or add to queue
    }

    ~RedirectStream() {
        if (!buf) return;
        std::lock_guard<std::mutex> lk(sync.mutex);
        auto it = std::find(sync.queue.begin(), sync.queue.end(), buf);
        if (it != sync.queue.end()) sync.queue.erase(it); // remove from queue
        else if (sync.queue.empty()) sync.stream.rdbuf(sync.original); // set to original
        else { // let next waiting stream take over
            sync.stream.rdbuf(sync.queue[0]);
            sync.queue.erase(sync.queue.begin());
        }
    }
};

/******************************************************************************/

}