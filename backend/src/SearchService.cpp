#include "../include/SearchService.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <cctype>
#include <cmath>
#include <sstream>
#include <thread>
#include <mutex>
#include <future>
#include <chrono>
#include <cstdint>

struct SearchResult {
    int doc_id;
    std::string url;
    double score;
    int publication_year;
    int cited_by_count;
};

bool compareResults(const SearchResult& a, const SearchResult& b) {
    if (std::abs(a.score - b.score) > 1e-6) {
        return a.score > b.score;
    }
    if (a.publication_year != b.publication_year) {
        return a.publication_year > b.publication_year;
    }
    return a.cited_by_count > b.cited_by_count;
}

std::vector<std::string> split_query(const std::string& str) {
    std::vector<std::string> words;
    std::stringstream ss(str);
    std::string word;
    while (ss >> word) {
        words.push_back(word);
    }
    return words;
}

SearchService::SearchService() {
    std::cout << "[Engine] Initializing Search Service...\n";
    
    // Load lexicon with trie
    if (!lexicon_trie_.load_from_json("data/processed/lexicon.json")) {
        std::cerr << "[Engine] CRITICAL: Could not load lexicon.json\n";
    } else {
        std::cout << "[Engine] Lexicon loaded: " << lexicon_trie_.size() << " words\n";
        std::cout << "[Engine] Trie built and ready for autocomplete\n";
    }
    
    // Load URL mapper
    if (!doc_url_mapper.load("data/processed/docid_to_url.json")) {
        std::cerr << "[Engine] WARNING: Could not load doc_url_map.json\n";
    }
    
    // Pre-allocate barrel cache (100 barrels max)
    barrel_cache_.reserve(100);
    
    // Load document metadata for ranking
    if (!document_metadata_.load("data/processed/document_metadata.json")) {
        std::cerr << "[Engine] WARNING: Could not load document_metadata.json\n";
        std::cerr << "[Engine] Run extract_metadata.py to generate metadata file\n";
    } else {
        std::cout << "[Engine] Document metadata loaded: " << document_metadata_.size() << " documents\n";
    }
    
    // NEW: Load all document stats into memory for O(1) lookup
    load_document_stats();

    // Load delta index
    load_delta_index();

    std::string doc_vectors_path = "data/processed/document_vectors.bin";
    std::string word_embeddings_path = "data/processed/word_embeddings.bin";
    
    semantic_search_enabled_ = semantic_scorer_.load_document_vectors(doc_vectors_path) && semantic_scorer_.load_word_embeddings(word_embeddings_path);
    if(semantic_search_enabled_) {
        std::cout << "[Engine] Semantic Search Ready!";
    }
    std::cout << "[Engine] Search Service ready!\n";

}

// NEW: Load all document lengths and title frequencies into RAM
bool SearchService::is_cache_valid(const std::string& cache_path, const std::string& source_path) {
    std::ifstream cache(cache_path, std::ios::binary);
    std::ifstream source(source_path);
    
    if (!cache.is_open() || !source.is_open()) return false;
    
    // Get file modification times (simple check: compare file sizes)
    cache.seekg(0, std::ios::end);
    source.seekg(0, std::ios::end);
    
    return cache.tellg() > 0; // Cache exists and has content
}

bool SearchService::load_doc_stats_from_cache(const std::string& cache_path) {
    std::ifstream f(cache_path, std::ios::binary);
    if (!f.is_open()) return false;
    
    // Read header
    uint32_t num_docs;
    f.read(reinterpret_cast<char*>(&num_docs), sizeof(num_docs));
    
    if (num_docs == 0 || num_docs > 10000000) return false; // Sanity check
    
    doc_stats_cache_.reserve(num_docs);
    
    // Read each document's stats
    for (uint32_t i = 0; i < num_docs; ++i) {
        int doc_id;
        int doc_length;
        uint32_t num_title_freqs;
        
        f.read(reinterpret_cast<char*>(&doc_id), sizeof(doc_id));
        f.read(reinterpret_cast<char*>(&doc_length), sizeof(doc_length));
        f.read(reinterpret_cast<char*>(&num_title_freqs), sizeof(num_title_freqs));
        
        DocStats stats;
        stats.doc_length = doc_length;
        
        // Read title frequencies
        for (uint32_t j = 0; j < num_title_freqs; ++j) {
            int word_id;
            int freq;
            f.read(reinterpret_cast<char*>(&word_id), sizeof(word_id));
            f.read(reinterpret_cast<char*>(&freq), sizeof(freq));
            stats.title_frequencies[word_id] = freq;
        }
        
        doc_stats_cache_[doc_id] = std::move(stats);
    }
    
    return !f.fail();
}

