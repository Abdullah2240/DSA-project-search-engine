#pragma once
// lexicon.hpp
//Lexicon class for building a vocabulary from a JSONL file
//It Uses document-frequency (counts once per document)
//Filters stopwords, short tokens, pure numbers
//Configurable min frequency and max-frequency percentile cutoff
//Saves lexicon as JSON with word_to_index (object) and index_to_word (array)
//Uses C++17 (std::filesystem) for directory creation

// The implementation is in lexicon.cpp
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <climits>
#include <iostream>
#include <filesystem>

class Lexicon {
public:
    Lexicon();

    // Configuration
    void set_min_frequency(int freq);
    void set_max_frequency_percentile(int percentile);
    void set_stopwords_path(const std::string& path);

    // Core methods for lexicon
    bool build_from_jsonl(const std::string& cleaned_data_path, const std::string& output_path);
    bool save_to_json(const std::string& output_path) const;
    bool load_from_json(const std::string& lexicon_path);

    // Lookups 
    int get_word_index(const std::string& word) const;
    std::string get_word(int index) const;
    size_t size() const;
    bool contains_word(const std::string& word) const;

private:
    // Internal containers
    std::unordered_map<std::string,int> word_to_index_;
    std::vector<std::string> index_to_word_;
    std::unordered_set<std::string> stop_words_;

    // Configs
    int min_frequency_ = 2;
    int max_frequency_percentile_ = 99;
    std::string stopwords_path_;

    // Helperss
    void load_default_stopwords();
    void load_stopwords_from_file(const std::string& path);
    bool is_significant_word(const std::string& word) const;
    std::vector<std::string> parse_tokens_from_jsonl_line(const std::string& json_line) const;
    std::string json_escape(const std::string& str) const;
};