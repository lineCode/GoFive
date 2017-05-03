#include "ThreadPool.h"

int ThreadPool::num_thread = 2;
void ThreadPool::start()
{
    running = true;
    threads.reserve(num_thread);
    for (int i = 0; i < num_thread; i++) {
        threads.push_back(std::thread(std::bind(&ThreadPool::threadFunc, this)));
    }
}

void ThreadPool::stop()
{
    {
        unique_lock<std::mutex> ul(mutex_condition);
        running = false;
        notEmpty_task.notify_all();
    }

    for (auto &iter : threads) {
        iter.join();
    }
}

void ThreadPool::run(Task t, bool origin)
{
    if (!threads.empty()) {
        if (origin)
        {
            mutex_origin_queue.lock();
            task_queue.push_back(t);
            if (num_working.load() == 0 && task_queue.size() == 1)
            {
                notEmpty_task.notify_one();
            }
            mutex_origin_queue.unlock();

        }
        else
        {
            mutex_queue.lock();
            task_priority_queue.push_back(t);
            if (task_priority_queue.size() > GameTreeNode::maxTaskNum)
            {
                GameTreeNode::maxTaskNum = task_priority_queue.size();
            }
            mutex_queue.unlock();
            notEmpty_task.notify_one();
        }
    }
}

void ThreadPool::wait()
{
    int count = 0;
    while (true)
    {
        if (task_priority_queue.size() + task_queue.size() == 0 && num_working.load() == 0)
        {
            break;
        }
        if (num_working.load() < num_thread / 2)
        {
            count++;
        }
        if (count > 10 && GameTreeNode::longtailmode == false)//1s
        {
            count = 0;
            GameTreeNode::longtailmode = true;
            GameTreeNode::longtail_threadcount = 0;
        }
        this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void ThreadPool::threadFunc()
{
    while (running) {
        Task task = take();
        if (task.node) {
            work(task);
            num_working--;
        }
    }
}

void ThreadPool::work(Task t)
{
    if (t.type == TASKTYPE_DEFEND)
    {
        t.node->alpha = GameTreeNode::bestRating;
        t.node->beta = INT32_MAX;
        t.node->buildDefendTreeNode(GameTreeNode::childsInfo[t.index].lastStepScore);
        RatingInfoDenfend info = t.node->getBestDefendRating(GameTreeNode::childsInfo[t.index].lastStepScore);
        GameTreeNode::childsInfo[t.index].rating = info.rating;
        GameTreeNode::childsInfo[t.index].depth = info.lastStep.step - GameTreeNode::startStep;
        if (t.index == 45)
        {
            t.index = 45;
        }
        t.node->deleteChilds();
        if (-GameTreeNode::childsInfo[t.index].rating.totalScore > GameTreeNode::bestRating)
        {
            GameTreeNode::bestRating = -GameTreeNode::childsInfo[t.index].rating.totalScore;
        }
    }
    else if (t.type == TASKTYPE_ATACK)
    {
        t.node->alpha = GameTreeNode::bestRating;
        t.node->beta = 0;
        //if (t.index == 24)
        //{
        //    t.index = 24;
        //}
        t.node->buildAtackTreeNode();
        RatingInfoAtack info = t.node->getBestAtackRating();
        GameTreeNode::childsInfo[t.index].rating = (t.node->playerColor == STATE_CHESS_BLACK) ? info.white : info.black;
        GameTreeNode::childsInfo[t.index].depth = info.depth;
        if (GameTreeNode::childsInfo[t.index].rating.highestScore >= SCORE_5_CONTINUE && info.depth > -1)
        {
            if (info.depth < GameTreeNode::bestRating)
            {
                GameTreeNode::bestRating = info.depth;
                GameTreeNode::bestIndex = t.index;
            }
        }
        //if (t.index == 24)
        //{
        //    t.index = 24;
        //}
        t.node->deleteChilds();
        delete t.node;
    }

}

Task ThreadPool::take()
{
    unique_lock<std::mutex> ul(mutex_condition);
    while (task_queue.empty() && task_priority_queue.empty() && running) {
        notEmpty_task.wait(ul);
    }
    Task task = { NULL };
    //优先解决task_priority_queue里面的
    mutex_queue.lock();
    if (!task_priority_queue.empty()) {
        /*task = queue_task.front();
        queue_task.pop_front();*/
        task = task_priority_queue.back();
        task_priority_queue.pop_back();
        num_working++;
        mutex_queue.unlock();
    }
    else//若queue_task为空
    {
        mutex_queue.unlock();
        mutex_origin_queue.lock();
        if (num_working.load() == 0 && !task_queue.empty())//task_priority_queue为空并且没有线程在工作
        {
            task = task_queue.front();
            task_queue.pop_front();
            num_working++;
        }
        mutex_origin_queue.unlock();
    }



    return task;
}
