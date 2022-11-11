#ifndef ESP32_SRC_MUTEX_H_
#define ESP32_SRC_MUTEX_H_

#include <cassert>
#include <mutex>

namespace safe_std {

template<class T>
class mutex;
template<class T>
class mutex_guard {
  mutex<T> *_parent;

 public:
  mutex_guard(mutex<T> *parent) noexcept;
  ~mutex_guard() noexcept;
  T &operator*() noexcept;
  T* operator->() noexcept;
};

template<class T>
mutex_guard<T>::mutex_guard(mutex<T> *parent) noexcept {
  _parent = parent;
}
template<class T>
mutex_guard<T>::~mutex_guard() noexcept {
  _parent->doneWithMutex();
  _parent = nullptr;
}
template<class T>
T &mutex_guard<T>::operator*() noexcept {
  assert(_parent != nullptr);
  return _parent->val;
}
template<class T>
T* mutex_guard<T>::operator->() noexcept {
  assert(_parent != nullptr);
  return &_parent->val;
}

template<class T>
class mutex {
  T val;
  bool isBorrowed = false;
  std::mutex mtx;
 public:
  mutex() noexcept;

  mutex(T t) noexcept;

  mutex_guard<T> lock() noexcept;
  T lockAndSwap(T&& newVal) noexcept;
  void doneWithMutex() noexcept;
  ~mutex() noexcept;
  friend class mutex_guard<T>;
};
template<class T>
mutex<T>::mutex(T t) noexcept {
  val = t;
}

template<class T>
mutex_guard<T> mutex<T>::lock() noexcept {
  mtx.lock();
  assert(!isBorrowed);
  isBorrowed = true;
  mutex_guard<T> guard(this);
  return guard;
}
template<class T>
mutex<T>::~mutex() noexcept {
  assert(!isBorrowed);
}
template<class T>
void mutex<T>::doneWithMutex() noexcept {
  assert(isBorrowed);
  isBorrowed = false;
  mtx.unlock();
}
template<class T>
T mutex<T>::lockAndSwap(T&& newVal) noexcept {
  mtx.lock();
  assert(!isBorrowed);
  isBorrowed = true;
  T oldVal = val;
  val = std::move(newVal);
  isBorrowed = false;
  mtx.unlock();
  return oldVal;
}

template<class T>
mutex<T>::mutex() noexcept = default;

} // safe_std

#endif //ESP32_SRC_MUTEX_H_
