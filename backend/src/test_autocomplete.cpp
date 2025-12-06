#include "LexiconWithTrie.hpp"
#include <iostream>
#include <string>

using namespace std;

int main(int argc, char* argv[]) {
    string lexicon_path = "data/processed/lexicon.json";
    
    if (argc >= 2) {
        lexicon_path = argv[1];
    }
    
    cout << "Loading lexicon from: " << lexicon_path << "\n";
    
    LexiconWithTrie lexicon_trie;
    if (!lexicon_trie.load_from_json(lexicon_path)) {
        cerr << "Error: Failed to load lexicon\n";
        return 1;
    }
    
    cout << "Lexicon loaded: " << lexicon_trie.size() << " words\n";
    cout << "Trie built successfully\n\n";
    
    cout << "=========================================\n";
    cout << "   AUTCOMPLETE TEST\n";
    cout << "=========================================\n\n";
    
    // Test autocomplete with various prefixes
    vector<string> test_prefixes = {"art", "comp", "data", "machine", "learn", "the"};
    
    for (const string& prefix : test_prefixes) {
        cout << "Prefix: \"" << prefix << "\"\n";
        vector<string> suggestions = lexicon_trie.autocomplete(prefix, 10);
        
        if (suggestions.empty()) {
            cout << "  No suggestions found\n";
        } else {
            cout << "  Suggestions (" << suggestions.size() << "):\n";
            for (size_t i = 0; i < suggestions.size(); ++i) {
                cout << "    " << (i + 1) << ". " << suggestions[i] << "\n";
            }
        }
        cout << "\n";
    }
    
    // Interactive mode
    cout << "Enter interactive mode (type 'quit' to exit):\n";
    string prefix;
    while (true) {
        cout << "> ";
        getline(cin, prefix);
        
        if (prefix == "quit" || prefix == "exit") {
            break;
        }
        
        if (prefix.empty()) {
            continue;
        }
        
        vector<string> suggestions = lexicon_trie.autocomplete(prefix, 10);
        if (suggestions.empty()) {
            cout << "  No suggestions found for \"" << prefix << "\"\n";
        } else {
            cout << "  Found " << suggestions.size() << " suggestions:\n";
            for (size_t i = 0; i < suggestions.size(); ++i) {
                cout << "    " << (i + 1) << ". " << suggestions[i] << "\n";
            }
        }
        cout << "\n";
    }
    
    return 0;
}

