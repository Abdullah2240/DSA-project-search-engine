#include "httplib.h" 
#include "SearchService.hpp"
#include "BatchIndexWriter.hpp"
#include "PDFProcessingPool.hpp"
#include "lexicon.hpp"
#include "forward_index.hpp"
#include "inverted_index.hpp"
#include "DocumentMetadata.hpp"
#include "doc_url_mapper.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <vector>

namespace fs = std::filesystem;

// Global progress tracker for upload status
struct UploadProgress {
    int total_files = 0;
    int processed_files = 0;
    int indexed_files = 0;
    std::vector<std::string> current_status;
    std::mutex mutex;
};

static UploadProgress g_upload_progress;

int main() {
    std::cout << "[Main] Initializing search engine...\n";
    SearchService engine;
    
    // Initialize components for PDF processing
    Lexicon lexicon;
    lexicon.load_from_json("data/processed/lexicon.json");
    
    ForwardIndexBuilder forward_builder;
    forward_builder.load_lexicon("data/processed/lexicon.json");
    
    InvertedIndexBuilder inverted_builder(100);
    
    DocumentMetadata metadata;
    metadata.load("data/processed/document_metadata.json");
    
    DocURLMapper url_mapper;
    url_mapper.load("data/processed/docid_to_url.json");
    
    // Initialize batch writer (flushes every 10 docs or 30 seconds)
    BatchIndexWriter batch_writer(
        lexicon,
        forward_builder,
        inverted_builder,
        metadata,
        url_mapper,
        10,  // batch_size
        std::chrono::seconds(30)  // flush_interval
    );
    
    // Initialize processing pool
    size_t num_workers = std::thread::hardware_concurrency();
    if (num_workers == 0) num_workers = 4;
    
    PDFProcessingPool processing_pool(num_workers, batch_writer, lexicon);
    
    std::cout << "[Main] Async processing pool ready with " << num_workers << " workers\n";
    
    httplib::Server svr;

    // CORS middleware - Add CORS headers to all responses
    svr.set_post_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
    });

    // Handle CORS preflight requests
    svr.Options(".*", [](const httplib::Request& req, httplib::Response& res) {
        res.status = 204;
    });

    // Serve static files from the static directory
    svr.set_mount_point("/", "./static");
    
    // Set default MIME types
    svr.set_file_extension_and_mimetype_mapping("js", "application/javascript");
    svr.set_file_extension_and_mimetype_mapping("mjs", "application/javascript");
    svr.set_file_extension_and_mimetype_mapping("css", "text/css");
    svr.set_file_extension_and_mimetype_mapping("html", "text/html");
    svr.set_file_extension_and_mimetype_mapping("svg", "image/svg+xml");
    svr.set_file_extension_and_mimetype_mapping("ico", "image/x-icon");

    // Root endpoint - Redirect to index.html or serve API status
    svr.Get("/api", [](const httplib::Request& req, httplib::Response& res) {
        std::string html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>DSA Search Engine API</title>
    <style>
        body { font-family: 'Segoe UI', Arial, sans-serif; max-width: 800px; margin: 50px auto; padding: 20px; background: #0a0e27; color: #fff; }
        h1 { color: #60a5fa; }
        .endpoint { background: rgba(255,255,255,0.05); padding: 15px; margin: 10px 0; border-radius: 8px; border-left: 4px solid #60a5fa; }
        .method { color: #34d399; font-weight: bold; }
        code { background: rgba(0,0,0,0.3); padding: 2px 6px; border-radius: 4px; }
        a { color: #60a5fa; text-decoration: none; }
        a:hover { text-decoration: underline; }
    </style>
</head>
<body>
    <h1>🚀 DSA Search Engine API</h1>
    <p>Backend server is running successfully!</p>
    <h2>Available Endpoints:</h2>
    <div class="endpoint">
        <span class="method">GET</span> <code>/search?q=&lt;query&gt;</code><br>
        Search for documents matching the query<br>
        <a href="/search?q=computer" target="_blank">Try example: /search?q=computer</a>
    </div>
    <div class="endpoint">
        <span class="method">GET</span> <code>/autocomplete?q=&lt;prefix&gt;&amp;limit=&lt;num&gt;</code><br>
        Get autocomplete suggestions<br>
        <a href="/autocomplete?q=comp&limit=5" target="_blank">Try example: /autocomplete?q=comp&limit=5</a>
    </div>
    <div class="endpoint">
        <span class="method">POST</span> <code>/upload</code><br>
        Upload PDF files (multipart/form-data)
    </div>
    <hr>
    <p>Frontend is served at: <a href="/" target="_blank">http://localhost:8080</a></p>
</body>
</html>
        )";
        res.set_content(html, "text/html");
    });

    // Define Route: /search?q=...
    svr.Get("/search", [&](const httplib::Request& req, httplib::Response& res) {
        if (req.has_param("q")) {
            std::string query = req.get_param_value("q");
            std::string json_output = engine.search(query);
            res.set_content(json_output, "application/json");
        } else {
            res.status = 400;
            res.set_content("{\"error\": \"Missing 'q' parameter\"}", "application/json");
        }
    });

    // Define Route: /autocomplete?q=...&limit=10
    svr.Get("/autocomplete", [&](const httplib::Request& req, httplib::Response& res) {
        if (req.has_param("q")) {
            std::string prefix = req.get_param_value("q");
            int limit = 10;
            if (req.has_param("limit")) {
                try {
                    limit = std::stoi(req.get_param_value("limit"));
                    if (limit < 1) limit = 1;
                    if (limit > 50) limit = 50;
                } catch (...) {
                    limit = 10;
                }
            }
            std::string json_output = engine.autocomplete(prefix, limit);
            res.set_content(json_output, "application/json");
        } else {
            res.status = 400;
            res.set_content("{\"error\": \"Missing 'q' parameter\"}", "application/json");
        }
    });

    // Define Route: /download/:doc_id - Download uploaded PDFs
    svr.Get(R"(/download/(\d+))", [](const httplib::Request& req, httplib::Response& res) {
        try {
            int doc_id = std::stoi(req.matches[1]);
            // Check both locations for PDF
            std::vector<std::string> possible_paths = {
                "data/downloads/" + std::to_string(doc_id) + ".pdf",
                "data/temp_pdfs/" + std::to_string(doc_id) + ".pdf"
            };
            
            std::string pdf_path;
            for (const auto& path : possible_paths) {
                if (fs::exists(path)) {
                    pdf_path = path;
                    break;
                }
            }
            
            if (pdf_path.empty()) {
                res.status = 404;
                res.set_content("{\"error\": \"PDF not found\"}", "application/json");
                return;
            }
            
            // Read file content
            std::ifstream file(pdf_path, std::ios::binary);
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();
            file.close();
            
            // Serve PDF
            res.set_header("Content-Type", "application/pdf");
            res.set_header("Content-Disposition", "attachment; filename=\"document_" + std::to_string(doc_id) + ".pdf\"");
            res.set_content(content, "application/pdf");
            
            std::cout << "[Download] Served PDF for doc_id " << doc_id << std::endl;
            
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content("{\"error\": \"Invalid doc_id\"}", "application/json");
        }
    });

    // NEW: Upload progress endpoint
    svr.Get("/upload-progress", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(g_upload_progress.mutex);
        
        nlohmann::json progress;
        progress["total"] = g_upload_progress.total_files;
        progress["processed"] = g_upload_progress.processed_files;
        progress["indexed"] = g_upload_progress.indexed_files;
        progress["status"] = g_upload_progress.current_status;
        
        res.set_content(progress.dump(), "application/json");
    });

    // OPTIMIZED Route: /upload - Async PDF uploads with concurrent processing
    svr.Post("/upload", [&processing_pool, &batch_writer, &metadata, &engine](
        const httplib::Request& req, httplib::Response& res) {
        
        try {
            // Reset progress tracker
            {
                std::lock_guard<std::mutex> lock(g_upload_progress.mutex);
                g_upload_progress.total_files = 0;
                g_upload_progress.processed_files = 0;
                g_upload_progress.indexed_files = 0;
                g_upload_progress.current_status.clear();
            }
            
            if (!req.has_header("Content-Type") || 
                req.get_header_value("Content-Type").find("multipart/form-data") == std::string::npos) {
                res.status = 400;
                res.set_content("{\"error\": \"Expected multipart/form-data\"}", "application/json");
                return;
            }
            
            auto upload_start = std::chrono::steady_clock::now();
            
            int uploaded_count = 0;
            int failed_count = 0;
            std::vector<std::future<int>> futures;
            std::vector<int> new_doc_ids;
            
            // Get the ACTUAL highest doc_id from metadata file (not the cached count)
            int next_doc_id = 0;
            {
                std::ifstream meta_file("data/processed/document_metadata.json");
                if (meta_file.is_open()) {
                    json meta_json;
                    meta_file >> meta_json;
                    for (auto& [key, value] : meta_json.items()) {
                        int doc_id = std::stoi(key);
                        if (doc_id >= next_doc_id) {
                            next_doc_id = doc_id + 1;
                        }
                    }
                }
            }
            
            if (req.form.has_file("files")) {
                auto files = req.form.get_files("files");
                
                // Set total count
                {
                    std::lock_guard<std::mutex> lock(g_upload_progress.mutex);
                    g_upload_progress.total_files = files.size();
                    g_upload_progress.current_status.push_back("Uploading files...");
                }
                
                for (const auto& file : files) {
                    std::string filename = file.filename;
                    
                    if (filename.size() < 4 || filename.substr(filename.size() - 4) != ".pdf") {
                        failed_count++;
                        continue;
                    }
                    
                    std::string temp_path = "data/temp_pdfs/" + filename;
                    fs::create_directories("data/temp_pdfs");
                    
                    std::ofstream out(temp_path, std::ios::binary);
                    if (!out.is_open()) {
                        failed_count++;
                        continue;
                    }
                    
                    out.write(file.content.data(), file.content.size());
                    out.close();
                    
                    std::cout << "[Upload] Saved: " << filename << " (doc_id will be " 
                              << next_doc_id << ")\n";
                    
                    auto future = processing_pool.submit_pdf(temp_path, next_doc_id);
                    futures.push_back(std::move(future));
                    new_doc_ids.push_back(next_doc_id);
                    
                    next_doc_id++;
                    uploaded_count++;
                }
            }
            
            // Update progress: Processing
            {
                std::lock_guard<std::mutex> lock(g_upload_progress.mutex);
                g_upload_progress.current_status.clear();
                g_upload_progress.current_status.push_back("Processing PDFs (tokenizing, max 5000 tokens)...");
            }
            
            // Wait for all PDFs to be processed (tokenized and queued)
            std::cout << "[Upload] Waiting for " << futures.size() << " documents to be processed...\n";
            for (size_t i = 0; i < futures.size(); ++i) {
                try { 
                    futures[i].wait();
                    
                    // Update progress
                    {
                        std::lock_guard<std::mutex> lock(g_upload_progress.mutex);
                        g_upload_progress.processed_files++;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[Upload] Processing error: " << e.what() << "\n";
                    failed_count++;
                    uploaded_count--;
                }
            }
            
            // Update progress: Indexing
            {
                std::lock_guard<std::mutex> lock(g_upload_progress.mutex);
                g_upload_progress.current_status.clear();
                g_upload_progress.current_status.push_back("Building search indices...");
            }
            
            // Force immediate batch flush to write indices
            if (uploaded_count > 0) {
                std::cout << "[Upload] Flushing batch to index...\n";
                batch_writer.flush_now();
                std::cout << "[Upload] ✅ Batch flush completed!\n";
                
                // Small delay to ensure all file writes are synced to disk
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                std::cout << "[Upload] Reloading search engine indices...\n";
                engine.reload_delta_index();
                engine.reload_metadata();
                
                // Update progress: Done
                {
                    std::lock_guard<std::mutex> lock(g_upload_progress.mutex);
                    g_upload_progress.indexed_files = uploaded_count;
                    g_upload_progress.current_status.clear();
                    g_upload_progress.current_status.push_back("✅ All documents are now searchable!");
                }
                
                std::cout << "[Upload] ✅ Documents indexed and searchable!\n";
            }
            
            auto upload_end = std::chrono::steady_clock::now();
            auto total_time = std::chrono::duration_cast<std::chrono::milliseconds>(upload_end - upload_start).count();
            
            std::string response = "{\"success\": true"
                                  ", \"uploadedCount\": " + std::to_string(uploaded_count) +
                                  ", \"failedCount\": " + std::to_string(failed_count) +
                                  ", \"processingTimeMs\": " + std::to_string(total_time) +
                                  ", \"newDocIds\": [";
            
            for (size_t i = 0; i < std::min(new_doc_ids.size(), static_cast<size_t>(uploaded_count)); ++i) {
                response += std::to_string(new_doc_ids[i]);
                if (i < std::min(new_doc_ids.size(), static_cast<size_t>(uploaded_count)) - 1) {
                    response += ", ";
                }
            }
            response += "], \"message\": \"";
            
            if (uploaded_count > 0) {
                response += std::to_string(uploaded_count) + 
                           " PDF(s) indexed in " + 
                           std::to_string(total_time) + "ms (avg " +
                           std::to_string(total_time / uploaded_count) + "ms each)";
            } else {
                response += "No files uploaded successfully.";
            }
            
            response += "\", \"status\": \"indexed\"}";
            
            res.set_content(response, "application/json");
            std::cout << "[Upload] ✅ Upload complete in " << total_time << "ms\n";
            
        } catch (const std::exception& e) {
            std::cerr << "[Upload] Error: " << e.what() << "\n";
            res.status = 500;
            res.set_content("{\"error\": \"Upload failed\"}", "application/json");
        }
    });

    // Stats endpoint for monitoring
    svr.Get("/stats", [&processing_pool, &batch_writer](
        const httplib::Request&, httplib::Response& res) {
        
        auto pool_stats = processing_pool.get_stats();
        auto batch_stats = batch_writer.get_stats();
        
        nlohmann::json stats_json;
        stats_json["processing_pool"] = {
            {"active_workers", pool_stats.active_workers},
            {"queue_size", pool_stats.queue_size},
            {"completed_tasks", pool_stats.completed_tasks},
            {"failed_tasks", pool_stats.failed_tasks}
        };
        stats_json["batch_writer"] = {
            {"documents_queued", batch_stats.documents_queued},
            {"documents_indexed", batch_stats.documents_indexed},
            {"batches_flushed", batch_stats.batches_flushed},
            {"avg_batch_time_ms", batch_stats.avg_batch_time_ms},
            {"current_queue_size", batch_stats.current_queue_size}
        };
        
        res.set_content(stats_json.dump(2), "application/json");
    });

    std::cout << "======================================" << std::endl;
    std::cout << "   DSA Search Engine - OPTIMIZED" << std::endl;
    std::cout << "======================================" << std::endl;
    std::cout << "API Endpoints:" << std::endl;
    std::cout << "  - GET  /search?q=<query>" << std::endl;
    std::cout << "  - GET  /autocomplete?q=<prefix>&limit=<num>" << std::endl;
    std::cout << "  - POST /upload (multipart/form-data)" << std::endl;
    std::cout << "  - GET  /download/<doc_id>" << std::endl;
    std::cout << "  - GET  /upload-progress" << std::endl;
    std::cout << "  - GET  /stats" << std::endl;
    std::cout << "======================================" << std::endl;
    std::cout << "Upload Speed: Max 5000 tokens, 20 pages" << std::endl;
    std::cout << "Target Time: <35 seconds per PDF" << std::endl;
    std::cout << "Concurrent Processing: " << num_workers << " workers" << std::endl;
    std::cout << "======================================" << std::endl;
    std::cout << "Open: http://localhost:8080" << std::endl;
    std::cout << "======================================" << std::endl;

    if (!svr.listen("0.0.0.0", 8080)) {
        std::cerr << "Failed to start server!" << std::endl;
        return 1;
    }

    return 0;
    
}