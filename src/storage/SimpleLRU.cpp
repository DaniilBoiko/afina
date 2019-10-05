#include "SimpleLRU.h"

namespace Afina {
    namespace Backend {

// See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Put(const std::string &key, const std::string &value) {
            if (!Is_present(key)) {
                return SimpleLRU::PutIfAbsent(key, value);
            } else {
                return SimpleLRU::Set(key, value);
            }
        }

// See MapBasedGlobalLockImpl.h
        bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
            if (Is_present(key)) {
                Send_to_back(key);
                return false;
            }

            size_t added = key.size() + value.size();
            if (added > _max_size) {
                return false;
            }

            Free_memory(added);
            Put_to_back(key, value, added);
            return true;
        }


// See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Set(const std::string &key, const std::string &value) {
            if (Is_present(key)) {
                size_t added;

                if (_lru_index.find(const_cast<std::string &>(key))->second.get().value.size() -
                    value.size() < 0) {
                    added = 0;
                }
                else {
                    added = _lru_index.find(const_cast<std::string &>(key))->second.get().value.size() -
                                   value.size();
                }

                if (key.size() + value.size() > _max_size) {
                    return false;
                }

                Free_memory(added);

                // Change value
                _lru_index.find(const_cast<std::string &>(key))->second.get().value = value;

                // Move to the end of the list
                Send_to_back(key);

                return true;
            }

            return false;
        }

// See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Delete(const std::string &key) {
            if (Is_present(key)) {
                _size_now -= key.size() + _lru_index.find(const_cast<std::string &>(key))->second.get().value.size();

                lru_node &to_be_deleted = _lru_index.find(const_cast<std::string &>(key))->second.get();


                if (_lru_head -> key == key) {
                    if (_lru_tail -> key == key) {
                        _lru_tail = 0;
                        _lru_head = 0;
                    }
                     else {
                        _lru_head = std::move(to_be_deleted.next);
                        _lru_head -> prev = 0;
                    }
                     return true;
                }

                if (_lru_tail -> key == key) {
                    _lru_tail = to_be_deleted.prev;
                    _lru_tail -> next = 0;

                    return true;
                }

                to_be_deleted.prev->next = std::move(to_be_deleted.next);
                to_be_deleted.next->prev = to_be_deleted.prev;

                _lru_index.erase(const_cast<std::string &>(key));

                return true;
            }

            return false;
        }

// See MapBasedGlobalLockImpl.h
        bool SimpleLRU::Get(const std::string &key, std::string &value) {
            if (Is_present(key)) {
                Send_to_back(key);

                value = _lru_tail->value;
                return true;
            }

            return false;
        }

        void SimpleLRU::Send_to_back(const std::string &key) {
            lru_node &to_send = _lru_index.find(const_cast<std::string &>(key))->second.get();

            if (_lru_tail -> key != key) {
                if (_lru_head -> key == key) {
                    _lru_tail -> next = std::move(_lru_head);

                    _lru_head = std::move(_lru_tail -> next -> next);
                    _lru_head -> prev = 0;

                    _lru_tail = &to_send;
                }
                else {
                    _lru_tail -> next = std::move(to_send.prev -> next);
                    to_send.prev -> next = std::move(to_send.next);

                    _lru_tail = to_send.next -> prev;
                    to_send.next -> prev = to_send.prev;
                }
            }
        }

        void SimpleLRU::Put_to_back(const std::string &key, const std::string &value, size_t added) {
            auto *new_lru_node = new lru_node({key, value, _lru_tail});
            _lru_index.insert(std::make_pair(std::ref(new_lru_node->key), std::ref(*new_lru_node)));

            if (_lru_head == 0) {
                _lru_head = std::unique_ptr<lru_node>(new_lru_node);
                _lru_tail = new_lru_node;

                _size_now += added;
            } else {
                _lru_tail->next = std::unique_ptr<lru_node>(new_lru_node);
                _lru_tail = new_lru_node;

                _size_now += added;
            }
        }

        void SimpleLRU::Free_memory(size_t added) {
            while (_size_now + added > _max_size) {
                Delete(_lru_head->key);
            }
        }

        bool SimpleLRU::Is_present(const std::string &key) {
            return _lru_index.find(const_cast<std::string &>(key)) != _lru_index.end();
        }

    } // namespace Backend
} // namespace Afina
