#pragma once

namespace co {

struct clink {
    clink* next;
    clink* prev;
};

class clist {
  public:
    constexpr static clink* const null = (clink*)0;

    constexpr clist() noexcept : _head(0) {}
    ~clist() = default;

    clink* front() const noexcept { return _head; }
    clink* back() const noexcept { return _head ? _head->prev : 0; }
    bool empty() const noexcept { return _head == null; }
    void clear() noexcept { _head = 0; }

    // push a new node to the front
    void push_front(clink* node) {
        if (_head) {
            node->next = _head;
            node->prev = _head->prev;
            _head->prev = node;
            _head = node;
        } else {
            node->next = 0;
            node->prev = node;
            _head = node;
        }
    }

    // push a new node to the back
    void push_back(clink* node) {
        if (_head) {
            node->next = 0;
            node->prev = _head->prev;
            _head->prev->next = node;
            _head->prev = node;
        } else {
            node->next = 0;
            node->prev = node;
            _head = node;
        }
    }

    // erase a node from the list
    void erase(clink* node) {
        if (node != _head) {
            node->prev->next = node->next;
            const auto x = node->next ? node->next : _head;
            x->prev = node->prev;
        } else {
            _head = _head->next;
            if (_head) _head->prev = node->prev;
        }
    }

    // move a node to the front
    void move_front(clink* node) {
        if (node != _head) {
            node->prev->next = node->next;
            if (node->next) {
                node->next->prev = node->prev;
                node->prev = _head->prev;
                _head->prev = node;
            }
            node->next = _head;
            _head = node;
        }
    }

    // move a node to the back
    void move_back(clink* node) {
        if (node != _head->prev) {
            if (node == _head) {
                _head = _head->next;
                node->prev->next = node;
                node->next = 0;
            } else {
                node->prev->next = node->next;
                node->next->prev = node->prev;
                node->prev = _head->prev;
                node->next = 0;
                _head->prev->next = node;
                _head->prev = node;
            }
        }
    }

  private:
    clink* _head;
};

} // co
