#pragma once

#include <list>
#include <memory>
#include <boost/thread/mutex.hpp>

namespace Bbx
{
namespace Impl
{

    /**
    * @brief Simple thread-safe queue.
    */
    template <typename T> 
    class BlockingPtrQueue
    {
    public:  
        /* Default constructor */
        BlockingPtrQueue();

        /* Destructor */
        ~BlockingPtrQueue();

        /* Checks if the queue is empty */
        bool empty() const;

        /* Clear the queue */
        void clear();

        /* Return the first member and pop it off the queue */
        std::shared_ptr<T> pop();
        
        /* Push the data into the end of queue if there is space
        and returns true, else returns false */
        bool emplace(std::shared_ptr<T> data);

    private:
        /* Using list here to allowing quick insertion or erasing */
        std::list<std::shared_ptr<T>> m_queue;

        /* Protection shared data mutex */
        mutable boost::mutex m_mutex;
    };

    template <typename T>
    inline BlockingPtrQueue<T>::BlockingPtrQueue()
        : m_queue()
    {  
    }

    template <typename T>
    inline BlockingPtrQueue<T>::~BlockingPtrQueue()
    {
        m_queue.clear();
    }

    template <typename T>
    inline bool BlockingPtrQueue<T>::empty() const
    {
        boost::mutex::scoped_lock lock(m_mutex);    
        return m_queue.empty();
    }

    template <typename T>
    inline void BlockingPtrQueue<T>::clear()
    {
        boost::mutex::scoped_lock lock(m_mutex);
        while (!m_queue.empty())
            m_queue.pop_front();    
    }

    template <typename T>
    inline std::shared_ptr<T> BlockingPtrQueue<T>::pop()
    {
        boost::mutex::scoped_lock lock(m_mutex);
        if (m_queue.empty())
            return std::shared_ptr<T>();

        std::shared_ptr<T> node = m_queue.front();
        m_queue.pop_front();    
        return node;
    }

    template <typename T>
    inline bool BlockingPtrQueue<T>::emplace(std::shared_ptr<T> data)
    {    
        boost::mutex::scoped_lock lock(m_mutex);

        try
        {
            m_queue.push_back(data);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }
}
}
