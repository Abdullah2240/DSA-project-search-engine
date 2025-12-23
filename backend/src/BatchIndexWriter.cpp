#include "BatchIndexWriter.hpp"
#include <iostream>
#include <fstream>
#include <algorithm>

BatchIndexWriter::BatchIndexWriter(
    Lexicon& lexicon,
    ForwardIndexBuilder& forward_builder,
    InvertedIndexBuilder& inverted_builder,
    DocumentMetadata& metadata,
    DocURLMapper& url_mapper,
    size_t batch_size,
    std::chrono::seconds flush_interval
) : lexicon_(lexicon),
    forward_builder_(forward_builder),
    inverted_builder_(inverted_builder),
    metadata_(metadata),
    url_mapper_(url_mapper),
    batch_size_(batch_size),
    flush_interval_(flush_interval),
    last_flush_time_(std::chrono::steady_clock::now())
{
    writer_thread_ = std::thread(&BatchIndexWriter::writer_thread, this);
    std::cout << "[BatchIndexWriter] Started with batch_size=" << batch_size 
              << ", flush_interval=" << flush_interval.count() << "s\n";
}

BatchIndexWriter::~BatchIndexWriter() {
    shutdown_ = true;
    queue_cv_.notify_all();
    if (writer_thread_.joinable()) {
        writer_thread_.join();
    }
    
    // Flush remaining documents
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (!queue_.empty()) {
        std::cout << "[BatchIndexWriter] Flushing " << queue_.size() 
                  << " remaining documents on shutdown\n";
        flush_batch(queue_);
    }
}

void BatchIndexWriter::enqueue_document(PendingDocument doc) {
    doc.enqueue_time = std::chrono::steady_clock::now();
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        queue_.push_back(std::move(doc));
        
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.documents_queued++;
        stats_.current_queue_size = queue_.size();
    }
    
    queue_cv_.notify_one();
}

void BatchIndexWriter::flush_now() {
    std::cout << "[BatchIndexWriter] flush_now() called - acquiring flush lock..." << std::endl;
    
    // CRITICAL: Lock flush_mutex FIRST to prevent concurrent flushes
    std::lock_guard<std::mutex> flush_lock(flush_mutex_);
    
    std::unique_lock<std::mutex> lock(queue_mutex_);
    if (queue_.empty()) {
        std::cout << "[BatchIndexWriter] Queue empty, nothing to flush" << std::endl;
        return;
    }
    
    std::vector<PendingDocument> batch;
    batch.swap(queue_);
    
    {
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.current_queue_size = 0;
    }
    
    lock.unlock();
    
    std::cout << "[BatchIndexWriter] flush_now() starting synchronous flush..." << std::endl;
    flush_batch(batch);
    std::cout << "[BatchIndexWriter] flush_now() completed! Files written to disk." << std::endl;
}

BatchIndexWriter::Stats BatchIndexWriter::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void BatchIndexWriter::writer_thread() {
    while (!shutdown_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        queue_cv_.wait_for(lock, flush_interval_, [this]() {
            auto time_since_flush = std::chrono::steady_clock::now() - last_flush_time_;
            return shutdown_ || 
                   queue_.size() >= batch_size_ ||
                   time_since_flush >= flush_interval_;
        });
        
        if (queue_.empty()) continue;
        
        // Extract batch
        std::vector<PendingDocument> batch;
        auto time_since_flush = std::chrono::steady_clock::now() - last_flush_time_;
        
        if (queue_.size() >= batch_size_ || time_since_flush >= flush_interval_) {
            size_t take = std::min(queue_.size(), batch_size_);
            batch.insert(batch.end(), 
                        std::make_move_iterator(queue_.begin()), 
                        std::make_move_iterator(queue_.begin() + take));
            queue_.erase(queue_.begin(), queue_.begin() + take);
            
            {
                std::lock_guard<std::mutex> stats_lock(stats_mutex_);
                stats_.current_queue_size = queue_.size();
            }
        }
        
        lock.unlock();
        
        if (!batch.empty()) {
            // Acquire flush lock to prevent concurrent flushes
            std::lock_guard<std::mutex> flush_lock(flush_mutex_);
            flush_batch(batch);
        }
    }
}

void BatchIndexWriter::flush_batch(std::vector<PendingDocument>& batch) {
    auto start = std::chrono::steady_clock::now();
    
    std::cout << "[BatchIndexWriter] Flushing batch of " << batch.size() << " documents...\n";
    
    try {
        update_indices(batch);
        
        auto end = std::chrono::steady_clock::now();
        auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        
        double total_latency = 0;
        for (const auto& doc : batch) {
            auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
                end - doc.enqueue_time
            ).count();
            total_latency += latency;
        }
        double avg_latency = total_latency / batch.size();
        
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.documents_indexed += batch.size();
        stats_.batches_flushed++;
        stats_.avg_batch_time_ms = 
            (stats_.avg_batch_time_ms * (stats_.batches_flushed - 1) + duration_ms) / 
            stats_.batches_flushed;
        
        std::cout << "[BatchIndexWriter] ✅ Batch complete in " << duration_ms << "ms "
                  << "(" << (duration_ms / batch.size()) << "ms/doc, "
                  << "avg latency: " << avg_latency << "ms)\n";
        
        last_flush_time_ = std::chrono::steady_clock::now();
        
    } catch (const std::exception& e) {
        std::cerr << "[BatchIndexWriter] ❌ Batch flush failed: " << e.what() << "\n";
    }
}

