#pragma once

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>

#if 0
#include <future>
#endif

#if 0
class semaphore {
public:
  explicit semaphore(unsigned int count = 0) noexcept
    : m_count(count) {}

  void post() noexcept {
    {
      std::unique_lock lock(m_mutex);
      ++m_count;
    }
    m_cv.notify_one();
  }

  void post(unsigned int count) noexcept {
    {
      std::unique_lock lock(m_mutex);
      m_count += count;
    }
    m_cv.notify_all();
  }

  void wait() noexcept {
    std::unique_lock lock(m_mutex);
    m_cv.wait(lock, [this]() { return m_count != 0; });
    --m_count;
  }

  template<typename T>
  bool wait_for(T &&t) noexcept {
    std::unique_lock lock(m_mutex);
    if (!m_cv.wait_for(lock, t, [this]() { return m_count != 0; }))
      return false;
    --m_count;
    return true;
  }

  template<typename T>
  bool wait_until(T &&t) noexcept {
    std::unique_lock lock(m_mutex);
    if (!m_cv.wait_until(lock, t, [this]() { return m_count != 0; }))
      return false;
    --m_count;
    return true;
  }

private:
  unsigned int m_count;
  std::mutex m_mutex;
  std::condition_variable m_cv;
};

class fast_semaphore {
public:
  explicit fast_semaphore(unsigned int count = 0) noexcept
    : m_count(count), m_semaphore(0) {}

  void post() noexcept {
    int count = m_count.fetch_add(1, std::memory_order_release);
    if (count < 0)
      m_semaphore.post();
  }

  void wait() noexcept {
    int count = m_count.fetch_sub(1, std::memory_order_acquire);
    if (count < 1)
      m_semaphore.wait();
  }

private:
  std::atomic_int m_count;
  semaphore m_semaphore;
};

template<typename T>
class atomic_blocking_queue
{
public:
  explicit atomic_blocking_queue(unsigned int size)
    : m_size(size), m_pushIndex(0), m_popIndex(0), m_count(0),
      m_data((T*)operator new(size * sizeof(T))),
      m_openSlots(size), m_fullSlots(0)
  {
    if(!size)
      throw std::invalid_argument("Invalid queue size!");
  }

  ~atomic_blocking_queue() noexcept
  {
    while (m_count--)
    {
      m_data[m_popIndex].~T();
      m_popIndex = ++m_popIndex % m_size;
    }
    operator delete(m_data);
  }

  template<typename Q = T>
  typename std::enable_if<std::is_nothrow_copy_constructible<Q>::value, void>::type
  push(const T& item) noexcept
  {
    m_openSlots.wait();

    auto pushIndex = m_pushIndex.fetch_add(1);
    new (m_data + (pushIndex % m_size)) T (item);
    ++m_count;

    auto expected = m_pushIndex.load();
    while(!m_pushIndex.compare_exchange_weak(expected, m_pushIndex % m_size))
      expected = m_pushIndex.load();

    m_fullSlots.post();
  }

  template<typename Q = T>
  typename std::enable_if<std::is_nothrow_move_constructible<Q>::value, void>::type
  push(T&& item) noexcept
  {
    m_openSlots.wait();

    auto pushIndex = m_pushIndex.fetch_add(1);
    new (m_data + (pushIndex % m_size)) T (std::move(item));
    ++m_count;

    auto expected = m_pushIndex.load();
    while(!m_pushIndex.compare_exchange_weak(expected, m_pushIndex % m_size))
      expected = m_pushIndex.load();

    m_fullSlots.post();
  }

  template<typename Q = T>
  typename std::enable_if<std::is_nothrow_copy_constructible<Q>::value, bool>::type
  try_push(const T& item) noexcept
  {
    auto result = m_openSlots.wait_for(std::chrono::seconds(0));
    if(!result)
      return false;

    auto pushIndex = m_pushIndex.fetch_add(1);
    new (m_data + (pushIndex % m_size)) T (item);
    ++m_count;

    auto expected = m_pushIndex.load();
    while(!m_pushIndex.compare_exchange_weak(expected, m_pushIndex % m_size))
      expected = m_pushIndex.load();

    m_fullSlots.post();
    return true;
  }

