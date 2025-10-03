#include "mpsc_queue.hpp"
#include <iostream>
#include <thread>

using namespace std::literals::chrono_literals;

int main (void)
{
    ngg::mpsc_queue<int> queue;
    auto lambda1 = [&queue] (std::stop_token token)
    {
        int i = 0;
        while (!token.stop_requested())
        {
            // std::this_thread::sleep_for(1s);
            queue.push(i);
            i += 2;
        }
        std::cout << "1: Max number: " << i << '\n';
    };
    auto lambda2 = [&queue] (std::stop_token token)
    {
        int i = 1;
        while (!token.stop_requested())
        {
            // std::this_thread::sleep_for(1s);
            queue.push(i);
            i += 2;
        }
        std::cout << "2: Max number: " << i << '\n';
    };
    auto lambda_consumer = [&queue] (std::stop_token token)
    {
        long long count = 0;
        while (!token.stop_requested())
        {
            if (auto v = queue.pull())
            {
                ++count;
            }
        }
        while (auto v = queue.pull())
        {
            ++count;
        }
        std::cout << "Total lines outputted: " << count << '\n';
    };
    std::jthread t1(lambda1);
    std::jthread t2(lambda2);
    std::jthread t_consumer(lambda_consumer);
    std::this_thread::sleep_for(10s);
    t1.request_stop();
    t2.request_stop();
    t_consumer.request_stop();
}
