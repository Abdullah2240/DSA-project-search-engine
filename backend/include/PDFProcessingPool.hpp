#pragma once
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>
#include <future>
#include "BatchIndexWriter.hpp"
#include "lexicon.hpp"

struct ProcessedPDF {
    int doc_id;
    std::string title;
    std::vector<std::string> tokens;
    bool success = false;
    std::string error;
};

class PDFProcessingPool {
public:
    explicit PDFProcessingPool(
        size_t num_threads,
        BatchIndexWriter& batch_writer,
        Lexicon& lexicon
    );
    
    ~PDFProcessingPool();
    
    // Submit PDF for async processing
    std::future<int> submit_pdf(
        const std::string& pdf_path,
        int doc_id
    );
    
    // Get pool statistics
    struct Stats {
        size_t active_workers = 0;
        size_t queue_size = 0;
        size_t completed_tasks = 0;
        size_t failed_tasks = 0;
    };
    Stats get_stats() const;
    
private:
    struct Task {
        std::string pdf_path;
        int doc_id;
        std::promise<int> result;
    };
    
    void worker_thread();
    void process_pdf(Task& task);
    ProcessedPDF call_python_tokenizer(const std::string& pdf_path, int doc_id);
    std::map<int, WordStats> build_doc_stats(
        const std::vector<std::string>& tokens
    );
    
    BatchIndexWriter& batch_writer_;
    Lexicon& lexicon_;
    
    std::vector<std::thread> workers_;
    std::queue<Task> task_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::atomic<bool> shutdown_{false};
    
    mutable std::mutex stats_mutex_;
    Stats stats_{};
};