  template<typename Q = T>
  typename std::enable_if<std::is_nothrow_move_constructible<Q>::value, bool>::type
  try_push(T&& item) noexcept
  {
    auto result = m_openSlots.wait_for(std::chrono::seconds(0));
    if(!result)
      return false;

    auto pushIndex = m_pushIndex.fetch_add(1);
    new (m_data + (pushIndex % m_size)) T (std::move(item));
    ++m_count;

    auto expected = m_pushIndex.load();
    while(!m_pushIndex.compare_exchange_weak(expected, m_pushIndex % m_size))
      expected = m_pushIndex.load();

    m_fullSlots.post();
    return true;
  }

  template<typename Q = T>
  typename std::enable_if<
    !std::is_move_assignable<Q>::value &&
    std::is_nothrow_copy_assignable<Q>::value, void>::type
  pop(T& item) noexcept
  {
    m_fullSlots.wait();

    auto popIndex = m_popIndex.fetch_add(1);
    item = m_data[popIndex % m_size];
    m_data[popIndex % m_size].~T();
    --m_count;

    auto expected = m_popIndex.load();
    while(!m_popIndex.compare_exchange_weak(expected, m_popIndex % m_size))
      expected = m_popIndex.load();

    m_openSlots.post();
  }

  template<typename Q = T>
  typename std::enable_if<
    std::is_move_assignable<Q>::value &&
    std::is_nothrow_move_assignable<Q>::value, void>::type
  pop(T& item) noexcept
  {
    m_fullSlots.wait();

    auto popIndex = m_popIndex.fetch_add(1);
    item = std::move(m_data[popIndex % m_size]);
    m_data[popIndex % m_size].~T();
    --m_count;

    auto expected = m_popIndex.load();
    while(!m_popIndex.compare_exchange_weak(expected, m_popIndex % m_size))
      expected = m_popIndex.load();

    m_openSlots.post();
  }

  template<typename Q = T>
  typename std::enable_if<
    !std::is_move_assignable<Q>::value &&
    std::is_nothrow_copy_assignable<Q>::value, bool>::type
  try_pop(T& item) noexcept
  {
    auto result = m_fullSlots.wait_for(std::chrono::seconds(0));
    if(!result)
      return false;

    auto popIndex = m_popIndex.fetch_add(1);
    item = m_data[popIndex % m_size];
    m_data[popIndex % m_size].~T();
    --m_count;

    auto expected = m_popIndex.load();
    while(!m_popIndex.compare_exchange_weak(expected, m_popIndex % m_size))
      expected = m_popIndex.load();

    m_openSlots.post();
    return true;
  }

  template<typename Q = T>
  typename std::enable_if<
    std::is_move_assignable<Q>::value &&
    std::is_nothrow_move_assignable<Q>::value, bool>::type
  try_pop(T& item) noexcept
  {
    auto result = m_fullSlots.wait_for(std::chrono::seconds(0));
    if(!result)
      return false;

    auto popIndex = m_popIndex.fetch_add(1);
    item = std::move(m_data[popIndex % m_size]);
    m_data[popIndex % m_size].~T();
    --m_count;

    auto expected = m_popIndex.load();
    while(!m_popIndex.compare_exchange_weak(expected, m_popIndex % m_size))
      expected = m_popIndex.load();

    m_openSlots.post();
    return true;
  }

  bool empty() const noexcept
  {
    return m_count == 0;
  }

  bool full() const noexcept
  {
    return m_count == m_size;
  }

  unsigned int size() const noexcept
  {
    return m_count;
  }

  unsigned int capacity() const noexcept
  {
    return m_size;
  }

private:
  const unsigned int m_size;
  std::atomic_uint m_pushIndex;
  std::atomic_uint m_popIndex;
  std::atomic_uint m_count;
  T* m_data;

  semaphore m_openSlots;
  semaphore m_fullSlots;
};

class simple_thread_pool {
public:
  const int thread_count;

