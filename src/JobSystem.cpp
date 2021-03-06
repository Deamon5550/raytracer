
#include "JobSystem.h"

#include <thread>
#include <mutex>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#define WINDOWS
#include <windows.h>
#include <direct.h>
#include <io.h>
#else
#define LINUX
#include <unistd.h>
#endif

// This is a simple job system to parallelize certain tasks
namespace scheduler {

    // a cross-platform sleep function with millisecond accuracy
    void sleep(int milli) {
#ifdef WINDOWS
        Sleep(milli);
#else
        usleep(milli * 1000);
#endif
    }

    std::thread **threads;
    int thread_count;
    bool running;
    int finished = 0;
    std::mutex job_mutex;
    Job *jobs_head;
    Job *jobs_tail;

    void worker() {
        while (true) {
            Job *job = nullptr;
            {
                std::lock_guard<std::mutex> guard(job_mutex);
                job = jobs_head;
                Job *end = nullptr;
                // Each worker pulls up to 10 tasks from the queue to lower the amount
                // of time the mutex needs to be held for
                for (int i = 0; i < 10; i++) {
                    if (jobs_head != nullptr) {
                        end = jobs_head;
                        jobs_head = jobs_head->next_job;
                        if (jobs_head == nullptr) {
                            jobs_tail = nullptr;
                            break;
                        }
                    } else {
                        break;
                    }
                }
                if (end != nullptr) {
                    end->next_job = nullptr;
                }
                if (job == nullptr && !running) {
                    // shutdown
                    finished++;
                    break;
                }
            }
            // execute the jobs
            while (job != nullptr) {
                job->job(job->job_data);
                job = job->next_job;
            }
        }
    }

    // starts `count` workers
    void startWorkers(int count) {
        printf("Starting jobs system with %d workers\n", count);
        thread_count = count;
        jobs_head = nullptr;
        jobs_tail = nullptr;
        running = true;
        threads = new std::thread*[count];
        for (int i = 0; i < count; i++) {
            threads[i] = new std::thread(worker);
        }
    }

    // inserts a job to the end of the job queue
    void submit(task job, void *job_data) {
        {
            std::lock_guard<std::mutex> guard(job_mutex);

            Job *newjob = new Job(job_data, job);

            if (jobs_head == nullptr) {
                jobs_head = newjob;
                jobs_tail = newjob;
            } else {
                jobs_tail->next_job = newjob;
                jobs_tail = newjob;
            }

        }

    }

    // waits until all jobs have been completed
    // also shuts down the scheduler allowing the worker threads to exit
    void waitForCompletion() {
        running = false;
        while (true) {
            if (jobs_head == nullptr && finished == thread_count) {
                break;
            } else {
                sleep(1);
            }
        }
    }

    Job::Job(void *data, task t) {
        job_data = data;
        job = t;
        next_job = nullptr;
    }

}