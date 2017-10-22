#include <memory>
#include <deque>
#include <mutex>
#include <chrono>
#include <stdexcept>
#include <condition_variable>

namespace rpool {

template <class T, class D_, int wait_> class PoolImpl;

template <class T, class D_, int wait_>
class resource_deleter {
public:
    resource_deleter(std::shared_ptr<PoolImpl<T, D_, wait_>> pool):pool_{pool} {}

    void operator()(T * object) {
        std::shared_ptr<PoolImpl<T, D_, wait_>> origin = pool_.lock();
        if (origin)
            origin->add(object);
        else
            delete object;
    }

private:
    std::weak_ptr<PoolImpl<T, D_, wait_>> pool_;
};

template <class T, class D_, int wait_>
class PoolImpl: public std::enable_shared_from_this<PoolImpl<T, D_, wait_>> {
public:
    void add(T * object) {
        std::unique_lock<std::mutex> lock(mut_);
        pool_.push_front(std::shared_ptr<T>(object, resource_deleter<T, D_, wait_>(
            std::enable_shared_from_this<PoolImpl<T, D_, wait_>>::shared_from_this())));

        object_avl_.notify_one();
    }

    std::shared_ptr<T> acquire() {
        std::unique_lock<std::mutex> lock(mut_);

        while(pool_.empty()) {
            if (!wait_)
                return std::shared_ptr<T>(nullptr);

            if (object_avl_.wait_for(lock, D_(wait_)) == std::cv_status::timeout)
                return std::shared_ptr<T>();
        }

        std::shared_ptr<T> object =  pool_.front();
        pool_.pop_front();

        return object;
    }

private:
    std::mutex mut_;
    std::condition_variable object_avl_;
    std::deque<std::shared_ptr<T>> pool_;
};

template <class T, class D_ = std::chrono::milliseconds, int wait_ = 0>
class Pool {
public:
    void add(T * object) {
        pool_->add(object);
    }

    std::shared_ptr<T> acquire() {
        return pool_->acquire();
    }

private:
    std::shared_ptr<PoolImpl<T, D_, wait_>> pool_{std::make_shared<PoolImpl<T, D_, wait_>>()};
};

}