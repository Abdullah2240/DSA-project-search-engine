#pragma once
// Trie.hpp
// Trie data structure for doing the  autocomplete functionality
// Stores words only at terminal nodes
// Supports lexicographically ordered autocomplete queries

#include <string>
#include <vector>
#include <map>
#include <memory>

class TrieNode {
public:
    std::map<char, std::unique_ptr<TrieNode>> children;
    bool is_end_of_word;
    std::string word;  // Store full word only at terminal nodes

    TrieNode() : is_end_of_word(false), word("") {}
};

class Trie {
public:
    //cnstructor and destructor
    Trie();
    ~Trie();

    // Insert a word into the trie
    void insert(const std::string& word);

    // Get autocomplete suggestions for a prefix
    // Returns up to k words in lexicographic order
    std::vector<std::string> autocomplete(const std::string& prefix, int k) const;

    // Check if the trie is empty
    bool empty() const;

    // Clear all nodes
    void clear();

private:
    std::unique_ptr<TrieNode> root_;

    // Helper function to collect all words from a subtree
    void collect_words(TrieNode* node, std::vector<std::string>& results, int max_count) const;
};

