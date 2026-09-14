#pragma once

namespace co {

struct clink {
    clink* next;
    clink* prev;
};

struct clist {
    constexpr static clink* const null = (clink*)0;

    constexpr clist() noexcept : _head(0) {}
    ~clist() = default;

    clink* front() const noexcept { return _head; }
    clink* back() const noexcept { return _head ? _head->prev : null; }
    bool empty() const noexcept { return _head == null; }
    void clear() noexcept { _head = null; }

    void push_front(clink* node) noexcept {
        if (_head) {
            node->next = _head;
            node->prev = _head->prev;
            _head->prev = node;
            _head = node;
        } else {
            node->next = null;
            node->prev = node;
            _head = node;
        }
    }

    // merge another list to the front, l will be cleared after merged
    void push_front(clist& l) {
        if (!l.empty()) {
            if (_head) {
                const auto tail = _head->prev;
                _head->prev = l._head->prev;
                l._head->prev->next = _head;
                l._head->prev = tail;
            }
            _head = l._head;
            l._head = null;
        }
    }

    void push_back(clink* node) noexcept {
        if (_head) {
            node->next = null;
            node->prev = _head->prev;
            _head->prev->next = node;
            _head->prev = node;
        } else {
            node->next = null;
            node->prev = node;
            _head = node;
        }
    }

    // merge another list to the back, l will be cleared after merged
    void push_back(clist& l) {
        if (!l.empty()) {
            if (_head) {
                const auto tail = l._head->prev;
                l._head->prev = _head->prev;
                _head->prev->next = l._head;
                _head->prev = tail;
            } else {
                _head = l._head;
            }
            l._head = null;
        }
    }

    clink* pop_front() noexcept {
        clink* const x = _head;
        if (_head) {
            _head = _head->next;
            if (_head) _head->prev = x->prev;
        }
        return x;
    }

    clink* pop_back() noexcept {
        clink* const node = this->back();
        if (node) {
            if (node != _head) {
                node->prev->next = null;
                _head->prev = node->prev;
            } else {
                _head = null;
            }
        }
        return node;
    }

    void erase(clink* node) noexcept {
        if (node != _head) {
            node->prev->next = node->next;
            const auto x = node->next ? node->next : _head;
            x->prev = node->prev;
        } else {
            _head = _head->next;
            if (_head) _head->prev = node->prev;
        }
    }

    // move a node already in the list to the front
    void move_front(clink* node) noexcept {
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

    // move a node already in the list to the back
    void move_back(clink* node) noexcept {
        if (node != _head->prev) {
            if (node == _head) {
                _head = _head->next;
                node->prev->next = node;
                node->next = null;
            } else {
                node->prev->next = node->next;
                node->next->prev = node->prev;
                node->prev = _head->prev;
                node->next = null;
                _head->prev->next = node;
                _head->prev = node;
            }
        }
    }

    void swap(clist& l) noexcept {
        clink* const x = _head;
        _head = l._head;
        l._head = x;
    }

    void swap(clist&& l) noexcept { l.swap(*this); }

    // run f on each element
    template<typename F>
    void for_each(F&& f) const {
        for (clink* c = _head; c;) {
            const auto x = c;
            c = c->next;
            f(x);
        }
    }

    clink* _head;
};

} // co
