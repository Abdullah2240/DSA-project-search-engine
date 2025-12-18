#include "LexiconWithTrie.hpp"

LexiconWithTrie::LexiconWithTrie() {
    // Lexicon constructor will be called automatically
}

void LexiconWithTrie::set_min_frequency(int freq) {
    lexicon_.set_min_frequency(freq);
}

void LexiconWithTrie::set_max_frequency_percentile(int percentile) {
    lexicon_.set_max_frequency_percentile(percentile);
}

void LexiconWithTrie::set_stopwords_path(const std::string& path) {
    lexicon_.set_stopwords_path(path);
}

bool LexiconWithTrie::build_from_jsonl(const std::string& cleaned_data_path, const std::string& output_path) {
    bool success = lexicon_.build_from_jsonl(cleaned_data_path, output_path);
    if (success) {
        rebuild_trie();
    }
    return success;
}

bool LexiconWithTrie::save_to_json(const std::string& output_path) const {
    return lexicon_.save_to_json(output_path);
}

bool LexiconWithTrie::load_from_json(const std::string& lexicon_path) {
    bool success = lexicon_.load_from_json(lexicon_path);
    if (success) {
        rebuild_trie();
    }
    return success;
}

int LexiconWithTrie::get_word_index(const std::string& word) const {
    return lexicon_.get_word_index(word);
}

std::string LexiconWithTrie::get_word(int index) const {
    return lexicon_.get_word(index);
}

size_t LexiconWithTrie::size() const {
    return lexicon_.size();
}

bool LexiconWithTrie::contains_word(const std::string& word) const {
    return lexicon_.contains_word(word);
}

std::vector<std::string> LexiconWithTrie::autocomplete(const std::string& prefix, int k) const {
    return trie_.autocomplete(prefix, k);
}

void LexiconWithTrie::rebuild_trie() {
    trie_.clear();
    
    // Insert all words from the lexicon into the trie
    size_t vocab_size = lexicon_.size();
    for (size_t i = 0; i < vocab_size; ++i) {
        std::string word = lexicon_.get_word(static_cast<int>(i));
        if (!word.empty()) {
            trie_.insert(word);
        }
    }
}






