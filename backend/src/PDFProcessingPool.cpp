#include "PDFProcessingPool.hpp"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include "json.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

PDFProcessingPool::PDFProcessingPool(
    size_t num_threads,
    BatchIndexWriter& batch_writer,
    Lexicon& lexicon
) : batch_writer_(batch_writer), lexicon_(lexicon) {
    
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back(&PDFProcessingPool::worker_thread, this);
    }
    
    stats_.active_workers = num_threads;
    
    std::cout << "[PDFProcessingPool] Started with " << num_threads << " workers\n";
}

PDFProcessingPool::~PDFProcessingPool() {
    shutdown_ = true;
    queue_cv_.notify_all();
    
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

std::future<int> PDFProcessingPool::submit_pdf(
    const std::string& pdf_path,
    int doc_id
) {
    Task task;
    task.pdf_path = pdf_path;
    task.doc_id = doc_id;
    
    auto future = task.result.get_future();
    
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push(std::move(task));
        
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        stats_.queue_size = task_queue_.size();
    }
    
    queue_cv_.notify_one();
    
    return future;
}

PDFProcessingPool::Stats PDFProcessingPool::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void PDFProcessingPool::worker_thread() {
    while (!shutdown_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        
        queue_cv_.wait(lock, [this]() {
            return shutdown_ || !task_queue_.empty();
        });
        
        if (shutdown_ && task_queue_.empty()) break;
        
        if (task_queue_.empty()) continue;
        
        Task task = std::move(task_queue_.front());
        task_queue_.pop();
        
        {
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.queue_size = task_queue_.size();
        }
        
        lock.unlock();
        
        try {
            process_pdf(task);
        } catch (const std::exception& e) {
            std::cerr << "[PDFProcessingPool] Error processing " 
                      << task.pdf_path << ": " << e.what() << "\n";
            task.result.set_exception(std::current_exception());
            
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            stats_.failed_tasks++;
        }
    }
}

void PDFProcessingPool::process_pdf(Task& task) {
    auto start = std::chrono::steady_clock::now();
    
    std::cout << "[PDFProcessingPool] Processing doc_id=" << task.doc_id 
              << " (" << task.pdf_path << ")\n";
    
    // 1. Call Python tokenizer
    ProcessedPDF processed = call_python_tokenizer(task.pdf_path, task.doc_id);
    if (!processed.success) {
        throw std::runtime_error(processed.error);
    }
    
    // 2. Build doc stats
    auto doc_stats = build_doc_stats(processed.tokens);
    
    // 3. Submit to batch writer
    PendingDocument pending;
    pending.doc_id = task.doc_id;
    pending.title = processed.title;
    pending.tokens = std::move(processed.tokens);
    pending.doc_stats = std::move(doc_stats);
    pending.url = "uploaded://" + fs::path(task.pdf_path).filename().string();
    pending.pdf_path = task.pdf_path;
    
    batch_writer_.enqueue_document(std::move(pending));
    
    auto end = std::chrono::steady_clock::now();
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    std::cout << "[PDFProcessingPool] ✅ doc_id=" << task.doc_id 
              << " processed in " << duration_ms << "ms\n";
    
    task.result.set_value(task.doc_id);
    
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    stats_.completed_tasks++;
}

ProcessedPDF PDFProcessingPool::call_python_tokenizer(
    const std::string& pdf_path, 
    int doc_id
) {
    ProcessedPDF result;
    result.doc_id = doc_id;
    result.success = false;
    
    std::string temp_dir = "data/temp_json";
    if (!fs::exists(temp_dir)) {
        fs::create_directories(temp_dir);
    }
    
    std::string temp_json = temp_dir + "/temp_" + std::to_string(doc_id) + ".json";
    
    std::string python_exe;
    #ifdef _WIN32
        if (fs::exists("venv/Scripts/python.exe")) {
            python_exe = "venv\\Scripts\\python.exe";
        } else {
            python_exe = "python";
        }
    #else
        if (fs::exists("venv/bin/python")) {
            python_exe = "venv/bin/python";
        } else {
            python_exe = "python3";
        }
    #endif
    
    std::string python_cmd = python_exe + " scripts/tokenize_single_pdf.py \"" 
                           + pdf_path + "\" " 
                           + std::to_string(doc_id) + " \"" 
                           + temp_json + "\"";
    
    int ret = std::system(python_cmd.c_str());
    
    if (ret != 0) {
        result.error = "Python tokenizer failed";
        fs::remove(temp_json);
        return result;
    }
    
    std::ifstream f(temp_json);
    if (!f.is_open()) {
        result.error = "Could not read tokenized output";
        fs::remove(temp_json);
        return result;
    }
    
    try {
        json j;
        f >> j;
        f.close();
        
        result.title = j.value("title", "Untitled");
        result.tokens = j.value("body_tokens", std::vector<std::string>());
        
        if (result.tokens.empty()) {
            result.error = "No tokens extracted from PDF";
            fs::remove(temp_json);
            return result;
        }
        
        result.success = true;
        fs::remove(temp_json);
        
    } catch (const std::exception& e) {
        result.error = std::string("JSON parse error: ") + e.what();
        fs::remove(temp_json);
        return result;
    }
    
    return result;
}

std::map<int, WordStats> PDFProcessingPool::build_doc_stats(
    const std::vector<std::string>& tokens
) {
    std::map<int, WordStats> doc_stats;
    int pos = 0;
    
    for (const auto& token : tokens) {
        std::string lower_token = token;
        std::transform(lower_token.begin(), lower_token.end(), lower_token.begin(),
                     [](unsigned char c) { return std::tolower(c); });
        
        int word_id = lexicon_.get_word_index(lower_token);
        if (word_id != -1) {
            doc_stats[word_id].body_frequency++;
            doc_stats[word_id].body_positions.push_back(pos);
        }
        pos++;
    }
    
    return doc_stats;
}
