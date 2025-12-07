#pragma once
// LexiconWithTrie.hpp
// Wrapper class that integrates our Lexicon with Trie for autocomplete functionality
// wewill keep the original lexicon functionality intact
#include "lexicon.hpp"
#include "Trie.hpp"
#include <string>
#include <vector>

class LexiconWithTrie {
public:
    LexiconWithTrie();

    // Configuration methods (forwarded to Lexicon)
    void set_min_frequency(int freq);
    void set_max_frequency_percentile(int percentile);
    void set_stopwords_path(const std::string& path);

    // Core lexicon methods (forwarded to Lexicon, with Trie rebuild)
    bool build_from_jsonl(const std::string& cleaned_data_path, const std::string& output_path);
    bool save_to_json(const std::string& output_path) const;
    bool load_from_json(const std::string& lexicon_path);

    // Lexicon lookup methods (forwarded to Lexicon)
    int get_word_index(const std::string& word) const;
    std::string get_word(int index) const;
    size_t size() const;
    bool contains_word(const std::string& word) const;

    // Autocomplete functionality
    std::vector<std::string> autocomplete(const std::string& prefix, int k) const;

    // Access to underlying Lexicon (if needed)
    const Lexicon& get_lexicon() const { return lexicon_; }
    Lexicon& get_lexicon() { return lexicon_; }

private:
    Lexicon lexicon_;
    Trie trie_;

    // Rebuild the trie from the current lexicon vocabulary
    void rebuild_trie();
};

