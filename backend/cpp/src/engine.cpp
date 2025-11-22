#include "engine.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>

// global lexicon pointer so we can load lexicon only once
static Lexicon* g_lexicon = nullptr;

//we will  load lexicon file only when needed
void initialize_lexicon(const string& lexicon_path) {
    if (!g_lexicon) {
        g_lexicon = new Lexicon();

        // try loading lexicon.json
        if (!g_lexicon->load_from_json(lexicon_path)) {
            cerr << "Warning: Could not load lexicon from " << lexicon_path << endl;
        }
    }
}

// convert query text into word indices
// this is the first step before using forward || inverted index
vector<int> tokenize_query(const string& query, Lexicon& lexicon) {
    vector<int> word_indices;
    stringstream ss(query);
    string word;

    // break query text into words
    while (ss >> word) {
        // lowercase every word so matching becomes easy
        transform(word.begin(), word.end(), word.begin(),
                 [](unsigned char c){ return tolower(c); });

        // get id of word from lexicon
        int idx = lexicon.get_word_index(word);

        // only push valid words
        if (idx >= 0) {
            word_indices.push_back(idx);
        }
    }

    return word_indices;
}
// ====================NOTE=====================
// main search function (for now this is only lexicon-level search)
// later we will attach forward + inverted index logic here
vector<string> search(const string& query) {
    // load lexicon if not loaded already
    if (!g_lexicon) {
        initialize_lexicon("backend/data/processed/lexicon.json");
    }

    // check if lexicon exists or is empty
    if (!g_lexicon || g_lexicon->size() == 0) {
        return {"Lexicon not loaded. Please build lexicon first."};
    }

    // convert query into word indices
    vector<int> query_indices = tokenize_query(query, *g_lexicon);

    // if no matching word show msg below
    if (query_indices.empty()) {
        return {"No matching words found in lexicon. Try different keywords."};
    }

    vector<string> results;
    results.push_back("Query processed: " + to_string(query_indices.size()) + " words found in lexicon");
    results.push_back("Word indices:");

    // show first few matches to user
    for (size_t i = 0; i < query_indices.size() && i < 10; ++i) {
        string word = g_lexicon->get_word(query_indices[i]);
        results.push_back("  [" + to_string(query_indices[i]) + "] " + word);
    }

    return results;
}

// show lexicon stats to check everything is working
vector<string> get_lexicon_stats() {
    if (!g_lexicon) {
        initialize_lexicon("backend/data/processed/lexicon.json");
    }

    vector<string> stats;

    if (g_lexicon && g_lexicon->size() > 0) {
        stats.push_back("Lexicon loaded successfully");
        stats.push_back("Total words: " + to_string(g_lexicon->size()));
        stats.push_back("Sample words (first 20):");

        for (size_t i = 0; i < min((size_t)20, g_lexicon->size()); ++i) {
            string w = g_lexicon->get_word(static_cast<int>(i));
            stats.push_back("  [" + to_string(i) + "] " + w);
        }

    } else {
        stats.push_back("Lexicon not loaded");
    }

    return stats;
}