void BatchIndexWriter::update_indices(const std::vector<PendingDocument>& batch) {
    // 1. Batch lexicon updates
    std::vector<std::string> all_tokens;
    for (const auto& doc : batch) {
        all_tokens.insert(all_tokens.end(), doc.tokens.begin(), doc.tokens.end());
    }
    
    if (!all_tokens.empty()) {
        lexicon_.update_from_tokens(all_tokens, "data/processed/lexicon.json");
    }
    
    // 2. Batch forward index updates
    std::ofstream forward_file("data/processed/forward_index.jsonl", std::ios::app);
    if (!forward_file.is_open()) {
        throw std::runtime_error("Failed to open forward_index.jsonl");
    }
    
    for (const auto& doc : batch) {
        json words_obj;
        for (const auto& [word_id, stats] : doc.doc_stats) {
            words_obj[std::to_string(word_id)] = {
                {"title_frequency", stats.title_frequency},
                {"body_frequency", stats.body_frequency},
                {"weighted_frequency", stats.get_weighted_frequency()},
                {"title_positions", stats.title_positions},
                {"body_positions", stats.body_positions}
            };
        }
        
        int total_tokens = 0;
        for (const auto& [_, stats] : doc.doc_stats) {
            total_tokens += stats.title_frequency + stats.body_frequency;
        }
        
        json doc_json;
        doc_json["doc_length"] = total_tokens;
        doc_json["title_length"] = 0;
        doc_json["body_length"] = total_tokens;
        doc_json["words"] = words_obj;
        
        json line_obj;
        line_obj["doc_id"] = std::to_string(doc.doc_id);
        line_obj["data"] = doc_json;
        
        forward_file << line_obj.dump(-1) << "\n";
    }
    forward_file.close();
    
    // 3. Batch delta barrel updates
    std::string delta_path = "data/processed/barrels/inverted_delta.json";
    json delta_json;
    
    std::ifstream delta_in(delta_path);
    if (delta_in.good()) {
        try { delta_in >> delta_json; } catch(...) { delta_json = json::object(); }
    }
    delta_in.close();
    
    for (const auto& doc : batch) {
        for (const auto& [word_id, stats] : doc.doc_stats) {
            std::string w_id_str = std::to_string(word_id);
            
            std::vector<int> all_positions = stats.title_positions;
            all_positions.insert(all_positions.end(), 
                               stats.body_positions.begin(), 
                               stats.body_positions.end());
            
            json entry = json::array({
                doc.doc_id,
                stats.get_weighted_frequency(),
                all_positions
            });
            
            if (delta_json.contains(w_id_str)) {
                delta_json[w_id_str].push_back(entry);
            } else {
                delta_json[w_id_str] = json::array({entry});
            }
        }
    }
    
    // Write to temp file first
    std::string delta_temp = delta_path + ".tmp";
    std::ofstream delta_out(delta_temp, std::ios::trunc);
    if (!delta_out.is_open()) {
        throw std::runtime_error("Failed to open delta temp file");
    }
    delta_out << delta_json.dump(-1);
    delta_out.flush();
    if (!delta_out.good()) {
        delta_out.close();
        throw std::runtime_error("Failed to write delta file");
    }
    delta_out.close();
    
    // Atomic rename
    std::rename(delta_temp.c_str(), delta_path.c_str());
    
    // 4. Batch metadata updates
    for (const auto& doc : batch) {
        metadata_.add_document(doc.doc_id, 2024, 1, 0, doc.title, doc.url);
    }
    metadata_.save("data/processed/document_metadata.json");
    
    // 5. Batch URL mappings
    for (const auto& doc : batch) {
        url_mapper_.add_mapping(doc.doc_id, doc.url);
    }
    url_mapper_.save("data/processed/docid_to_url.json");
    
    // 6. Batch test.jsonl updates
    std::ofstream test_file("data/processed/test.jsonl", std::ios::app);
    if (test_file.is_open()) {
        for (const auto& doc : batch) {
            json doc_json;
            doc_json["doc_id"] = doc.doc_id;
            doc_json["title"] = doc.title;
            doc_json["body_tokens"] = doc.tokens;
            doc_json["word_count"] = doc.tokens.size();
            doc_json["pdf_path"] = doc.pdf_path;
            doc_json["url"] = doc.url;
            
            test_file << doc_json.dump(-1) << "\n";
        }
    }
    test_file.close();
}
