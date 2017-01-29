#pragma once

namespace scheduler {

    typedef void(*task)(void*);
    class Job {
    public:
        Job(void *job_data, task job);

        void *job_data;
        task job;

        Job *next_job;

    };

    void startWorkers(int count);
    void submit(task job, void *job_data);
    void waitForCompletion();

}