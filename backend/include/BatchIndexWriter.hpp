#pragma once
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <chrono>
#include "forward_index.hpp"
#include "inverted_index.hpp"
#include "lexicon.hpp"
#include "DocumentMetadata.hpp"
#include "doc_url_mapper.hpp"
#include "json.hpp"

using json = nlohmann::json;

struct PendingDocument {
    int doc_id;
    std::string title;
    std::vector<std::string> tokens;
    std::map<int, WordStats> doc_stats;
    std::string url;
    std::string pdf_path;
    std::chrono::steady_clock::time_point enqueue_time;
};

class BatchIndexWriter {
public:
    BatchIndexWriter(
        Lexicon& lexicon,
        ForwardIndexBuilder& forward_builder,
        InvertedIndexBuilder& inverted_builder,
        DocumentMetadata& metadata,
        DocURLMapper& url_mapper,
        size_t batch_size = 10,
        std::chrono::seconds flush_interval = std::chrono::seconds(30)
    );
    
    ~BatchIndexWriter();
    
    // Thread-safe: Add document to batch queue
    void enqueue_document(PendingDocument doc);
    
    // Force immediate flush (blocking)
    void flush_now();
    
    // Get statistics
    struct Stats {
        size_t documents_queued = 0;
        size_t documents_indexed = 0;
        size_t batches_flushed = 0;
        double avg_batch_time_ms = 0.0;
        size_t current_queue_size = 0;
    };
    Stats get_stats() const;
    
private:
    void writer_thread();
    void flush_batch(std::vector<PendingDocument>& batch);
    void update_indices(const std::vector<PendingDocument>& batch);
    
    Lexicon& lexicon_;
    ForwardIndexBuilder& forward_builder_;
    InvertedIndexBuilder& inverted_builder_;
    DocumentMetadata& metadata_;
    DocURLMapper& url_mapper_;
    
    std::vector<PendingDocument> queue_;
    std::mutex queue_mutex_;
    std::mutex flush_mutex_;  // Prevents concurrent flushes
    std::condition_variable queue_cv_;
    std::thread writer_thread_;
    std::atomic<bool> shutdown_{false};
    
    size_t batch_size_;
    std::chrono::seconds flush_interval_;
    
    mutable std::mutex stats_mutex_;
    Stats stats_{};
    std::chrono::steady_clock::time_point last_flush_time_;
};
