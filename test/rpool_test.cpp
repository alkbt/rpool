#include <atomic>
#include <thread>
#include <chrono>
#include <vector>

#include <gtest/gtest.h>

#include "../include/rpool.hpp"

TEST(rpool_test_case, simple_test)
{
    char * x = new char[5];
    rpool::Pool<char> pool;
    pool.add(x);

    auto y = pool.acquire();

    ASSERT_EQ(y.get(), x);
}

struct LiveCounter {
    LiveCounter(std::atomic<int>& live_objects_count)
                :live_objects_count(live_objects_count) {
        ++live_objects_count;
    }

    ~LiveCounter() {
        --live_objects_count;
    };

    std::atomic<int>& live_objects_count;
};

TEST(rpool_test_case, resource_deletion_test)
{
    std::atomic<int> live_objects_count{0};

    {
        rpool::Pool<LiveCounter> pool;

        for (int i = 0; i < 10; ++i)
            pool.add(new LiveCounter(live_objects_count));

        ASSERT_EQ(live_objects_count, 10);
    }

    ASSERT_EQ(live_objects_count, 0) << "There are "
                                     << live_objects_count
                                     << " objects still alive!";
}

TEST(rpool_test_case, deletion_after_pool_test)
{
    std::atomic<int> live_objects_count{0};

    {
        std::shared_ptr<LiveCounter> resource;

        {
            rpool::Pool<LiveCounter> pool;

            for (int i = 0; i < 10; ++i)
                pool.add(new LiveCounter(live_objects_count));

            resource = pool.acquire();
        }

        ASSERT_EQ(live_objects_count, 1)
                << "The object must remain after removing the pool";
    }

    ASSERT_EQ(live_objects_count, 0) << "There are "
                                     << live_objects_count
                                     << " objects still alive!";
}

TEST(rpool_test_case, no_wait_for_objects_test)
{
    std::atomic<int> live_objects_count{0};
    std::atomic<int> null_objs{0};

    {
        rpool::Pool<LiveCounter> pool;

        for (int i = 0; i < 10; ++i)
            pool.add(new LiveCounter(live_objects_count));

        auto res_processor = [&]() {
            for (int i = 0; i < 10; ++i) {
                auto object = pool.acquire();
                if (!object)
                    ++null_objs;

                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        };

        std::vector<std::thread> threads;

        for (int i = 0; i < 100; ++i)
            threads.emplace_back(res_processor);

        for (auto& thread : threads)
            thread.join();
    }

    ASSERT_EQ(live_objects_count, 0) << "There are "
                                     << live_objects_count
                                     << " objects still alive!";

    ASSERT_GT(null_objs, 0) << "Test is broken!";
}

TEST(rpool_test_case, wait_for_objects_test)
{
    std::atomic<int> live_objects_count{0};
    std::atomic<int> null_objs{0};

    {
        rpool::Pool<LiveCounter, std::chrono::hours, 1> pool;

        for (int i = 0; i < 10; ++i)
            pool.add(new LiveCounter(live_objects_count));

        auto res_processor = [&]() {
            for (int i = 0; i < 10; ++i) {
                auto object = pool.acquire();
                if (!object)
                    ++null_objs;

                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
        };

        std::vector<std::thread> threads;

        for (int i = 0; i < 100; ++i)
            threads.emplace_back(res_processor);

        for (auto& thread : threads)
            thread.join();
    }

    ASSERT_EQ(live_objects_count, 0) << "There are "
                                     << live_objects_count
                                     << " objects still alive!";

    ASSERT_EQ(null_objs, 0) << "Wait is broken!";
}

TEST(rpool_test_case, move_constructor_test)
{
    std::atomic<int> live_objects_count{0};

    auto create_pool = [&]() {
        rpool::Pool<LiveCounter> pool;
        for (int i = 0; i < 10; ++i)
            pool.add(new LiveCounter(live_objects_count));

        return pool;
    };

    {
        auto pool = create_pool();
        for (int i = 0; i < 10; ++i)
            auto object = pool.acquire();
    }

    ASSERT_EQ(live_objects_count, 0) << "There are "
                                     << live_objects_count
                                     << " objects still alive!";
}

int main(int argc, char ** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}