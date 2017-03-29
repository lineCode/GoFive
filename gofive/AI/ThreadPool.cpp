#include "ThreadPool.h"

void ThreadPool::start()
{
    running_ = true;
    threads_.reserve(num_thread);
    for (int i = 0; i < num_thread; i++) {
        threads_.push_back(std::thread(std::bind(&ThreadPool::threadFunc, this)));
    }
}

void ThreadPool::stop()
{
    {
        unique_lock<std::mutex> ul(mutex_condition);
        running_ = false;
        notEmpty_task.notify_all();
    }

    for (auto &iter : threads_) {
        iter.join();
    }
}

void ThreadPool::run(Task t, bool origin)
{
    if (threads_.empty()) {//���߳�
        work(t);
    }
    else {
        if (origin)
        {
            mutex_origin_queue.lock();
            queue_origin_task.push_back(t);
            if (num_working.load() == 0 && queue_origin_task.size() == 1)
            {
                notEmpty_task.notify_one();
            }
            mutex_origin_queue.unlock();

        }
        else
        {
            mutex_queue.lock();
            queue_task.push_back(t);
            if (queue_task.size() > GameTreeNode::maxTaskNum)
            {
                GameTreeNode::maxTaskNum = queue_task.size();
            }
            mutex_queue.unlock();
            notEmpty_task.notify_one();
        }

    }
}

void ThreadPool::threadFunc()
{
    while (running_) {
        Task task = take();
        if (task.node) {
            work(task);
            num_working--;
        }
    }
}

void ThreadPool::work(Task t)
{

    t.node->buildChild(false);//���ݹ�
    size_t len = t.node->childs.size();
    if (len > 0)
    {
        Task task = t;
        //string nodehash;
        for (size_t i = 0; i < len; ++i)
        {
            task.node = t.node->childs[i];
            run(task, false);
            /*nodehash = task.node->chessBoard->toString();
            mutex_map.lock();
            auto s = GameTreeNode::historymap->find(nodehash);
            if (s != GameTreeNode::historymap->end())
            {
                t.node->childs[i] = s->second;
                mutex_map.unlock();
            }
            else
            {
                GameTreeNode::historymap->insert(map<string, GameTreeNode*>::value_type(nodehash, t.node->childs[i]));
                mutex_map.unlock();
                run(task, false);
            }*/      
        }
    }
}

Task ThreadPool::take()
{
    unique_lock<std::mutex> ul(mutex_condition);
    while (queue_origin_task.empty() && queue_task.empty() && running_) {
        notEmpty_task.wait(ul);
    }
    Task task = { NULL };
    //���Ƚ��queue_task�����
    mutex_queue.lock();
    if (!queue_task.empty()) {
        /*task = queue_task.front();
        queue_task.pop_front();*/
        task = queue_task.back();
        queue_task.pop_back();
        num_working++;
        mutex_queue.unlock();
    }
    else//��queue_taskΪ��
    {
        mutex_queue.unlock();
        mutex_origin_queue.lock();
        if (num_working.load() == 0 && !queue_origin_task.empty())//queue_taskΪ�ղ���û���߳��ڹ���
        {
            task = queue_origin_task.front();
            queue_origin_task.pop_front();
            num_working++;
        }
        mutex_origin_queue.unlock();
    }



    return task;
}