void SearchService::build_doc_stats_cache(const std::string& cache_path) {
    std::cout << "[Engine] Building doc stats cache from forward_index.jsonl...\n";
    
    std::ifstream f("data/processed/forward_index.jsonl");
    if (!f.is_open()) {
        std::cerr << "[Engine] ERROR: Could not open forward_index.jsonl\n";
        return;
    }

    std::string line;
    line.reserve(4096);
    int docs_loaded = 0;
    
    doc_stats_cache_.clear();
    doc_stats_cache_.reserve(50000);

    while (std::getline(f, line)) {
        if (line.empty()) continue;

        try {
            json doc_line = json::parse(line);
            
            if (!doc_line.contains("doc_id") || !doc_line.contains("data")) continue;

            int doc_id = std::stoi(doc_line["doc_id"].get<std::string>());
            json& data = doc_line["data"];

            DocStats stats;
            stats.doc_length = data.value("doc_length", 0);

            if (data.contains("words")) {
                for (auto& word_item : data["words"].items()) {
                    int word_id = std::stoi(word_item.key());
                    json& word_stats = word_item.value();
                    
                    if (word_stats.contains("title_frequency")) {
                        int title_freq = word_stats["title_frequency"].get<int>();
                        if (title_freq > 0) {
                            stats.title_frequencies[word_id] = title_freq;
                        }
                    }
                }
            }

            doc_stats_cache_[doc_id] = std::move(stats);
            docs_loaded++;

        } catch (const std::exception&) {
            continue;
        }
    }
    
    // Write binary cache
    std::ofstream out(cache_path, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "[Engine] WARNING: Could not create cache file\n";
        return;
    }
    
    uint32_t num_docs = static_cast<uint32_t>(doc_stats_cache_.size());
    out.write(reinterpret_cast<const char*>(&num_docs), sizeof(num_docs));
    
    for (const auto& [doc_id, stats] : doc_stats_cache_) {
        out.write(reinterpret_cast<const char*>(&doc_id), sizeof(doc_id));
        out.write(reinterpret_cast<const char*>(&stats.doc_length), sizeof(stats.doc_length));
        
        uint32_t num_title_freqs = static_cast<uint32_t>(stats.title_frequencies.size());
        out.write(reinterpret_cast<const char*>(&num_title_freqs), sizeof(num_title_freqs));
        
        for (const auto& [word_id, freq] : stats.title_frequencies) {
            out.write(reinterpret_cast<const char*>(&word_id), sizeof(word_id));
            out.write(reinterpret_cast<const char*>(&freq), sizeof(freq));
        }
    }
    
    std::cout << "[Engine] ✅ Cache built: " << doc_stats_cache_.size() << " documents\n";
}

void SearchService::load_document_stats() {
    std::string cache_path = "data/processed/doc_stats.bin";
    
    // Try loading from binary cache first (100x faster)
    if (is_cache_valid(cache_path, "data/processed/forward_index.jsonl")) {
        std::cout << "[Engine] Loading from binary cache...\n";
        auto start = std::chrono::high_resolution_clock::now();
        
        if (load_doc_stats_from_cache(cache_path)) {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            
            std::cout << "[Engine] ⚡ Loaded " << doc_stats_cache_.size() 
                      << " documents in " << duration << "ms (from cache)\n";
            return;
        } else {
            std::cout << "[Engine] Cache corrupted, rebuilding...\n";
        }
    }
    
    // Cache miss or invalid - build from scratch
    std::cout << "[Engine] No valid cache found, building from forward_index.jsonl...\n";
    auto start = std::chrono::high_resolution_clock::now();
    
    build_doc_stats_cache(cache_path);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    std::cout << "[Engine] ✅ Built cache in " << duration << "ms\n";
    
    size_t estimated_mem = doc_stats_cache_.size() * (sizeof(int) + sizeof(DocStats) + 50);
    std::cout << "[Engine] Memory usage: " << (estimated_mem / 1024 / 1024) << " MB\n";
}

void SearchService::load_delta_index() {
    std::ifstream f("data/processed/barrels/inverted_delta.json");
    if (!f.good()) {
        std::cout << "[Engine] No delta index found (this is normal for fresh builds)\n";
        return;
    }

    try {
        json j;
        f >> j;
        for (auto& item : j.items()) {
            int word_id = std::stoi(item.key());
            std::vector<DeltaEntry> entries;
            for (auto& elem : item.value()) {
                entries.push_back({
                    elem[0].get<int>(),
                    elem[1].get<int>(),
                    elem[2].get<std::vector<int>>()
                });
            }
            delta_index_[word_id] = std::move(entries);
        }
        std::cout << "[Engine] Delta Index loaded: " << delta_index_.size() << " words\n";
    } catch (const std::exception& e) {
        std::cerr << "[Engine] Error loading delta index: " << e.what() << "\n";
    }
}