  explicit simple_thread_pool(unsigned int size, unsigned int threads = std::thread::hardware_concurrency())
    : thread_count(threads),
      m_queue(size)
  {
    if (!threads)
      throw std::invalid_argument("Invalid thread count!");

    m_tasks = 0;
    m_ran = 0;
    auto worker = [this]() {
      while (true) {
        Proc f;
        m_queue.pop(f);
        if (!f) {
          m_queue.push(std::move(f));
          break;
        }

        f();

        m_ran++;
      }
    };

    for (auto i = 0; i < threads; ++i)
      m_threads.emplace_back(worker);
  }

  ~simple_thread_pool() {
    m_queue.push({});
    for (auto &thread : m_threads)
      thread.join();
  }

  template<typename F, typename... Args>
  void enqueue_work(F &&f, Args &&... args) {
    m_tasks++;
    m_queue.push([p = std::forward<F>(f), t = std::make_tuple(std::forward<Args>(args)...)]() { std::apply(p, t); });
  }

  void wait() {
    // wait until last task is complete:
    while (m_ran < m_tasks) {
      //std::this_thread::yield();
    }
    m_tasks = 0;
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
  using Queue = atomic_blocking_queue<Proc>;
  Queue m_queue;

  using Threads = std::vector<std::thread>;
  Threads m_threads;

  std::atomic<int> m_ran;
  std::atomic<int> m_tasks;
};
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
    while (m_queue.empty() && !m_done)
      m_ready.wait(lock);
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
    while (m_queue.empty() && !m_done)
      m_ready.wait(lock);
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

class thread_pool
{
public:
  const int thread_count;

  explicit thread_pool(unsigned int threads = std::thread::hardware_concurrency())
    : m_queues(threads), m_count(threads), thread_count(threads)
  {
    if(!threads)
      throw std::invalid_argument("Invalid thread count!");

    m_tasks = 0;
    m_ran = 0;
    auto worker = [this](auto i)
    {
      while(true)
      {
        Proc f;
        for(auto n = 0; n < m_count * K; ++n)
          if(m_queues[(i + n) % m_count].try_pop(f))
            break;
        if(!f && !m_queues[i].pop(f))
          break;
        f();

        m_ran++;
      }
    };

    for(auto i = 0; i < threads; ++i)
      m_threads.emplace_back(worker, i);
  }

  ~thread_pool()
  {
    for(auto& queue : m_queues)
      queue.push({});
    for(auto& thread : m_threads)
      thread.join();
  }

  template<typename F, typename... Args>
  void enqueue_work(F&& f, Args&&... args)
  {
    auto work = [p = std::forward<F>(f), t = std::make_tuple(std::forward<Args>(args)...)]() { std::apply(p, t); };
    auto i = m_index++;

    for(auto n = 0; n < m_count * K; ++n)
      if(m_queues[(i + n) % m_count].try_push(work)) {
        m_tasks++;
        return;
      }

    m_queues[i % m_count].push(std::move(work));
    m_tasks++;
  }

#if 0
  template<typename F, typename... Args>
  [[nodiscard]] auto enqueue_task(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>
  {
    using task_return_type = std::invoke_result_t<F, Args...>;
    using task_type = std::packaged_task<task_return_type()>;

    auto task = std::make_shared<task_type>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    auto work = [task]() { (*task)(); };
    auto result = task->get_future();
    auto i = m_index++;

    for(auto n = 0; n < m_count * K; ++n)
      if(m_queues[(i + n) % m_count].try_push(work))
        return result;

    m_queues[i % m_count].push(std::move(work));

    return result;
  }
#endif

  void wait() {
    // wait until last task is complete:
    while (m_ran < m_tasks) {
      //std::this_thread::yield();
    }
    m_tasks = 0;
    m_ran = 0;
  }

private:
  using Proc = std::function<void(void)>;
  using Queue = blocking_queue<Proc>;
  using Queues = std::vector<Queue>;
  Queues m_queues;

  using Threads = std::vector<std::thread>;
  Threads m_threads;

  const unsigned int m_count;
  std::atomic_uint m_index = 0;

  std::atomic<int> m_ran;
  std::atomic<int> m_tasks;

  inline static const unsigned int K = 2;
};
