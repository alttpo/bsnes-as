#pragma once

#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>

#if 0
#include <future>
#endif

template<typename T>
class blocking_queue {
public:
  template<typename Q = T>
  typename std::enable_if<std::is_copy_constructible<Q>::value, void>::type
  push(const T &item) {
    {
      std::unique_lock lock(m_mutex);
      m_queue.push(item);
    }
    m_ready.notify_one();
  }

  template<typename Q = T>
  typename std::enable_if<std::is_move_constructible<Q>::value, void>::type
  push(T &&item) {
    {
      std::unique_lock lock(m_mutex);
      m_queue.emplace(std::forward<T>(item));
    }
    m_ready.notify_one();
  }

  template<typename Q = T>
  typename std::enable_if<std::is_copy_constructible<Q>::value, bool>::type
  try_push(const T &item) {
    {
      std::unique_lock lock(m_mutex, std::try_to_lock);
      if (!lock)
        return false;
      m_queue.push(item);
    }
    m_ready.notify_one();
    return true;
  }

  template<typename Q = T>
  typename std::enable_if<std::is_move_constructible<Q>::value, bool>::type
  try_push(T &&item) {
    {
      std::unique_lock lock(m_mutex, std::try_to_lock);
      if (!lock)
        return false;
      m_queue.emplace(std::forward<T>(item));
    }
    m_ready.notify_one();
    return true;
  }

  template<typename Q = T>
  typename std::enable_if<
    std::is_copy_assignable<Q>::value &&
    !std::is_move_assignable<Q>::value, bool>::type
  pop(T &item) {
    std::unique_lock lock(m_mutex);
    while (m_queue.empty() && !m_done) {
      m_ready.wait(lock);
    }
    if (m_queue.empty())
      return false;
    item = m_queue.front();
    m_queue.pop();
    return true;
  }

  template<typename Q = T>
  typename std::enable_if<std::is_move_assignable<Q>::value, bool>::type
  pop(T &item) {
    std::unique_lock lock(m_mutex);
    while (m_queue.empty() && !m_done) {
      m_ready.wait(lock);
    }
    if (m_queue.empty())
      return false;
    item = std::move(m_queue.front());
    m_queue.pop();
    return true;
  }

  template<typename Q = T>
  typename std::enable_if<
    std::is_copy_assignable<Q>::value &&
    !std::is_move_assignable<Q>::value, bool>::type
  try_pop(T &item) {
    std::unique_lock lock(m_mutex, std::try_to_lock);
    if (!lock || m_queue.empty())
      return false;
    item = m_queue.front();
    m_queue.pop();
    return true;
  }

  template<typename Q = T>
  typename std::enable_if<std::is_move_assignable<Q>::value, bool>::type
  try_pop(T &item) {
    std::unique_lock lock(m_mutex, std::try_to_lock);
    if (!lock || m_queue.empty())
      return false;
    item = std::move(m_queue.front());
    m_queue.pop();
    return true;
  }

  void done() noexcept {
    {
      std::unique_lock lock(m_mutex);
      m_done = true;
    }
    m_ready.notify_all();
  }

  bool empty() const noexcept {
    std::scoped_lock lock(m_mutex);
    return m_queue.empty();
  }

  unsigned int size() const noexcept {
    std::scoped_lock lock(m_mutex);
    return m_queue.size();
  }

private:
  std::queue<T> m_queue;
  mutable std::mutex m_mutex;
  std::condition_variable m_ready;
  bool m_done = false;
};

class simple_thread_pool {
public:
  const int thread_count;

  explicit simple_thread_pool(unsigned int threads = std::thread::hardware_concurrency())
    : thread_count(threads)
  {
    if (!threads)
      throw std::invalid_argument("Invalid thread count!");

    m_ran = 0;
    auto worker = [this]() {
      while (true) {
        Proc f;
        if (!m_queue.pop(f))
          break;
        f();

        m_ran++;
      }
    };

    for (auto i = 0; i < threads; ++i)
      m_threads.emplace_back(worker);
  }

  ~simple_thread_pool() {
    m_queue.done();
    for (auto &thread : m_threads)
      thread.join();
  }

  template<typename F, typename... Args>
  void enqueue_work(F &&f, Args &&... args) {
    m_count++;
    m_queue.push([p = std::forward<F>(f), t = std::make_tuple(std::forward<Args>(args)...)]() { std::apply(p, t); });
  }

  void wait() {
    // wait until last task is complete:
    while (m_ran < m_count) {
      //std::this_thread::yield();
    }
    m_count = 0;
    m_ran = 0;
  }

#if 0
  template<typename F, typename... Args>
  [[nodiscard]] auto enqueue_task(F &&f, Args &&... args) -> std::future<std::invoke_result_t<F, Args...>> {
    using task_return_type = std::invoke_result_t<F, Args...>;
    using task_type = std::packaged_task<task_return_type()>;

    auto task = std::make_shared<task_type>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    auto result = task->get_future();

    m_queue.push([task]() { (*task)(); });

    return result;
  }
#endif

private:
  using Proc = std::function<void(void)>;
  using Queue = blocking_queue<Proc>;
  Queue m_queue;

  using Threads = std::vector<std::thread>;
  Threads m_threads;

  std::mutex alldone_lock;
  std::condition_variable cv_alldone;
  std::atomic<int> m_ran;
  std::atomic<int> m_count;
};