// Barrel cache with LRU eviction
json& SearchService::get_barrel(int barrel_id) {
    // Check if already in cache
    auto it = barrel_cache_.find(barrel_id);
    if (it != barrel_cache_.end()) {
        return it->second;
    }
    
    // Limit cache size to prevent memory overflow (max 30 barrels in memory)
    // 30 barrels * ~5MB each = ~150MB max
    if (barrel_cache_.size() >= 30) {
        // Simple eviction: clear oldest half
        auto erase_it = barrel_cache_.begin();
        std::advance(erase_it, 15);
        barrel_cache_.erase(barrel_cache_.begin(), erase_it);
    }
    
    std::string path = "data/processed/barrels/inverted_barrel_" + std::to_string(barrel_id) + ".json";
    std::ifstream f(path);
    
    if (f.is_open()) {
        json j;
        f >> j;
        barrel_cache_[barrel_id] = std::move(j);
    } else {
        std::cerr << "[Engine] WARNING: Could not load barrel " << barrel_id << "\n";
        barrel_cache_[barrel_id] = json::object();
    }
    
    return barrel_cache_[barrel_id];
}

// Fast O(1) memory lookup for title frequency
int SearchService::get_title_frequency(int doc_id, int word_id) {
    auto doc_it = doc_stats_cache_.find(doc_id);
    if (doc_it == doc_stats_cache_.end()) {
        return 0;
    }
    
    auto word_it = doc_it->second.title_frequencies.find(word_id);
    if (word_it == doc_it->second.title_frequencies.end()) {
        return 0;
    }
    
    return static_cast<int>(word_it->second);
}

// Fast O(1) memory lookup for document length
int SearchService::get_document_length(int doc_id) {
    auto it = doc_stats_cache_.find(doc_id);
    if (it == doc_stats_cache_.end()) {
        return 0;
    }
    return it->second.doc_length;
}

