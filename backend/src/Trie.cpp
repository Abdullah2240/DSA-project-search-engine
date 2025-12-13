#include "Trie.hpp"
#include <algorithm>
#include <cctype>
//after this i willl be done finally!
Trie::Trie() : root_(std::make_unique<TrieNode>()) {}

Trie::~Trie() {
    // Smart pointers will handle cleanup automatically
    clear();
}

void Trie::insert(const std::string& word) {
    if (word.empty()) return;

    TrieNode* current = root_.get();
    
    // Traverse/create path for each character
    for (char c : word) {
        // Convertingto lowercase for case insensitive matching
        char lower_c = std::tolower(static_cast<unsigned char>(c));
        
        if (current->children.find(lower_c) == current->children.end()) {
            current->children[lower_c] = std::make_unique<TrieNode>();
        }
        current = current->children[lower_c].get();
    }
    
    // Mark as end of word and store the full word at terminal node
    current->is_end_of_word = true;
    current->word = word;
}

std::vector<std::string> Trie::autocomplete(const std::string& prefix, int k) const {
    std::vector<std::string> results;
    if (k <= 0) return results;

    TrieNode* current = root_.get();
    
    // If prefix is empty, start from root (return first k words)
    if (!prefix.empty()) {
        // Navigate to the prefix node
        for (char c : prefix) {
            char lower_c = std::tolower(static_cast<unsigned char>(c));
            auto it = current->children.find(lower_c);
            if (it == current->children.end()) {
                // Prefix not found
                return results;
            }
            current = it->second.get();
        }
    }
    
    // Collect words from this subtree
    collect_words(current, results, k);
    
    return results;
}

bool Trie::empty() const {
    return root_->children.empty();
}

void Trie::clear() {
    root_ = std::make_unique<TrieNode>();
}

void Trie::collect_words(TrieNode* node, std::vector<std::string>& results, int max_count) const {
    if (results.size() >= static_cast<size_t>(max_count)) {
        return;
    }
    
    // If this is an end-of-word node, add the stored word
    if (node->is_end_of_word && !node->word.empty()) {
        results.push_back(node->word);
        if (results.size() >= static_cast<size_t>(max_count)) {
            return;
        }
    }
    
    // Traverse children in lexicographic order (map is already sorted)
    for (auto& pair : node->children) {
        collect_words(pair.second.get(), results, max_count);
        if (results.size() >= static_cast<size_t>(max_count)) {
            return;
        }
    }
}

