#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm> 
#include <chrono>
#include <unordered_map>
#include <iomanip>
#include "json.hpp" 
#include "lexicon.hpp" 

using json = nlohmann::json;
using namespace std::chrono;

struct SearchResult {
    int doc_id;
    int score;
};

bool compareResults(const SearchResult& a, const SearchResult& b) {
    return a.score > b.score;
}

int main() {
    try {
        std::cout << "\nLoading Lexicon...\n";
        Lexicon lex;
        if (!lex.load_from_json("backend/data/processed/lexicon.json")) {
            std::cerr << "Failed to load Lexicon.\n";
            return 1;
        }

        std::unordered_map<int, json> barrel_cache;

        std::cout << "\n=========================================\n";
        std::cout << "   SEARCH ENGINE READY (Sorted by TF)    \n";
        std::cout << "=========================================\n";

        while (true) {
            std::string query;
            std::cout << "\nSearch > ";
            std::getline(std::cin, query);

            if (query == "exit" || query.empty()) break;

            auto start_time = high_resolution_clock::now();

            std::string clean_query;
            for(char c : query) if(!isspace(c)) clean_query += tolower(c);

            int word_id = lex.get_word_index(clean_query);
            if (word_id == -1) {
                std::cout << "Word not found in Lexicon.\n";
                continue;
            }

            int barrel_id = word_id % 100;
            
            if (barrel_cache.find(barrel_id) == barrel_cache.end()) {
                std::string path = "backend/data/processed/barrels/inverted_barrel_" + std::to_string(barrel_id) + ".json";
                std::ifstream f(path);
                if (!f.is_open()) {
                    std::cerr << "Error: Missing barrel " << path << "\n";
                    continue;
                }
                json j;
                f >> j;
                barrel_cache[barrel_id] = j;
            }

            json& barrel = barrel_cache[barrel_id];
            std::string id_str = std::to_string(word_id);
            
            if (barrel.contains(id_str)) {
                auto raw_results = barrel[id_str];
                
                std::vector<SearchResult> ranked_results;
                for (auto& entry : raw_results) {
                    int doc_id = entry[0];
                    int freq = entry[1]; 
                    ranked_results.push_back({doc_id, freq});
                }

                std::sort(ranked_results.begin(), ranked_results.end(), compareResults);

                auto stop_time = high_resolution_clock::now();
                auto duration = duration_cast<microseconds>(stop_time - start_time);

                std::cout << "\nFound " << ranked_results.size() << " results (" << duration.count() / 1000.0 << " ms)\n";
                std::cout << std::string(40, '-') << "\n";
                
                // Table Header
                std::cout << std::left 
                          << std::setw(8) << "Rank" 
                          << std::setw(15) << "Doc ID" 
                          << std::setw(10) << "Score" 
                          << "\n";
                std::cout << std::string(40, '-') << "\n";
                
                // Table Rows
                int count = 0;
                for (const auto& res : ranked_results) {
                    std::cout << std::left 
                              << std::setw(8) << (count + 1)
                              << std::setw(15) << res.doc_id 
                              << std::setw(10) << res.score 
                              << "\n";
                    
                    if (++count >= 15) break; // Show top 15
                }
                std::cout << std::string(40, '-') << "\n";

            } else {
                std::cout << "Word ID exists in lexicon, but no docs found in index.\n";
            }
        }

    } catch (const std::exception& e) {
        std::cerr << "\nERROR: " << e.what() << "\n";
    }

    return 0;
}