std::string SearchService::search(std::string query) {
    json response_json;
    response_json["query"] = query;
    response_json["results"] = json::array();

    // 1. Clean and Split Query
    std::string clean_query_str;
    clean_query_str.reserve(query.size());
    for(char c : query) {
        if(std::isalnum(static_cast<unsigned char>(c))) {
            clean_query_str += std::tolower(static_cast<unsigned char>(c));
        } else {
            clean_query_str += ' ';
        }
    }

    std::vector<std::string> query_words = split_query(clean_query_str);
    if (query_words.empty()) return response_json.dump();

    // Pre-allocate with estimated size
    std::unordered_map<int, double> doc_scores;
    doc_scores.reserve(2000);
    std::unordered_map<int, int> doc_match_count;
    doc_match_count.reserve(2000);
    std::unordered_map<int, std::map<int, std::vector<int>>> doc_positions_map;

    int valid_query_words = 0;

    // 2. Process query words (sequential is faster for small queries due to overhead)
    for (size_t i = 0; i < query_words.size(); ++i) {
        std::string word = query_words[i];
        int word_id = lexicon_trie_.get_word_index(word);

        if (word_id != -1) {
            valid_query_words++;
            std::string id_str = std::to_string(word_id);
            
            int barrel_id = word_id % 100;
            json& barrel = get_barrel(barrel_id);
            
            std::vector<DeltaEntry> combined_entries;
            combined_entries.reserve(500);

            // Combine main index and delta index
            if (barrel.contains(id_str)) {
                auto& raw = barrel[id_str];
                combined_entries.reserve(raw.size());
                for (auto& entry : raw) {
                    if (entry.size() >= 3) {
                        combined_entries.push_back({
                            entry[0].get<int>(),
                            entry[1].get<int>(),
                            entry[2].get<std::vector<int>>()
                        });
                    }
                }
            }

            if (delta_index_.count(word_id)) {
                const auto& deltas = delta_index_[word_id];
                combined_entries.insert(combined_entries.end(), deltas.begin(), deltas.end());
            }

            // Process entries
            for (auto& entry : combined_entries) {
                int doc_id = entry.doc_id;
                int weighted_freq = entry.frequency;
                const std::vector<int>& positions = entry.positions;

                // OPTIMIZED: Memory lookups instead of disk I/O
                int title_freq = get_title_frequency(doc_id, word_id);
                int doc_len = get_document_length(doc_id);

                ScoreComponents scores = ranking_scorer_.calculate_score(
                    weighted_freq,
                    title_freq,
                    positions,
                    doc_id,
                    doc_len,
                    &document_metadata_
                );

                doc_scores[doc_id] += scores.final_score;
                doc_match_count[doc_id]++;
                doc_positions_map[doc_id][static_cast<int>(i)] = positions;
            }
        }
    }

    if (valid_query_words == 0) return response_json.dump();

    // 3. Filter and Apply Proximity
    std::vector<SearchResult> final_results;
    final_results.reserve(std::min(static_cast<size_t>(500), doc_match_count.size()));

    for (const auto& [doc_id, count] : doc_match_count) {
        // Only include documents that match ALL query words
        if (count == valid_query_words) {
            double final_score = doc_scores[doc_id];

            // Proximity bonus for adjacent words
            for (int k = 0; k < static_cast<int>(query_words.size()) - 1; ++k) {
                if (doc_positions_map[doc_id].count(k) && doc_positions_map[doc_id].count(k + 1)) {
                    const auto& posA = doc_positions_map[doc_id][k];
                    const auto& posB = doc_positions_map[doc_id][k + 1];

                    bool found_adjacent = false;
                    for (int a : posA) {
                        for (int b : posB) {
                            if (b == a + 1) {
                                found_adjacent = true;
                                break;
                            }
                        }
                        if (found_adjacent) break;
                    }
                    if (found_adjacent) final_score += 100.0;
                }
            }

            std::string url = doc_url_mapper.get(doc_id);
            int pub_year = document_metadata_.get_publication_year(doc_id);
            int citations = document_metadata_.get_cited_by_count(doc_id);

            final_results.push_back({doc_id, url, final_score, pub_year, citations});
        }
    }

    // After final_results is populated with initial search results

// 4. Apply semantic scoring if available
if (semantic_search_enabled_ && !final_results.empty()) {
    std::cout << "[Engine] Computing semantic scores for " << final_results.size() << " documents\n";
    
    // Get semantic scores for all results
    std::vector<double> semantic_scores;
    semantic_scores.reserve(final_results.size());
    
    for (const auto& result : final_results) {
        double sem_score = semantic_scorer_.compute_similarity(result.doc_id, query_words);
        semantic_scores.push_back(sem_score);
    }
    
    // Find min and max for normalization
    auto [min_it, max_it] = std::minmax_element(semantic_scores.begin(), semantic_scores.end());
    double min_score = *min_it;
    double max_score = *max_it;
    double range = max_score - min_score;
    
    if (range > 0) {
        // Update scores with 60% lexical + 40% semantic
        for (size_t i = 0; i < final_results.size(); i++) {
            double normalized_semantic = (semantic_scores[i] - min_score) / range;
            final_results[i].score = 0.6 * final_results[i].score + 0.4 * normalized_semantic;
        }
    }
}

// 4. Partial sort for top 50 (faster than full sort)
if (final_results.size() > 50) {
    std::partial_sort(final_results.begin(), final_results.begin() + 50, 
                     final_results.end(), compareResults);
    final_results.resize(50);
} else {
    std::sort(final_results.begin(), final_results.end(), compareResults);
}

// 5. Build JSON response
for (const auto& res : final_results) {
    json item;
    item["docId"] = res.doc_id;
    item["score"] = res.score;
    item["url"] = res.url;
    
    // Add title from metadata
    const DocMetadata* meta = document_metadata_.get_metadata(res.doc_id);
    if (meta && !meta->title.empty()) {
        item["title"] = meta->title;
    } else {
        item["title"] = "Document #" + std::to_string(res.doc_id);
    }
    
    if (res.publication_year > 0) item["publication_year"] = res.publication_year;
    if (res.cited_by_count > 0) item["cited_by_count"] = res.cited_by_count;
    
    response_json["results"].push_back(item);
}
    
return response_json.dump();
}

std::string SearchService::autocomplete(const std::string& prefix, int limit) {
    json response_json;
    response_json["prefix"] = prefix;
    response_json["suggestions"] = json::array();
    
    if (prefix.empty() || limit <= 0) {
        return response_json.dump();
    }
    
    std::string clean_prefix;
    clean_prefix.reserve(prefix.size());
    for (char c : prefix) {
        if (!std::isspace(c)) {
            clean_prefix += std::tolower(static_cast<unsigned char>(c));
        }
    }
    
    std::vector<std::string> suggestions = lexicon_trie_.autocomplete(clean_prefix, limit);
    
    for (const auto& suggestion : suggestions) {
        response_json["suggestions"].push_back(suggestion);
    }
    
    return response_json.dump();
}

void SearchService::reload_delta_index() {
    std::cout << "[Engine] Reloading delta index..." << std::endl;
    delta_index_.clear();
    load_delta_index();
    std::cout << "[Engine] Delta index reloaded successfully" << std::endl;
}

void SearchService::reload_metadata() {
    std::cout << "[Engine] Reloading metadata..." << std::endl;
    document_metadata_.load("data/processed/document_metadata.json");
    std::cout << "[Engine] Metadata reloaded: " << document_metadata_.size() << " documents" << std::endl;
}
