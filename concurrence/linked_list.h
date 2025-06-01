//
// Created by cxk_zjq on 25-6-1.
//

#ifndef GOCOROUTINE_LINKED_LIST_H
#define GOCOROUTINE_LINKED_LIST_H
#pragma once
#include <atomic>

/*
 * @brief 侵入式双向链表实现
 * 使用时需要继承LinkedNode，并将其作为数据结构的基类
 */

namespace cxk
{

struct LinkedList;

struct LinkedNode
{
    LinkedNode* prev = nullptr;
    LinkedNode* next = nullptr;

    inline bool is_linked() {
        return prev || next;
    }
};

struct LinkedList
{
public:
    LinkedList() {
        clear();
    }

    LinkedList(LinkedList const&) = delete;
    LinkedList& operator=(LinkedList const&) = delete;

    void clear()
    {
        head_ = tail_ = nullptr;
    }

    void push(LinkedNode* node)
    {
        if (!tail_) {
            head_ = tail_ = node;
            node->prev = node->next = nullptr;
            return ;
        }

        tail_->next = node;
        node->prev = tail_;
        node->next = nullptr;
        tail_ = node;
    }

    LinkedNode* front()
    {
        return head_;
    }

    bool unlink(LinkedNode* node)
    {
        if (head_ == tail_ && head_ == node) {
            node->prev = node->next = nullptr;
            clear();
            return true;
        }

        if (tail_ == node) {
            tail_ = tail_->prev;
            tail_->next = nullptr;
            node->prev = node->next = nullptr;
            return true;
        }

        if (head_ == node) {
            head_->prev = nullptr;
            head_ = head_->next;
            node->prev = node->next = nullptr;
            return true;
        }

        bool unlinked = false;
        if (node->prev) {
            node->prev->next = node->next;
            unlinked = true;
        }

        if (node->next) {
            node->next->prev = node->prev;
            unlinked = true;
        }
        node->next = node->prev = nullptr;
        return unlinked;
    }

private:
    LinkedNode* head_;
    LinkedNode* tail_;
};

} // namespace cxk


#endif //GOCOROUTINE_LINKED_LIST_H
