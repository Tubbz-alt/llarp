#include "threadpool.hpp"
#ifndef _MSC_VER
#include <pthread.h>
#endif
#include <cstring>

#include <llarp/time.h>
#include <queue>

#include "logger.hpp"

#if(__FreeBSD__) || (__OpenBSD__) || (__NetBSD__)
#include <pthread_np.h>
#endif

#ifdef _MSC_VER
#include <windows.h>
extern "C" void
SetThreadName(DWORD dwThreadID, LPCSTR szThreadName);
#endif

namespace llarp
{
  namespace thread
  {
    Pool::Pool(size_t workers, const char *name)
    {
      stop = false;
      while(workers--)
      {
        threads.emplace_back([this, name] {
          if(name)
          {
#if(__APPLE__ && __MACH__)
            pthread_setname_np(name);
#elif(__FreeBSD__) || (__OpenBSD__) || (__NetBSD__)
            pthread_set_name_np(pthread_self(), name);
#elif !defined(_MSC_VER) || !defined(_WIN32)
            pthread_setname_np(pthread_self(), name);
#else
            SetThreadName(GetCurrentThreadId(), name);
#endif
          }
          for(;;)
          {
            llarp_thread_job *job;
            {
              lock_t lock(this->queue_mutex);
              this->condition.wait(
                  lock, [this] { return this->stop || !this->jobs.empty(); });
              if(this->stop && this->jobs.empty())
                return;
              job = this->jobs.top().job;
              this->jobs.pop();
            }
            // do work
            job->work(job->user);

            delete job;
          }
        });
      }
    }

    void
    Pool::Stop()
    {
      {
        lock_t lock(queue_mutex);
        stop = true;
      }
      condition.notify_all();
    }

    void
    Pool::Join()
    {
      for(auto &t : threads)
        t.join();
      threads.clear();
      done.notify_all();
    }

    void
    Pool::QueueJob(const llarp_thread_job &job)
    {
      {
        lock_t lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if(stop)
          return;

        jobs.emplace(ids++, new llarp_thread_job(job.user, job.work));
      }
      condition.notify_one();
    }

  }  // namespace thread
}  // namespace llarp

struct llarp_threadpool
{
  llarp::thread::Pool *impl;

  std::mutex m_access;
  std::queue< llarp_thread_job * > jobs;

  llarp_threadpool(int workers, const char *name)
      : impl(new llarp::thread::Pool(workers, name))
  {
  }

  llarp_threadpool() : impl(nullptr)
  {
  }
};

struct llarp_threadpool *
llarp_init_threadpool(int workers, const char *name)
{
  if(workers > 0)
    return new llarp_threadpool(workers, name);
  else
    return nullptr;
}

struct llarp_threadpool *
llarp_init_same_process_threadpool()
{
  return new llarp_threadpool();
}

void
llarp_threadpool_join(struct llarp_threadpool *pool)
{
  llarp::LogDebug("threadpool join");
  if(pool->impl)
    pool->impl->Join();
}

void
llarp_threadpool_start(struct llarp_threadpool *pool)
{ /** no op */
}

void
llarp_threadpool_stop(struct llarp_threadpool *pool)
{
  llarp::LogDebug("threadpool stop");
  if(pool->impl)
    pool->impl->Stop();
}

void
llarp_threadpool_wait(struct llarp_threadpool *pool)
{
  std::mutex mtx;
  llarp::LogDebug("threadpool wait");
  if(pool->impl)
  {
    std::unique_lock< std::mutex > lock(mtx);
    pool->impl->done.wait(lock);
  }
}

void
llarp_threadpool_queue_job(struct llarp_threadpool *pool,
                           struct llarp_thread_job job)
{
  if(pool->impl)
    pool->impl->QueueJob(job);
  else if(job.user && job.work)
  {
    auto j  = new llarp_thread_job;
    j->work = job.work;
    j->user = job.user;
    {
      std::unique_lock< std::mutex > lock(pool->m_access);
      pool->jobs.push(j);
    }
  }
}

void
llarp_threadpool_tick(struct llarp_threadpool *pool)
{
  while(pool->jobs.size())
  {
    llarp_thread_job *job;
    {
      std::unique_lock< std::mutex > lock(pool->m_access);
      job = pool->jobs.front();
      pool->jobs.pop();
    }
    job->work(job->user);
    delete job;
  }
}

void
llarp_free_threadpool(struct llarp_threadpool **pool)
{
  if(*pool)
  {
    delete *pool;
  }
  *pool = nullptr;
}
