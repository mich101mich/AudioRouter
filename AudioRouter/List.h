#pragma once

#include "Prelude.h"

template<typename T>
struct ListNode
{
    T m_data;
    ListNode<T>* m_next;

    ListNode(ListNode<T>* next = nullptr)
        : m_next(next)
    {}
};

template<typename T>
struct ListIter
{
    ListNode<T>* m_node;

    ListIter& operator++()
    {
        m_node = m_node->m_next;
        return *this;
    }
    bool operator!=(const ListIter& other) const
    {
        return m_node != other.m_node;
    }
    T& operator*()
    {
        return m_node->m_data;
    }

    ListIter(ListNode<T>* node)
        : m_node(node)
    {}
};

template<typename T>
class List
{
public:
    List() : m_head(nullptr) {}
    ~List()
    {
        clear();
    }

    NTSTATUS add(ListNode<T>*& node)
    {
        NTSTATUS status = allocate(node);
        if (NT_SUCCESS(status))
        {
            new (node) ListNode<T>(m_head);
            m_head = node;
        }
        return status;
    }
    void remove(ListNode<T>* node)
    {
        if (node == m_head)
        {
            m_head = m_head->m_next;
        }
        else
        {
            ListNode<T>* prev = m_head;
            while (prev->m_next != node)
            {
                prev = prev->m_next;
            }
            prev->m_next = node->m_next;
        }
        deallocate(node);
    }
    void clear()
    {
        ListNode<T>* node = m_head;
        while (node)
        {
            ListNode<T>* next = node->m_next;
            deallocate(node);
            node = next;
        }
        m_head = nullptr;
    }

    ListIter<T> begin()
    {
        return ListIter<T>(m_head);
    }
    ListIter<T> end()
    {
        return ListIter<T>(nullptr);
    }

private:
    ListNode<T>* m_head;
};

