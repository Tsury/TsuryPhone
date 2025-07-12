#pragma once

#include <Arduino.h>

template <typename T, size_t N> class RingBuffer {
public:
  RingBuffer() : _buffer(), _head(0), _tail(0), _full(false) {}

  bool empty() const {
    return (!_full && (_head == _tail));
  }

  bool full() const {
    return _full;
  }

  size_t capacity() const {
    return N;
  }

  size_t size() const {
    if (_full) {
      return N;
    }
    if (_head >= _tail) {
      return _head - _tail;
    }
    return N + _head - _tail;
  }

  bool push(const T &item) {
    if (_full) {
      return false;
    }

    _buffer[_head] = item;
    _head = (_head + 1) % N;
    _full = (_head == _tail);
    return true;
  }

  T pop() {
    if (empty()) {
      return T();
    }

    T item = _buffer[_tail];
    _tail = (_tail + 1) % N;
    _full = false;
    return item;
  }

  T front() const {
    if (empty()) {
      return T();
    }

    return _buffer[_tail];
  }

  void clear() {
    _head = _tail;
    _full = false;
  }

private:
  T _buffer[N];
  size_t _head;
  size_t _tail;
  bool _full;
};
